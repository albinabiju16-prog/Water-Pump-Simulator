
#include <stdio.h>
#include <assert.h>
#include "pump.h"

int main() {
    PumpSimulator pump;

    // Test 1: Initial state (M03, M08)
    pump_init(&pump);
    assert(pump.state == PUMP_STOPPED);
    assert(pump.actual_rpm == 0);
    assert(pump.motor_temp == 30.0f);
    printf("PASS: Test 1 - Pump Initialization\n");

    // Test 2: Start command and physics (M09, M12, M13, M14)
    pump_process_command(&pump, CMD_START);
    assert(pump.state == PUMP_RUNNING);
    pump_step(&pump);
    assert(pump.actual_rpm == 500);
    assert(pump.water_flow > 0.0f);
    assert(pump.discharge_pressure > 0.0f);
    printf("PASS: Test 2 - Start Command and Physics\n");

    // Test 3: Speed limit check (M11, R07)
    pump_set_speed(&pump, 2000, 3000);
    assert(pump.requested_rpm == 2000);
    pump_set_speed(&pump, 5000, 3000); // 5000 is invalid (>3000)
    assert(pump.requested_rpm == 2000); // Must stay 2000
    printf("PASS: Test 3 - Speed Setpoint Validation\n");

    // Test 4: High-temp fault (>80C) and reset (M18, M19)
    pump.fault_injection = 2;
    pump_step(&pump);
    assert(pump.state == PUMP_FAULT);
    assert(pump.fault_code == FAULT_HIGH_TEMP);

    pump.fault_injection = 0;
    pump.motor_temp = 50.0f; // Safe temperature
    pump_process_command(&pump, CMD_RESET_FAULT);
    assert(pump.state == PUMP_STOPPED);
    assert(pump.fault_code == FAULT_NONE);
    printf("PASS: Test 4 - Fault Detection and Reset\n");

    printf("\nALL UNIT TESTS PASSED!\n");
    return 0;
}