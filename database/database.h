#ifndef DATABASE_H
#define DATABASE_H

#define DATABASE_PATH "database/database.db"

#include "sqlite3.h"
#include <stdbool.h>

// Opens the SQLite database. Returns 1 on success, 0 on failure.
int db_open(const char *filename);

// Closes the currently opened database.
void db_close();

// Executes an SQL command (CREATE, INSERT, UPDATE, DELETE). Returns 1 on
// success.
int db_exec(const char *sql);

// Executes a SELECT query with a user-provided callback.
// The callback receives: void* data, int argc, char** argv, char** colname.
int db_query(const char *sql, int (*callback)(void *, int, char **, char **),
             void *data);

// Channel & Message operations
int db_create_channel(const char *name, const char *type, const char *password,
                      int admin_id);
int db_list_public_channels(int (*callback)(void *, int, char **, char **),
                            void *data);
int db_list_viewable_channels(int user_id, int (*callback)(void *, int, char **, char **),
                              void *data);
int db_get_channel_id(const char *name);
// Returns true if channel is private and password matches, or if public, or if user is admin.
bool db_validate_channel_password(int channel_id, const char *password, int user_id);
int db_save_message(int user_id, int channel_id, const char *content);
// Callback should print/send lines. limit=0 for all.
int db_get_history(int channel_id, int limit,
                   int (*callback)(void *, int, char **, char **), void *data);

// User helper
int db_get_user_id(const char *username);

// Example callback that prints rows to stdout.
int print_row(void *unused, int argc, char **argv, char **colname);

#endif // DATABASE_H
