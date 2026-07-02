#include <stdio.h>

#include "nixel.h" 
#include "agent.h"

int main(int argc, char **argv){
    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        return 1;
    }
    check_result_t r;
    r.host = argv[1];
    r.port = argv[2];
    check_client(&r, TIMEOUT);
    return report_result("127.0.0.1", "9000", &r);
}
