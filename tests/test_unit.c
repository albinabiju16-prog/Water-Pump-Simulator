#include <stdio.h>
#include <assert.h>
#include "pump.h"
#include "utils.h"

int main() {
    PumpSimulator pump;
    AppConfig cfg;

    // 1-5: Config loading (Positive & Negative)
    assert(load_config("config.json", &cfg) == 0);
    assert(cfg.port == 5020);
    assert(cfg.unit_id == 1);
    assert(cfg.max_rpm == 3000);
    assert(load_config("nonexistent.json", &cfg) == -1);
    printf("PASS [1-5]: Config file validation\n");

    // 6-10: Initial pump state
    pump_init(&pump);
    assert(pump.state == PUMP_STOPPED);
    assert(pump.actual_rpm == 0);
    assert(pump.water_flow == 0.0f);
    assert(pump.discharge_pressure == 0.0f);
    assert(pump.motor_temp == 30.0f);
    printf("PASS [6-10]: Initial state verification\n");

    // 11-15: Speed setpoint boundaries & negative checks
    pump_set_speed(&pump, 0, 3000);
    assert(pump.requested_rpm == 0);
    pump_set_speed(&pump, 3000, 3000);
    assert(pump.requested_rpm == 3000);
    pump_set_speed(&pump, -1, 3000);
    assert(pump.requested_rpm == 3000);
    pump_set_speed(&pump, 3001, 3000);
    assert(pump.requested_rpm == 3000);
    pump_set_speed(&pump, 1500, 3000);
    assert(pump.requested_rpm == 1500);
    printf("PASS [11-15]: Speed setpoint boundary and invalid checks\n");

    // 16-20: Start command, gradual acceleration, flow, tank fill, heating
    pump_process_command(&pump, CMD_NONE);
    assert(pump.state == PUMP_STOPPED);
    pump_process_command(&pump, CMD_START);
    assert(pump.state == PUMP_RUNNING);
    pump_step(&pump);
    assert(pump.actual_rpm == 500);
    assert(pump.water_flow > 9.9f && pump.water_flow < 10.1f);
    assert(pump.motor_temp == 30.5f);
    printf("PASS [16-20]: Start command, ramp-up, flow, and heating\n");

    // 21-24: Target RPM ceiling & tank 100% cap
    pump_step(&pump); // 1000 RPM
    pump_step(&pump); // 1500 RPM
    pump_step(&pump); // stays 1500 RPM
    assert(pump.actual_rpm == 1500);
    pump.tank_level = 99.9f;
    pump_step(&pump);
    assert(pump.tank_level == 100.0f);
    pump_process_command(&pump, 99); // unknown command
    assert(pump.state == PUMP_RUNNING);
    printf("PASS [21-24]: RPM ceiling, tank 100%% cap, unknown command\n");

    // 25-27: Stop command, gradual deceleration, and cooling floor
    pump_process_command(&pump, CMD_STOP);
    assert(pump.state == PUMP_STOPPED);
    pump_step(&pump); // 1000 RPM
    pump_step(&pump); // 500 RPM
    pump_step(&pump); // 0 RPM
    pump_step(&pump); // stays 0 RPM
    assert(pump.actual_rpm == 0);
    pump.motor_temp = 30.2f;
    pump_step(&pump);
    assert(pump.motor_temp == 30.0f);
    printf("PASS [25-27]: Stop deceleration and 30C cooling floor\n");

    // 28-32: Low-pressure & High-temp faults, blocked start/reset, valid reset
    pump.fault_injection = 1;
    pump_step(&pump);
    assert(pump.state == PUMP_FAULT && pump.fault_code == FAULT_LOW_PRESSURE);
    pump_process_command(&pump, CMD_START); // cannot start in fault
    assert(pump.state == PUMP_FAULT);
    pump_process_command(&pump, CMD_RESET_FAULT); // blocked while injected
    assert(pump.state == PUMP_FAULT);
    pump.fault_injection = 0;
    pump_process_command(&pump, CMD_RESET_FAULT);
    assert(pump.state == PUMP_STOPPED && pump.fault_code == FAULT_NONE);

    pump.fault_injection = 2; // High temp (85C)
    pump_step(&pump);
    pump.fault_injection = 0;
    for (int i = 0; i < 5; i++) pump_step(&pump); // cool to 80.0C
    pump_process_command(&pump, CMD_RESET_FAULT);
    assert(pump.state == PUMP_STOPPED);
    printf("PASS [28-32]: Fault detection, safety interlocks, and reset\n");

    // 33-35: Tank drain command and auto-close when empty
    pump.tank_level = 10.0f;
    pump_process_command(&pump, CMD_DRAIN_TANK);
    assert(pump.drain_active == 1);
    pump_step(&pump); // 10.0 -> 5.0%
    assert(pump.tank_level == 5.0f);
    pump_step(&pump); // 5.0 -> 0.0% and auto-closes
    assert(pump.tank_level == 0.0f && pump.drain_active == 0);
    printf("PASS [33-35]: Tank drain command and auto-close when empty\n");

    printf("\nALL 35 UNIT TESTS PASSED!\n");
    return 0;
}
