#ifndef NIXEL_H
#define NIXEL_H

#define TIMEOUT 3

typedef enum {
    CHECK_UP, CHECK_DOWN_REFUSED, CHECK_DOWN_TIMEOUT, CHECK_ERROR
} check_status_t;

typedef struct {
    const char *host;
    const char *port;
    check_status_t status;
    double latency_ms;
} check_result_t;

#endif
