# Makefile pour le projet Chat-Socket
# Compile le serveur et le client, et gère la base de données.

# Compilateur et Options
CC = gcc
# Options de compilation :
# -Wall -Wextra : Affiche tous les avertissements
# -I... : Ajoute les répertoires d'en-tête (headers)
CFLAGS = -Wall -Wextra -I./database -I./auth -I./ui -I./images -I./file_transfer -I/opt/homebrew/include

# Options de l'éditeur de liens (Linker) :
# -lsqlite3 : Lie avec la bibliothèque SQLite
# -L... : Ajoute les répertoires de bibliothèque
# -lm : Lie avec la bibliothèque mathématique (pour renderer.c)
LDFLAGS = -lsqlite3 -L/opt/homebrew/lib -ldl -lpthread -lm

# Fichiers sources communs (utilisés par le client et le serveur)
SRC_COMMON = database/database.c \
             auth/auth.c \
             file_transfer/file_transfer.c

# Sources du Serveur
SERVER_SRC = server.c $(SRC_COMMON)

# Sources du Client (inclut l'UI et le rendu d'images)
CLIENT_SRC = client.c ui/ui_core.c ui/ui_display.c ui/ui_input.c images/renderer.c $(SRC_COMMON)

# Fichiers Objets (.o) générés automatiquement
OBJ_SERVER = $(SERVER_SRC:.c=.o)
OBJ_CLIENT = $(CLIENT_SRC:.c=.o)

# Noms des exécutables et fichiers de données
SERVER_TARGET = server
CLIENT_TARGET = client
DB_FILE = database/database.db
SCHEMA = database/schema.sql

# --- Cibles (Targets) ---

# Cible par défaut : tout construire et initialiser la DB
all: $(SERVER_TARGET) $(CLIENT_TARGET) initdb

# Compilation du Serveur
$(SERVER_TARGET): $(OBJ_SERVER)
	$(CC) $(OBJ_SERVER) -o $(SERVER_TARGET) $(LDFLAGS)

# Compilation du Client
$(CLIENT_TARGET): $(OBJ_CLIENT)
	$(CC) $(OBJ_CLIENT) -o $(CLIENT_TARGET) $(LDFLAGS)

# Règle générique : Compilation des fichiers .c en .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Initialisation de la Base de Données
# Crée le fichier .db seulement s'il n'existe pas
initdb:
	@if [ ! -f $(DB_FILE) ]; then \
		echo "[DB] Création de la base de données..."; \
		sqlite3 $(DB_FILE) < $(SCHEMA); \
		echo "[DB] Base de données initialisée."; \
	else \
		echo "[DB] La base de données existe déjà."; \
	fi

# Nettoyage des fichiers compilés
clean:
	rm -f $(OBJ_SERVER) $(OBJ_CLIENT) $(SERVER_TARGET) $(CLIENT_TARGET) *.o
	# Note : Les fichiers objets dans les sous-dossiers ne sont pas supprimés par *.o ici, 
	# il faudrait ajouter rm -f **/*.o si nécessaire.

# Réinitialisation complète de la Base de Données (ATTENTION : PERTE DE DONNÉES)
db-reset:
	rm -f $(DB_FILE)
	sqlite3 $(DB_FILE) < $(SCHEMA)
	echo "[DB] Base de données réinitialisée."

.PHONY: all clean initdb db-reset