#ifndef DATABASE_H
#define DATABASE_H

#define DATABASE_PATH "database/database.db"

#include "sqlite3.h"

// Opens the SQLite database. Returns 1 on success, 0 on failure.
int db_open(const char *filename);

// Closes the currently opened database.
void db_close();

// Executes an SQL command (CREATE, INSERT, UPDATE, DELETE). Returns 1 on success.
int db_exec(const char *sql);

// Executes a SELECT query with a user-provided callback.
// The callback receives: void* data, int argc, char** argv, char** colname.
int db_query(const char *sql, int (*callback)(void*, int, char**, char**), void *data);

// Example callback that prints rows to stdout.
int print_row(void *unused, int argc, char **argv, char **colname);

#endif // DATABASE_H
