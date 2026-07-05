#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <netinet/in.h>
#include <sys/fcntl.h>
#include <sqlite3.h>

#include "server.h"

#define PORT 9000
#define MAX_CONNS 64

static volatile sig_atomic_t running = 1;   

static void handle_signal(int sig) {
    (void)sig;
    running = 0;             
}

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(void) {
    // same as agent/main.c
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sa.sa_flags   = 0;              /* NO SA_RESTART -> poll() returns EINTR on signal */
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    // never die on a dead socket
    signal(SIGPIPE, SIG_IGN);

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
    set_nonblocking(listen_fd);    
    printf("nixel-server listening on port %d\n", PORT);
    
    // Polling different agents
    struct pollfd fds[MAX_CONNS];
    conn_t conns[MAX_CONNS];

    for (int i = 0; i < MAX_CONNS; i++) {
        fds[i].fd = -1;                    /* -1 tells poll() to skip this slot */
        fds[i].events = 0;
        conns[i].fd = -1;
        memset(&conns[i], 0, sizeof(conns[i]));   // payload = NULL, all counters 0
    }
    fds[0].fd     = listen_fd;
    fds[0].events = POLLIN;    

    // opening the database
    sqlite3 *db;
    if (store_open("nixel.db", &db) != 0){
        fprintf(stderr, "fatal: could not open database\n");
        exit(1);
    }
    
    while (running) {
        int ready = poll(fds, MAX_CONNS, -1);   /* -1 = block until something happens */        
        if (ready < 0) {
            if (errno == EINTR) continue;        /* a signal woke us -> re-check running */
            perror("poll");
            break;
        }
         /* --- (A) listener slot: a new agent is knocking --- */
        if (fds[0].revents & POLLIN) {
            for (;;) {                            /* drain ALL pending connections */
                int client_fd = accept(listen_fd, NULL, NULL);
                if (client_fd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) break;  /* no more waiting */
                    if (errno == EINTR) continue;
                    perror("accept");
                    break;
                }

                /* find a free slot (skip 0, that's the listener) */
                int slot = -1;
                for (int i = 1; i < MAX_CONNS; i++) {
                    if (fds[i].fd == -1) { slot = i; break; }
                }
                if (slot == -1) {                 /* table full -> refuse politely */
                    close(client_fd);
                    continue;
                }

                set_nonblocking(client_fd);
                fds[slot].fd      = client_fd;
                fds[slot].events  = POLLIN;
                conns[slot].fd    = client_fd;
                conn_reset(&conns[slot]);         /* arm the decoder for this agent */
            }
        }

        /* --- (B) agent slots: bytes arrived, or the peer hung up --- */
        for (int i = 1; i < MAX_CONNS; i++) {
            if (fds[i].fd == -1) continue;        /* empty slot */
            if (fds[i].revents == 0) continue;    /* nothing happened on this one */
            int drop = 0;

            if (fds[i].revents & POLLIN) {
                uint8_t buf[4096];
                for (;;) {                        /* drain everything available now */
                    ssize_t n = read(fds[i].fd, buf, sizeof(buf));
                    if (n > 0) {
                        if (conn_feed(&conns[i], buf, (size_t)n, db) != 0) {
                            drop = 1;             /* protocol violation -> drop */
                            break;
                        }
                        continue;                 /* maybe more buffered; read again */
                    }
                    if (n == 0) { drop = 1; break; }
                    if (errno == EAGAIN || errno == EWOULDBLOCK) break;  /* drained */
                    if (errno == EINTR) continue;
                    drop = 1; break;              /* real read error */
                }
            }

            /* POLLERR/POLLHUP/POLLNVAL also mean "this socket is done" */
            if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
                drop = 1;

            if (drop) {
                close(fds[i].fd);
                fds[i].fd   = -1;                 /* free the slot for reuse */
                conn_reset(&conns[i]);            /* frees any half-built payload */
                conns[i].fd = -1;
            }
        }
    }

    /* graceful shutdown: close every live agent, then the listener */
    for (int i = 1; i < MAX_CONNS; i++) {
        if (fds[i].fd != -1) {
            close(fds[i].fd);
            conn_reset(&conns[i]);                /* free buffers -> zero leaks */
        }
    }

    return 0;
}
