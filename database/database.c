#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite3.h"

// Simple SQLite wrapper for your project
// Provides: db_open, db_close, db_exec, db_query

static sqlite3 *db = NULL;

// Open database
int db_open(const char *filename) {
    if (sqlite3_open(filename, &db) != SQLITE_OK) {
        fprintf(stderr, "[DB] Cannot open database: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

// Close database
void db_close() {
    if (db) sqlite3_close(db);
    db = NULL;
}

// Execute SQL without expecting rows (CREATE, INSERT, UPDATE...)
int db_exec(const char *sql) {
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB] SQL error: %s\n", err);
        sqlite3_free(err);
        return 0;
    }
    return 1;
}

// Callback function for SELECT results
// Users can pass their own callback
int db_query(const char *sql, int (*callback)(void*,int,char**,char**), void *data) {
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, callback, data, &err);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB] SQL query error: %s\n", err);
        sqlite3_free(err);
        return 0;
    }
    return 1;
}

// Example default callback
int print_row(void *unused, int argc, char **argv, char **colname) {
    for (int i = 0; i < argc; i++) {
        printf("%s = %s\n", colname[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}
