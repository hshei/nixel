#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "nixel.h" 
#include "agent.h"

static volatile sig_atomic_t running = 1;   

static void handle_signal(int sig) {
    (void)sig;
    running = 0;             
}

int main(int argc, char **argv){
    // sending to a sockrt whose peer has closed raises the SIGPIPE signal
    // which terminates the session by default
    // this tells the system to ignore it
    signal(SIGPIPE, SIG_IGN);   
    // Those flipp the running vraible, so when the agent quits it fress the memory allocated
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    const char *config_path = (argc > 1) ? argv[1] : "nixel.conf";
    
    target_t *targets = NULL;
    int interval = 5;
    int n = load_config(config_path, &targets, &interval);
    if (n < 0) { fprintf(stderr, "config load failed: %s\n", config_path); return 1; }
    if (n == 0) { fprintf(stderr, "no targets in %s\n", config_path); return 1; }

    int server_fd = -1;
    int backoff = 1;

    while (running) {
        // ensure we are connecting, reconnecting the backoff capped at 30s
        if (server_fd == -1) {
            server_fd = connect_to_server("127.0.0.1", "9000");
            if (server_fd == -1) {
                sleep(backoff);
                backoff = (backoff < 30) ? backoff * 2 : 30; 
                continue;                                    
            }
            backoff = 1;                                      
        }
                         
        for (int i = 0; i < n; i++) {       
            check_result_t r;
            
            r.host = targets[i].host;       
            r.port = targets[i].port;
            
            check_client(&r, TIMEOUT);      
            
            if (r.status != targets[i].last_status) {
                // only log the transition of status
                fprintf(stderr, "%s:%s  %s -> %s\n",
                    r.host, r.port,
                    status_to_str(targets[i].last_status),
                    status_to_str(r.status));
                targets[i].last_status = r.status;
            }

            if (send_result(server_fd, &r) == -1) {   // send failed = conn is dead
                close(server_fd);
                server_fd = -1;                        // mark disconnected
                break;                                 // stop this pass; reconnect up top
            }
        }
        // if there is a disconnection don't wait, reconnect immediatly
        if (server_fd != -1) sleep(interval);
    }

    if (server_fd != -1) close(server_fd);
    free(targets);                          
    return 0;

}
