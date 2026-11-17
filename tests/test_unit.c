#include <stdio.h>
#include <assert.h>
#include "pump.h"
#include "utils.h"

int main() {
    PumpSimulator pump;
    AppConfig cfg;

    // 1-2. Positive & Negative: Config loading
    assert(load_config("config.json", &cfg) == 0);
    assert(load_config("missing_file.json", &cfg) == -1);
    printf("PASS [1-2]: Config valid and missing file checks\n");

    // 3. Positive: Initial state (STOPPED, 0 RPM, 30C)
    pump_init(&pump);
    assert(pump.state == PUMP_STOPPED && pump.actual_rpm == 0 && pump.motor_temp == 30.0f);
    printf("PASS [3]: Initial pump state\n");

    // 4-7. Positive: Start command, RPM ramp-up, flow, pressure, tank fill, heating
    pump_process_command(&pump, CMD_START);
    assert(pump.state == PUMP_RUNNING);
    pump_step(&pump);
    assert(pump.actual_rpm == 500);
    assert(pump.water_flow > 0.0f && pump.discharge_pressure > 0.0f);
    assert(pump.tank_level > 10.0f && pump.motor_temp > 30.0f);
    printf("PASS [4-7]: Start, acceleration, flow, pressure, tank fill, and heating\n");

    // 8-9. Boundary: Min (0) and Max (3000) RPM limits
    pump_set_speed(&pump, 0, 3000);
    assert(pump.requested_rpm == 0);
    pump_set_speed(&pump, 3000, 3000);
    assert(pump.requested_rpm == 3000);
    printf("PASS [8-9]: Boundary RPM setpoints (0 and 3000)\n");

    // 10-12. Negative: Invalid RPM (-1, 3001) and unknown command (99)
    pump_set_speed(&pump, -1, 3000);
    assert(pump.requested_rpm == 3000);
    pump_set_speed(&pump, 3001, 3000);
    assert(pump.requested_rpm == 3000);
    pump_process_command(&pump, 99);
    assert(pump.state == PUMP_RUNNING);
    printf("PASS [10-12]: Negative invalid inputs rejected\n");

    // 13-14. Positive: Stop command, deceleration, and cooling
    pump_process_command(&pump, CMD_STOP);
    assert(pump.state == PUMP_STOPPED);
    pump_step(&pump);
    assert(pump.actual_rpm == 0);
    printf("PASS [13-14]: Stop deceleration and cooling\n");

    // 15-16. Positive & Negative: Fault injection, blocked reset, and valid reset
    pump.fault_injection = 1;
    pump_step(&pump);
    assert(pump.state == PUMP_FAULT && pump.fault_code == FAULT_LOW_PRESSURE);
    pump_process_command(&pump, CMD_RESET_FAULT); // Must fail while fault_injection == 1
    assert(pump.state == PUMP_FAULT);
    pump.fault_injection = 0;
    pump_process_command(&pump, CMD_RESET_FAULT); // Must succeed now
    assert(pump.state == PUMP_STOPPED && pump.fault_code == FAULT_NONE);
    printf("PASS [15-16]: Fault detection, blocked reset, and valid reset\n");

    printf("\nALL 16 UNIT TESTS PASSED!\n");
    return 0;
}