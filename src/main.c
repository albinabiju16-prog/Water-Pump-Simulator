#include "utils.h"
#include "pump.h"
#include "modbus_server.h"

int main() {
    AppConfig config;
    if (load_config("config.json", &config) != 0) {
        log_event("ERROR", "Failed to load config.json");
        return 1;
    }

    PumpSimulator pump;
    pump_init(&pump);

    return run_modbus_server(&config, &pump);
}