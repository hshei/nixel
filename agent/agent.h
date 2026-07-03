// agent/agent.h
#ifndef AGENT_H
#define AGENT_H

#include "nixel.h"   // for check_result_t, check_status_t

void check_client(check_result_t *r, int timeout_sec);
int connect_to_server(const char *server, const char *port);
int  send_result(int server_fd, const check_result_t *r);
char *status_to_str(check_status_t s);
void print_status_msg(check_status_t status, double ms);
int load_config(const char *path, target_t **out_targets, int *interval);

#endif
