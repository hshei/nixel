#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nixel.h"
#include "agent.h"

int load_config(const char *path, target_t **out_targets, int *interval){
    target_t *targets = NULL;
    int count = 0;
    int cap = 0;

    FILE *fp = fopen(path, "r");
    if (fp == NULL) return -1;

    char buf[512];
    while (fgets(buf, sizeof(buf), fp) != NULL){
        if ((buf[0] == '\n') || (buf[0] == '#')) continue;

        // if this is the interval line
        if (sscanf(buf, "interval %d", interval) == 1) continue;

        // if this is the host port line
        char host[256], port[16];
        if (sscanf(buf, "%255s %15s", host, port) == 2){
            // reallocating the array 
            if (count == cap) {
                int new_cap = (cap == 0) ? 8 : cap * 2;      // start at 8, then double
                target_t *tmp = realloc(targets, new_cap * sizeof(target_t));
                if (tmp == NULL) { free(targets); fclose(fp); return -1; }
                targets = tmp;
                cap = new_cap;
            }
            snprintf(targets[count].host, sizeof(targets[count].host), "%s", host);
            snprintf(targets[count].port, sizeof(targets[count].port), "%s", port);
            targets[count].last_status = CHECK_UNKNOWN;
            count++;
        }
    }
    fclose(fp);
    *out_targets = targets;
    return count;
}

