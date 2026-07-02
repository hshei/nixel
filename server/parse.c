#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "server.h"

int parse_result(const char *json_str, parsed_result_t *out) {
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) return -1;                 // malformed JSON → reject

    cJSON *host = cJSON_GetObjectItemCaseSensitive(root, "host");
    if (!cJSON_IsString(host) || host->valuestring == NULL) {
        cJSON_Delete(host);
        return -1;
    }
    snprintf(out->host, sizeof(out->host), "%s", host->valuestring);  // bounded copy
    
    cJSON *port = cJSON_GetObjectItemCaseSensitive(root, "port");
    if (!cJSON_IsString(port) || port->valuestring == NULL){
        cJSON_Delete(port);
        return -1;
    }
    snprintf(out->port, sizeof(out->port), "%s", port->valuestring);  // bounded copy

    cJSON *status = cJSON_GetObjectItemCaseSensitive(root, "status");
    if (!cJSON_IsString(status) || status->valuestring == NULL){
        cJSON_Delete(status);
        return -1;
    }
    snprintf(out->status, sizeof(out->status), "%s", status->valuestring);  // bounded copy

    cJSON *lat = cJSON_GetObjectItemCaseSensitive(root, "latency_ms");
    if (!cJSON_IsNumber(lat)) {
        cJSON_Delete(lat);
        return -1;
    }
    out->latency_ms = lat->valuedouble;

    cJSON_Delete(root);   
    return 0;
}
