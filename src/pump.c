#include "pump.h"
#include "utils.h"

// M03 & M08: Start STOPPED, 0 RPM, and 30C temperature
void pump_init(PumpSimulator *pump) {
    pump->state = PUMP_STOPPED;
    pump->actual_rpm = 0;
    pump->requested_rpm = 1500;
    pump->tank_level = 10.0f;
    pump->water_flow = 0.0f;
    pump->discharge_pressure = 0.0f;
    pump->motor_temp = 30.0f;
    pump->fault_code = FAULT_NONE;
    pump->fault_injection = 0;
    log_event("INFO", "Pump initialized in STOPPED state");
}

// M11 & R07: Validate and set motor speed setpoint
void pump_set_speed(PumpSimulator *pump, int rpm, int max_rpm) {
    if (rpm < 0 || rpm > max_rpm) {
        log_event("WARN", "Rejected invalid RPM setpoint");
        return;
    }
    pump->requested_rpm = rpm;
}

// M09, M10, M19, M20: Handle Start, Stop, and Fault Reset commands
void pump_process_command(PumpSimulator *pump, int command) {
    if (command == CMD_NONE) return;

    if (command == CMD_START) {
        if (pump->state == PUMP_STOPPED) {
            pump->state = PUMP_RUNNING;
            log_event("INFO", "START command received: Pump is RUNNING");
        }
    } else if (command == CMD_STOP) {
        if (pump->state == PUMP_RUNNING) {
            pump->state = PUMP_STOPPED;
            log_event("INFO", "STOP command received: Pump is stopping");
        }
    } else if (command == CMD_RESET_FAULT) {
        // M19: Reset only if fault injection is 0 and temp is safe
        if (pump->state == PUMP_FAULT && pump->fault_injection == 0 && pump->motor_temp <= 80.0f) {
            pump->state = PUMP_STOPPED;
            pump->fault_code = FAULT_NONE;
            log_event("INFO", "RESET command received: Fault cleared, Pump is STOPPED");
        } else {
            log_event("WARN", "Cannot reset fault: Fault condition is still active");
        }
    } else {
        log_event("WARN", "Rejected unknown pump command");
    }
}

// Updates sensors once every second
void pump_step(PumpSimulator *pump) {
    // 1. Check fault injections and high temperature (M17, M18)
    if (pump->fault_injection == 1 && pump->state != PUMP_FAULT) {
        pump->state = PUMP_FAULT;
        pump->fault_code = FAULT_LOW_PRESSURE;
        log_event("ERROR", "FAULT DETECTED: Low discharge pressure");
    }
    if (pump->fault_injection == 2) {
        pump->motor_temp = 85.0f;
    }
    if (pump->motor_temp > 80.0f && pump->state != PUMP_FAULT) {
        pump->state = PUMP_FAULT;
        pump->fault_code = FAULT_HIGH_TEMP;
        log_event("ERROR", "FAULT DETECTED: Motor temperature exceeded 80C");
    }

    // 2. Gradual motor acceleration and deceleration (M10, M12)
    int target_rpm = (pump->state == PUMP_RUNNING) ? pump->requested_rpm : 0;
    int step_rpm = 500; 

    if (pump->actual_rpm < target_rpm) {
        pump->actual_rpm += step_rpm;
        if (pump->actual_rpm > target_rpm) pump->actual_rpm = target_rpm;
    } else if (pump->actual_rpm > target_rpm) {
        pump->actual_rpm -= step_rpm;
        if (pump->actual_rpm < target_rpm) pump->actual_rpm = target_rpm;
    }

    // 3. Calculate water flow (0-60 m3/h) and pressure (0-8 bar) (M13, M14)
    pump->water_flow = (pump->actual_rpm / 3000.0f) * 60.0f;
    if (pump->fault_code == FAULT_LOW_PRESSURE) {
        pump->discharge_pressure = 0.5f;
    } else {
        pump->discharge_pressure = (pump->actual_rpm / 3000.0f) * 8.0f;
    }

    // 4. Fill tank when water is flowing (M15)
    if (pump->water_flow > 0.0f) {
        pump->tank_level += (pump->water_flow / 36.0f);
        if (pump->tank_level > 100.0f) pump->tank_level = 100.0f;
    }

    // 5. Motor heating while running and cooling when stopped (M16)
    if (pump->state == PUMP_RUNNING && pump->actual_rpm > 0) {
        pump->motor_temp += 0.5f;
    } else if (pump->fault_injection != 2 && pump->motor_temp > 30.0f) {
        pump->motor_temp -= 1.0f;
        if (pump->motor_temp < 30.0f) pump->motor_temp = 30.0f;
    }
}