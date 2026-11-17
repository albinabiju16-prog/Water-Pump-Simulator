
#ifndef UTILS_H
#define UTILS_H

// Holds values from config.json
typedef struct {
    char ip[32];
    int port;
    int unit_id;
    int update_interval_sec;
    int max_rpm;
} AppConfig;

// Functions to load config and write logs
int load_config(const char *filename, AppConfig *config);
void log_event(const char *level, const char *message);

#endif