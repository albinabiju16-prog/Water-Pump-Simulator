
#ifndef PUMP_H
#define PUMP_H

// Pump states (M03, M09, M17, M18)
typedef enum {
    PUMP_STOPPED = 0,
    PUMP_RUNNING = 1,
    PUMP_FAULT   = 2
} PumpState;

// Commands from Holding Register 0
typedef enum {
    CMD_NONE        = 0,
    CMD_START       = 1,
    CMD_STOP        = 2,
    CMD_RESET_FAULT = 3
} PumpCommand;

// Fault codes for Input Register 5
typedef enum {
    FAULT_NONE         = 0,
    FAULT_LOW_PRESSURE = 1,
    FAULT_HIGH_TEMP    = 2
} FaultCode;

// Structure holding all live machine values
typedef struct {
    PumpState state;            //current FSM state (stopped,running,fault)
    int actual_rpm;             // 0 to 3000 RPM (M07)
    int requested_rpm;          // 0 to 3000 RPM (M11)
    float tank_level;           // 0 to 100 % (M04)
    float water_flow;           // 0 to 60 m3/h (M05)
    float discharge_pressure;   // 0 to 8 bar (M06)
    float motor_temp;           // Starts at 30 C (M08)
    FaultCode fault_code;       // 0, 1, or 2
    int fault_injection;        // 0 = None, 1 = Low pressure, 2 = High temp
} PumpSimulator;

void pump_init(PumpSimulator *pump);
void pump_set_speed(PumpSimulator *pump, int rpm, int max_rpm);
void pump_process_command(PumpSimulator *pump, int command);
void pump_step(PumpSimulator *pump);

#endif