#ifndef SERVER_H
#define SERVER_H

#include <sqlite3.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_MSG (64 * 1024)   // reject anything larger 

typedef struct {
    char   host[256];
    char   port[16];
    char   status[32];
    double latency_ms;
} parsed_result_t;

// with non-blocking reads, one read() can deliver half od length prefix or one-and-a-half frames
// and recv_all waits for the exact bytes, which stalls other agents
// so a struct is needed so each connection its own positiion in the frame
typedef struct {
    int      fd;               // -1 means this slot is free

    uint8_t  len_buf[4];       // the 4-byte length prefix as it arrives
    size_t   len_have;         // how many of those 4 bytes we have so far (0..4)
    int      need_len;         // 1 = still reading prefix, 0 = reading payload

    char    *payload;          // heap buffer, size payload_len + 1
    uint32_t payload_len;      // expected payload size from the prefix
    size_t   payload_have;     // payload bytes received so far
} conn_t;

void conn_reset(conn_t *c);                                        // free payload, reset to "reading prefix"
int  conn_feed(conn_t *c, const uint8_t *data, size_t n, sqlite3 *db);  // feed bytes, emit complete frames

int parse_result(const char *json_str, parsed_result_t *out);
int  store_open(const char *path, sqlite3 **db);
int  store_insert(sqlite3 *db, const parsed_result_t *pr);
void store_close(sqlite3 *db);

#endif

