// agent/agent.h
#ifndef AGENT_H
#define AGENT_H

#include "nixel.h"   // for check_result_t, check_status_t

void check_client(check_result_t *r, int timeout_sec);
int  report_result(const char *server_host, const char *server_port,
                   const check_result_t *r);
void print_status_msg(check_status_t status, double ms);

#endif
