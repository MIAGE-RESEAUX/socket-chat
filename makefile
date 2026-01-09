# Simple Makefile for your C project using SQLite

CC = gcc
CFLAGS = -Wall -Wextra -I./database -I./auth -I./images -I/opt/homebrew/include
LDFLAGS = -lsqlite3 -L/opt/homebrew/lib -ldl -lpthread


# Common source files
SRC_COMMON = database/database.c \
             auth/auth.c

# Server and client sources
SERVER_SRC = server.c $(SRC_COMMON)
CLIENT_SRC = client.c client_ui.c images/renderer.c $(SRC_COMMON)

# Object files
OBJ_SERVER = $(SERVER_SRC:.c=.o)
OBJ_CLIENT = $(CLIENT_SRC:.c=.o)

SERVER_TARGET = server
CLIENT_TARGET = client
DB_FILE = database/database.db
SCHEMA = database/schema.sql

# Default target
all: $(SERVER_TARGET) $(CLIENT_TARGET) initdb

# Build server
$(SERVER_TARGET): $(OBJ_SERVER)
	$(CC) $(OBJ_SERVER) -o $(SERVER_TARGET) $(LDFLAGS)

# Build client
$(CLIENT_TARGET): $(OBJ_CLIENT)
	$(CC) $(OBJ_CLIENT) -o $(CLIENT_TARGET) $(LDFLAGS)

# Compile .c -> .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Initialize database once
initdb:
	@if [ ! -f $(DB_FILE) ]; then \
		echo "[DB] Creating database..."; \
		sqlite3 $(DB_FILE) < $(SCHEMA); \
		echo "[DB] Database initialized."; \
	else \
		echo "[DB] Database already exists."; \
	fi

# Clean
clean:
	rm -f $(OBJ_SERVER) $(OBJ_CLIENT) $(SERVER_TARGET) $(CLIENT_TARGET)

# Reset database manually
db-reset:
	rm -f $(DB_FILE)
	sqlite3 $(DB_FILE) < $(SCHEMA)
	echo "[DB] Database reset complete."

.PHONY: all clean initdb db-reset