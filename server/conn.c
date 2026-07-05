#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"

void conn_reset(conn_t *c) {
    free(c->payload);          // give back the heap buffer (NULL-safe: free(NULL) is fine)
    c->payload      = NULL;
    c->payload_len  = 0;
    c->payload_have = 0;
    c->len_have     = 0;       // zero of the 4 length bytes received
    c->need_len     = 1;       // 1 = "next bytes belong to the length prefix"
}


int conn_feed(conn_t *c, const uint8_t *data, size_t n, sqlite3 *db) {
    size_t off = 0;                     // how far into `data` we've consumed

    while (off < n) {                   // keep going while unconsumed bytes remain
        if (c->need_len) {
            // --- still assembling the 4-byte length prefix ---
            while (c->len_have < 4 && off < n)
                c->len_buf[c->len_have++] = data[off++];

            if (c->len_have < 4) return 0;   // prefix incomplete -> wait for more

            uint32_t len = ((uint32_t)c->len_buf[0] << 24) |
                           ((uint32_t)c->len_buf[1] << 16) |
                           ((uint32_t)c->len_buf[2] <<  8) |
                           ((uint32_t)c->len_buf[3]);

            if (len == 0 || len > MAX_MSG) return -1;   // protocol violation -> drop conn

            c->payload = calloc(len + 1, 1);
            if (!c->payload) return -1;
            c->payload_len  = len;
            c->payload_have = 0;
            c->need_len     = 0;             // now switch to reading the body
        } else {
            // --- assembling the payload ---
            size_t want = c->payload_len - c->payload_have;   // bytes still needed
            size_t avail = n - off;                            // bytes we have on hand
            size_t take = want < avail ? want : avail;         // copy the smaller

            memcpy(c->payload + c->payload_have, data + off, take);
            c->payload_have += take;
            off             += take;

            if (c->payload_have < c->payload_len) return 0;    // body incomplete -> wait

            // one whole frame is here: parse + store (your existing functions)
            parsed_result_t pr;
            if (parse_result(c->payload, &pr) == 0) {
                if (store_insert(db, &pr) != 0)
                    fprintf(stderr, "insert failed\n");
            } else {
                fprintf(stderr, "bad message, ignored\n");
            }

            conn_reset(c);                   // ready for the next frame on this stream
        }
    }
    return 0;
}
