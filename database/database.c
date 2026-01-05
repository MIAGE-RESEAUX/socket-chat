#include "sqlite3.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  if (db)
    sqlite3_close(db);
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
int db_query(const char *sql, int (*callback)(void *, int, char **, char **),
             void *data) {
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

// --- Channel & Message Implementation ---

#include <time.h>

int db_create_channel(const char *name, const char *type, const char *password,
                      int admin_id) {
  char query[512];
  // Use 'NULL' for password if it's NULL, otherwise quote it
  char pass_val[128];
  if (password)
    snprintf(pass_val, sizeof(pass_val), "'%s'", password);
  else
    strcpy(pass_val, "NULL");

  // Attempt to generate a unique 4-digit ID (1000-9999)
  // Simple retry logic
  srand(time(NULL) + admin_id);
  int max_retries = 10;
  int rc = 0;

  for (int i = 0; i < max_retries; i++) {
    int rand_id = (rand() % 9000) + 1000;

    snprintf(query, sizeof(query),
             "INSERT INTO channel (id, name, type, password, admin_id) VALUES "
             "(%d, '%s', "
             "'%s', %s, %d);",
             rand_id, name, type, pass_val, admin_id);

    rc = db_exec(query);
    if (rc)
      return 1; // Success
  }

  return 0; // Failed to find unique ID or other error
}

// Helper callback for getting integer ID
static int db_get_int_cb(void *out_id, int argc, char **argv, char **colname) {
  if (argc > 0 && argv[0]) {
    *(int *)out_id = atoi(argv[0]);
  }
  return 0;
}

int db_get_channel_id(const char *name) {
  char query[256];
  int id = -1;
  snprintf(query, sizeof(query), "SELECT id FROM channel WHERE name='%s';",
           name);
  db_query(query, db_get_int_cb, &id);
  return id;
}

// Helper callback for fetching password
static int db_get_pass_cb(void *out_pass, int argc, char **argv,
                          char **colname) {
  if (argc > 0 && argv[0]) {
    strncpy((char *)out_pass, argv[0], 64);
  } else {
    // If password column is NULL (e.g. public channel or no password set
    // correctly)
    ((char *)out_pass)[0] = '\0';
  }
  return 0;
}

// Quick local callback for string
static int get_str_cb(void *out, int argc, char **argv, char **col) {
  if (argc > 0 && argv[0])
    strcpy((char *)out, argv[0]);
  return 0;
}

bool db_validate_channel_password(int channel_id, const char *password) {
  // 1. Check type. If public, return true.
  // 2. If private, check password.
  char query[256];
  char stored_type[16] = {0};
  char stored_pass[64] = {0};

  // Check type first
  snprintf(query, sizeof(query), "SELECT type FROM channel WHERE id=%d;",
           channel_id);
  db_query(query, get_str_cb, stored_type);

  if (strcmp(stored_type, "public") == 0)
    return true;

  // It is private, check password
  snprintf(query, sizeof(query), "SELECT password FROM channel WHERE id=%d;",
           channel_id);
  db_query(query, get_str_cb, stored_pass);

  if (password && strcmp(stored_pass, password) == 0)
    return true;

  return false;
}

int db_save_message(int user_id, int channel_id, const char *content) {
  char query[1024];
  // Simple sanitization should be done, but for this exercise we assume basic
  // content
  snprintf(query, sizeof(query),
           "INSERT INTO message (user_id, channel_id, content) VALUES (%d, %d, "
           "'%s');",
           user_id, channel_id, content);
  return db_exec(query);
}

int db_get_history(int channel_id, int limit,
                   int (*callback)(void *, int, char **, char **), void *data) {
  char query[512];
  int l = (limit > 0) ? limit : 50; // default limit
  // JOIN to get username
  snprintf(query, sizeof(query),
           "SELECT u.username, m.content, m.timestamp "
           "FROM message m "
           "JOIN users u ON m.user_id = u.id "
           "WHERE m.channel_id=%d "
           "ORDER BY m.timestamp ASC "
           "LIMIT %d;",
           channel_id, l);

  return db_query(query, callback, data);
}

int db_get_user_id(const char *username) {
  char query[256];
  int id = -1;
  snprintf(query, sizeof(query), "SELECT id FROM users WHERE username='%s';",
           username);
  db_query(query, db_get_int_cb, &id);
  return id;
}
