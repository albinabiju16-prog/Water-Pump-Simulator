
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "utils.h"

int load_config(const char *filename, AppConfig *config) {
    FILE *file = fopen(filename, "r");
    if (!file) return -1;

    char line[128];
    while (fgets(line, sizeof(line), file)) {
        sscanf(line, " \"ip\": \"%31[^\"]\"", config->ip);
        sscanf(line, " \"port\": %d", &config->port);
        sscanf(line, " \"unit_id\": %d", &config->unit_id);
        sscanf(line, " \"update_interval_sec\": %d", &config->update_interval_sec);
        sscanf(line, " \"max_rpm\": %d", &config->max_rpm);
    }
    fclose(file);
    return 0;
}

void log_event(const char *level, const char *message) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    // 1. Print to terminal screen
    printf("[%s] [%s] %s\n", time_str, level, message);

    // 2. Save to pump.log file (Requirement M20 & R04)
    FILE *log_file = fopen("pump.log", "a");
    if (log_file) {  //checks that linux successfully opened the file
        fprintf(log_file, "[%s] [%s] %s\n", time_str, level, message);
        fclose(log_file);
    }
}