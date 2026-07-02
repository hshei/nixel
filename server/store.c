#include <stdio.h>
#include <sqlite3.h>
#include <time.h>

#include "server.h"

int store_open(const char *path, sqlite3 **db) {
    if (sqlite3_open(path, db) != SQLITE_OK) return -1;

    const char *ddl =
        "CREATE TABLE IF NOT EXISTS checks ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  account_id TEXT NOT NULL,"
        "  ts INTEGER NOT NULL,"
        "  host TEXT NOT NULL,"
        "  port TEXT NOT NULL,"
        "  status TEXT NOT NULL,"
        "  latency_ms REAL NOT NULL"
        ");";

    char *err = NULL;
    if (sqlite3_exec(*db, ddl, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "create table failed: %s\n", err ? err : "?");
        sqlite3_free(err);      // sqlite3_exec allocates the error string
        return -1;
    }
    return 0;
}

int store_insert(sqlite3 *db, const parsed_result_t *pr) {
    const char *sql =
        "INSERT INTO checks (account_id, ts, host, port, status, latency_ms) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_text  (stmt, 1, "test-account", -1, SQLITE_STATIC);
    sqlite3_bind_int64 (stmt, 2, (sqlite3_int64)time(NULL));
    sqlite3_bind_text  (stmt, 3, pr->host,   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 4, pr->port,   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 5, pr->status, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 6, pr->latency_ms);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);      
    return (rc == SQLITE_DONE) ? 0 : -1;
}

void store_close(sqlite3 *db){
    sqlite3_close(db);
}