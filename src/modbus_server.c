
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <modbus/modbus.h>
#include "modbus_server.h"

static volatile int keep_running = 1;

void handle_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

// Copy pump values into Modbus registers (Section 1.3 scaling)
static void update_registers(const PumpSimulator *pump, modbus_mapping_t *map) {
    // Input Registers (Read-Only, 0 to 5)
    map->tab_input_registers[0] = (uint16_t)(pump->tank_level * 10.0f);         // Tank level x 10
    map->tab_input_registers[1] = (uint16_t)(pump->water_flow * 10.0f);         // Flow x 10
    map->tab_input_registers[2] = (uint16_t)(pump->discharge_pressure * 100.0f);// Pressure x 100
    map->tab_input_registers[3] = (uint16_t)(pump->actual_rpm);                 // Actual RPM
    map->tab_input_registers[4] = (uint16_t)(pump->motor_temp * 10.0f);         // Temp x 10
    map->tab_input_registers[5] = (uint16_t)(pump->fault_code);                 // Fault code

    // Discrete Inputs (Read-Only, 0 to 1)
    map->tab_input_bits[0] = (pump->state == PUMP_RUNNING) ? 1 : 0;             // Running status
    map->tab_input_bits[1] = (pump->state == PUMP_FAULT) ? 1 : 0;               // Fault status
}

// Read commands written by Node-RED into Holding Registers (0 to 2)
static void apply_holding_registers(PumpSimulator *pump, const AppConfig *config, modbus_mapping_t *map) {
    // Holding Register 1: Requested motor speed in RPM
    int req_rpm = (int)map->tab_registers[1];
    if (req_rpm != pump->requested_rpm) {
        pump_set_speed(pump, req_rpm, config->max_rpm);
        map->tab_registers[1] = (uint16_t)pump->requested_rpm;
    }

    // Holding Register 2: Fault injection (0=None, 1=Low Press, 2=High Temp)
    int fault_inj = (int)map->tab_registers[2];
    if (fault_inj >= 0 && fault_inj <= 2) {
        pump->fault_injection = fault_inj;
    } else {
        log_event("WARN", "Rejected invalid fault injection value");
        map->tab_registers[2] = (uint16_t)pump->fault_injection;
    }

    // Holding Register 0: Pump Command (execute once, then reset to 0)
    int cmd = (int)map->tab_registers[0];
    if (cmd != 0) {
        pump_process_command(pump, cmd);
        map->tab_registers[0] = 0; // Reset to 0 after processing
    }
}

int run_modbus_server(AppConfig *config, PumpSimulator *pump) {
    signal(SIGINT, handle_sigint);

    modbus_t *ctx = modbus_new_tcp(config->ip, config->port);
    if (!ctx) {
        log_event("ERROR", "Failed to create Modbus TCP context");
        return -1;
    }
    modbus_set_slave(ctx, config->unit_id);

    // Allocate: 0 Coils, 2 Discrete Inputs, 3 Holding Registers, 6 Input Registers
    modbus_mapping_t *map = modbus_mapping_new(0, 2, 3, 6);
    if (!map) {
        log_event("ERROR", "Failed to allocate Modbus registers");
        modbus_free(ctx);
        return -1;
    }

    // Initialize Holding Registers
    map->tab_registers[0] = 0;
    map->tab_registers[1] = (uint16_t)pump->requested_rpm;
    map->tab_registers[2] = 0;
    update_registers(pump, map);

    int server_socket = modbus_tcp_listen(ctx, 5);
    if (server_socket == -1) {
        log_event("ERROR", "Cannot listen on port 5020");
        modbus_mapping_free(map);
        modbus_free(ctx);
        return -1;
    }

    log_event("INFO", "Modbus TCP Server listening on port 5020 (Unit ID 1)");

    fd_set refset, rdset;
    FD_ZERO(&refset);
    FD_SET(server_socket, &refset);
    int fdmax = server_socket;
    time_t last_update = time(NULL);

    while (keep_running) {
        rdset = refset;
        struct timeval tv = {0, 100000}; // Check every 100ms

        if (select(fdmax + 1, &rdset, NULL, NULL, &tv) == -1) {
            if (errno == EINTR) continue;
            break;
        }

        // Handle incoming Modbus TCP connections and requests
        for (int s = 0; s <= fdmax; s++) {
            if (!FD_ISSET(s, &rdset)) continue;

            if (s == server_socket) {
                struct sockaddr_in client;
                socklen_t addrlen = sizeof(client);
                int newfd = accept(server_socket, (struct sockaddr *)&client, &addrlen);
                if (newfd != -1) {
                    FD_SET(newfd, &refset);
                    if (newfd > fdmax) fdmax = newfd;
                    log_event("INFO", "Node-RED / Modbus client connected");
                }
            } else {
                modbus_set_socket(ctx, s);
                uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
                int rc = modbus_receive(ctx, query);
                if (rc > 0) {
                    modbus_reply(ctx, query, rc, map);
                    apply_holding_registers(pump, config, map);
                    update_registers(pump, map);
                } else if (rc == -1) {
                    close(s);
                    FD_CLR(s, &refset);
                    log_event("WARN", "Modbus client disconnected");
                }
            }
        }

        // Update pump physics once every second (M01-M20)
        time_t now = time(NULL);
        if (now - last_update >= config->update_interval_sec) {
            pump_step(pump);
            update_registers(pump, map);
            last_update = now;
        }
    }

    log_event("INFO", "Shutting down Modbus TCP server gracefully");
    close(server_socket);
    modbus_mapping_free(map);
    modbus_free(ctx);
    return 0;
}