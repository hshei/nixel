#ifndef SERVER_H
#define SERVER_H

#include <sqlite3.h>

typedef struct {
    char   host[256];
    char   port[16];
    char   status[32];
    double latency_ms;
} parsed_result_t;

int parse_result(const char *json_str, parsed_result_t *out);
int  store_open(const char *path, sqlite3 **db);
int  store_insert(sqlite3 *db, const parsed_result_t *pr);
void store_close(sqlite3 *db);

#endif

