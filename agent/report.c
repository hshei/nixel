#include <stdio.h>   
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "nixel.h"

// Escapes `src` into `dst` per JSON string rules (RFC 8259).
// Writes at most dstlen bytes including the NUL terminator.
// Returns the number of chars written (excluding NUL), or -1 if it wouldn't fit.
static int json_escape(char *dst, size_t dstlen, const char *src) {
    size_t w = 0;  // bytes written so far

    // helper: append one raw byte, guarding against overflow
    #define PUT(ch)                        \
        do {                               \
            if (w + 1 >= dstlen) return -1; \
            dst[w++] = (ch);               \
        } while (0)

    for (const unsigned char *p = (const unsigned char *)src; *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"':  PUT('\\'); PUT('"');  break;
            case '\\': PUT('\\'); PUT('\\'); break;
            case '\b': PUT('\\'); PUT('b');  break;
            case '\f': PUT('\\'); PUT('f');  break;
            case '\n': PUT('\\'); PUT('n');  break;
            case '\r': PUT('\\'); PUT('r');  break;
            case '\t': PUT('\\'); PUT('t');  break;
            default:
                if (c < 0x20) {
                    // other control chars → \u00XX
                    // need 6 bytes: \ u 0 0 X X
                    if (w + 6 >= dstlen) return -1;
                    int n = snprintf(dst + w, dstlen - w, "\\u%04x", c);
                    if (n < 0) return -1;
                    w += (size_t)n;
                } else {
                    PUT((char)c);   // printable byte, copy as-is
                }
        }
    }

    if (w + 1 > dstlen) return -1;
    dst[w] = '\0';
    #undef PUT
    return (int)w;
}

static int send_all(int fd, void *buf, size_t len) {    
    size_t total = 0;
    char *p = buf;
    while (total < len) {
        ssize_t n = send(fd, p + total, len - total, 0);
        if (n <= 0) return -1;
        total += n;
    }
    return 0;
}

char *status_to_str(check_status_t s) {
    switch (s) {
        case CHECK_UNKNOWN:      return "unknown";
        case CHECK_UP:           return "up";
        case CHECK_DOWN_REFUSED: return "down_refused";
        case CHECK_DOWN_TIMEOUT: return "down_timeout";
        case CHECK_ERROR:        return "error";
    }
    return "unknown";
}

int build_result_json(const check_result_t *r, char *json_out, size_t out_size){
    char host_esc[256];
    char port_esc[64];
    if (json_escape(host_esc, sizeof(host_esc), r->host) < 0) return -1;  // dst, size, src
    if (json_escape(port_esc, sizeof(port_esc), r->port) < 0) return -1;
    
    int n = snprintf(json_out, out_size,
        "{\"host\":\"%s\",\"port\":\"%s\",\"status\":\"%s\",\"latency_ms\":%.2f}",
        host_esc, port_esc, status_to_str(r->status), r->latency_ms);

    // snprintf returns the length it WANTED to write. If that's >= out_size,
    // the output was truncated — treat as failure.
    if (n < 0 || (size_t)n >= out_size) return -1;

    return n;  // number of bytes written (excluding NUL)
}

int connect_to_server(const char *server, const char *port){
     // connecting to server
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;    /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;  /* TCP */

    if (getaddrinfo(server, port, &hints, &res) != 0) return -1;

    int fd = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1) continue;
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;  // success
        close(fd);
        fd = -1;
    }    

    freeaddrinfo(res);
    if (fd == -1) return -1;   

    return fd;
}

int send_result(int server_fd, const check_result_t *r){
    // building the json result
    char json[2048];
    int json_len = build_result_json(r, json, sizeof(json));
    
    if (json_len < 0) return -1;

    // frame: 4-byte length prefix
    uint32_t net_len = htonl(json_len);
    
    // sending the length bytes, then the payload
    if (send_all(server_fd, &net_len, sizeof(net_len)) != 0) return -1; 
    if (send_all(server_fd, json, (size_t)json_len)   != 0) return -1;

    return 0;
}