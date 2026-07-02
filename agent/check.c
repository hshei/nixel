#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>

#include "nixel.h"


void check_client(check_result_t *r, int timeout_sec){
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;    /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;  /* TCP */
    
    if (getaddrinfo(r->host, r->port, &hints, &res) != 0) { r->status = CHECK_ERROR; return;}
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    check_status_t status = CHECK_ERROR;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        int fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1)
            continue;
            
        // switch to non-blocking to connect() returns immediately
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        int rc = connect(fd, rp->ai_addr, rp->ai_addrlen);
        if (rc == 0) {
            status = CHECK_UP;              /* status instantly */
            close(fd);
            break;
        }

        if (errno != EINPROGRESS) {     /* real error, try next address */
            close(fd);
            // Whenever refused, just stop and don't check other results
            if (errno == ECONNREFUSED) {
                status = CHECK_DOWN_REFUSED;
                break;
            }
            else {
                status = CHECK_ERROR;
                continue;
            }
        }

        // wait until writeable, or until timeout expires
        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(fd, &wset);
        struct timeval tv = { .tv_sec = timeout_sec, .tv_usec = 0 };

        int select_rc = select(fd + 1, NULL, &wset, NULL, &tv);
        
        if ( select_rc > 0) {
            int err = 0;
            socklen_t len = sizeof(err);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
            if (err == 0) status = CHECK_UP;          // connection succeeded
            else if (err == ECONNREFUSED) {status = CHECK_DOWN_REFUSED; close(fd); break;}
            else status = CHECK_ERROR;
        } else if (select_rc == 0) status = CHECK_DOWN_TIMEOUT;
        else status = CHECK_ERROR;

        close(fd);
        if (status == CHECK_UP)
            break;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    freeaddrinfo(res);

    r->status = status;
    r->latency_ms = (end.tv_sec - start.tv_sec) * 1000.0
                + (end.tv_nsec - start.tv_nsec) / 1000000.0;

}


void print_status_msg(check_status_t status, double ms){
    switch (status) {
        case CHECK_UP:           printf("UP - %.2f ms\n", ms); break;
        case CHECK_DOWN_REFUSED: printf("DOWN (refused) - %.2f ms\n", ms); break;
        case CHECK_DOWN_TIMEOUT: printf("DOWN (timeout) - %.2f ms\n", ms); break;
        case CHECK_ERROR:        printf("ERROR - %.2f ms\n", ms); break;
    }
}
