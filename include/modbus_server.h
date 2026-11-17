
#ifndef MODBUS_SERVER_H
#define MODBUS_SERVER_H

#include "utils.h"
#include "pump.h"

int run_modbus_server(AppConfig *config, PumpSimulator *pump);

#endif