#include <stdio.h>

#include "nixel.h" 
#include "agent.h"

int main(int argc, char **argv){
    check_result_t r;
    r.host = argv[1];
    r.port = argv[2];
    check_client(&r, TIMEOUT);
    int report = report_result("127.0.0.1", "9000", &r);

    return report;
}