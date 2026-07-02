#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sqlite3.h>

#include "server.h"

#define PORT 9000
#define MAX_MSG (64 * 1024)   // reject anything larger 

static int recv_all(int fd, void *buf, size_t len) {
    size_t total = 0;
    char *p = buf;
    while (total < len) {
        ssize_t n = recv(fd, p + total, len - total, 0);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return 0;
}

int main(void) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }
    
    int yes = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   
    addr.sin_port        = htons(PORT);
    
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    if (listen(listen_fd, 16) < 0) { perror("listen"); return 1; }
    
    printf("nixel-server listening on port %d\n", PORT);
    
    sqlite3 *db;
    // opening the database
    if (store_open("nixel.db", &db) != 0){
        fprintf(stderr, "fatal: could not open database\n");
        exit(1);
    }
    
    for (;;) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd < 0) { perror("accept"); continue; }
        
        // recv 4 bytes length
        uint32_t net_length;
        if (recv_all(client_fd, &net_length, sizeof(net_length)) != 0) {
            close(client_fd); continue;
        }
        // ntohl
        uint32_t length = ntohl(net_length);

        // Bound-check
        if (length == 0 || length > MAX_MSG) {
            fprintf(stderr, "rejected bad length: %u\n", length);           
            close(client_fd);
            continue;
        }

        // recv_all payload
        char *payload = calloc(length + 1, 1);
        if (payload == NULL) { close(client_fd); continue; }
        if (recv_all(client_fd, payload, length) != 0) {
           free(payload); close(client_fd); continue;
        }

        // parsing the payload
        parsed_result_t pr;
        if (parse_result(payload, &pr) == 0) {
            printf("parsed: host=%s port=%s status=%s latency=%.2f ms\n",
                pr.host, pr.port, pr.status, pr.latency_ms);
            if (store_insert(db, &pr) != 0)
                fprintf(stderr, "insert failed\n");
        } else {
            fprintf(stderr, "bad message, ignored\n");
        }
        free(payload);
        
        // shutdown
        close(client_fd);
    }

    store_close(db);

    close(listen_fd);
    return 0;
}
