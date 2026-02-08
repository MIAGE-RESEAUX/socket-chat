#include "sqlite3.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file database.c
 * @brief Gestion de la base de données SQLite.
 *
 * Fournit des fonctions pour ouvrir/fermer la base, exécuter des commandes,
 * et des fonctions spécifiques pour gérer les utilisateurs, canaux et messages.
 */

static sqlite3 *db = NULL;

/**
 * @brief Ouvre la connexion à la base de données.
 * @param filename Chemin du fichier de base de données.
 * @return 1 en cas de succès, 0 sinon.
 */
int db_open(const char *filename) {
  if (sqlite3_open(filename, &db) != SQLITE_OK) {
    fprintf(stderr, "[DB] Cannot open database: %s\n", sqlite3_errmsg(db));
    return 0;
  }
  return 1;
}

/**
 * @brief Ferme la connexion à la base de données.
 */
void db_close() {
  if (db)
    sqlite3_close(db);
  db = NULL;
}

/**
 * @brief Exécute une commande SQL sans retour de données (INSERT, UPDATE, CREATE).
 * @param sql La commande SQL à exécuter.
 * @return 1 en cas de succès, 0 sinon.
 */
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

/**
 * @brief Exécute une requête SQL de sélection (SELECT).
 * @param sql La requête SQL.
 * @param callback Fonction appelée pour chaque ligne de résultat.
 * @param data Pointeur de données passé au callback.
 * @return 1 en cas de succès, 0 sinon.
 */
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

/**
 * @brief Callback d'affichage par défaut (pour le debug).
 */
int print_row(void *unused, int argc, char **argv, char **colname) {
  (void)unused;
  for (int i = 0; i < argc; i++) {
    printf("%s = %s\n", colname[i], argv[i] ? argv[i] : "NULL");
  }
  printf("\n");
  return 0;
}

#include <time.h>

/**
 * @brief Crée un canal avec un ID unique (généré aléatoirement).
 * @param name Nom du canal.
 * @param type Type de canal ("public" ou "private").
 * @param password Mot de passe (pour les canaux privés, peut être NULL).
 * @param admin_id ID de l'utilisateur créateur (admin).
 * @return 1 en cas de succès, 0 sinon.
 */
int db_create_channel(const char *name, const char *type, const char *password,
                      int admin_id) {
  char query[512];
  char pass_val[128];
  if (password)
    snprintf(pass_val, sizeof(pass_val), "'%s'", password);
  else
    strcpy(pass_val, "NULL");

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
      return 1;
  }

  return 0;
}

/**
 * @brief Liste tous les canaux publics.
 * @param callback Fonction pour traiter les résultats.
 * @param data Données utilisateur.
 * @return 1 succès, 0 erreur.
 */
int db_list_public_channels(int (*callback)(void *, int, char **, char **),
                            void *data) {
  char query[256];
  snprintf(
      query, sizeof(query),
      "SELECT name, id FROM channel WHERE type='public' ORDER BY name ASC;");
  return db_query(query, callback, data);
}

/**
 * @brief Liste les canaux visibles pour un utilisateur (publics + ceux dont il est admin).
 * @param user_id ID de l'utilisateur.
 * @param callback Fonction pour traiter les résultats.
 * @param data Données utilisateur.
 * @return 1 succès, 0 erreur.
 */
int db_list_viewable_channels(int user_id, int (*callback)(void *, int, char **, char **),
                              void *data) {
  char query[512];
  snprintf(
      query, sizeof(query),
      "SELECT name, id, type FROM channel WHERE type='public' OR admin_id=%d ORDER BY name ASC;", user_id);
  return db_query(query, callback, data);
}

/**
 * @brief Callback SQLite pour récupérer un entier (ex: ID).
 * Lit la première colonne de la première ligne.
 *
 * @param out_id Pointeur vers l'entier de sortie.
 * @param argc Nombre de colonnes.
 * @param argv Valeurs des colonnes.
 * @param colname Noms des colonnes.
 * @return 0 Toujours 0.
 */
static int db_get_int_cb(void *out_id, int argc, char **argv, char **colname) {
  (void)colname;
  if (argc > 0 && argv[0]) {
    *(int *)out_id = atoi(argv[0]);
  }
  return 0;
}

/**
 * @brief Récupère l'ID d'un canal par son nom.
 * @param name Nom du canal.
 * @return ID du canal ou -1 si introuvable.
 */
int db_get_channel_id(const char *name) {
  char query[256];
  int id = -1;
  snprintf(query, sizeof(query), "SELECT id FROM channel WHERE name='%s';",
           name);
  db_query(query, db_get_int_cb, &id);
  return id;
}

/**
 * @struct ChannelInfo
 * @brief Structure helper pour stocker les infos d'un canal (type et admin).
 */
typedef struct {
  char type[16]; /**< Type du canal ("public", "private"). */
  int admin_id;  /**< ID de l'administrateur. */
} ChannelInfo;

/**
 * @brief Callback SQLite pour remplir une structure ChannelInfo.
 * Attend 2 colonnes : type, admin_id.
 *
 * @param out Pointeur vers struct ChannelInfo.
 * @param argc Nombre de colonnes.
 * @param argv Valeurs des colonnes.
 * @param col Pointeur vers noms des colonnes (inutilisé).
 * @return 0 Toujours 0.
 */
static int get_info_cb(void *out, int argc, char **argv, char **col) {
  (void)col;
  ChannelInfo *i = (ChannelInfo *)out;
  if (argc >= 2) {
    if (argv[0])
      strncpy(i->type, argv[0], 15);
    if (argv[1])
      i->admin_id = atoi(argv[1]);
  }
  return 0;
}

/**
 * @brief Callback SQLite pour récupérer une simple chaîne.
 * Copie la première colonne dans le buffer de sortie.
 *
 * @param out Buffer de sortie (char*).
 * @param argc Nombre de colonnes.
 * @param argv Valeurs des colonnes.
 * @param col Noms des colonnes.
 * @return 0 Toujours 0.
 */
static int get_str_cb_local(void *out, int argc, char **argv, char **col) {
  (void)col;
  if (argc > 0 && argv[0])
    strcpy((char *)out, argv[0]);
  return 0;
}

/**
 * @brief Valide l'accès à un canal (mot de passe ou droits admin/public).
 * @param channel_id ID du canal.
 * @param password Mot de passe fourni (peut être NULL).
 * @param user_id ID de l'utilisateur demandeur.
 * @return true si accès autorisé, false sinon.
 */
bool db_validate_channel_password(int channel_id, const char *password, int user_id) {
  char query[256];
  ChannelInfo info = { .type="", .admin_id=-1 };
  
  snprintf(query, sizeof(query), "SELECT type, admin_id FROM channel WHERE id=%d;", channel_id);
  db_query(query, get_info_cb, &info);

  if (strcmp(info.type, "public") == 0) return true;
  if (info.admin_id == user_id) return true;

  char stored_pass[64] = {0};
  snprintf(query, sizeof(query), "SELECT password FROM channel WHERE id=%d;", channel_id);
  
  db_query(query, get_str_cb_local, stored_pass);

  if (strlen(stored_pass) == 0) return true;

  if (password && strcmp(stored_pass, password) == 0) return true;

  return false;
}

/**
 * @brief Sauvegarde un message dans la base de données.
 * @param user_id ID de l'auteur.
 * @param channel_id ID du canal.
 * @param content Contenu du message.
 * @return 1 succès, 0 erreur.
 */
int db_save_message(int user_id, int channel_id, const char *content) {
  char query[1024];
  snprintf(query, sizeof(query),
           "INSERT INTO message (user_id, channel_id, content) VALUES (%d, %d, "
           "'%s');",
           user_id, channel_id, content);
  return db_exec(query);
}

/**
 * @brief Récupère l'historique des messages d'un canal.
 * @param channel_id ID du canal.
 * @param limit Nombre max de messages.
 * @param callback Fonction pour traiter chaque message.
 * @param data Données utilisateur.
 * @return 1 succès, 0 erreur.
 */
int db_get_history(int channel_id, int limit,
                   int (*callback)(void *, int, char **, char **), void *data) {
  char query[512];
  int l = (limit > 0) ? limit : 50;
  snprintf(query, sizeof(query),
           "SELECT COALESCE(sub.username, 'Inconnu'), sub.content, sub.timestamp FROM ("
           "  SELECT u.username, m.content, m.timestamp "
           "  FROM message m "
           "  LEFT JOIN users u ON m.user_id = u.id "
           "  WHERE m.channel_id=%d AND m.content NOT LIKE '[IMG]%%' "
           "  ORDER BY m.timestamp DESC "
           "  LIMIT %d"
           ") AS sub ORDER BY sub.timestamp ASC;",
           channel_id, l);

  return db_query(query, callback, data);
}

/**
 * @brief Récupère l'ID d'un utilisateur par son pseudo.
 * @param username Pseudo de l'utilisateur.
 * @return ID ou -1 si non trouvé.
 */
int db_get_user_id(const char *username) {
  char query[256];
  int id = -1;
  snprintf(query, sizeof(query), "SELECT id FROM users WHERE username='%s';",
           username);
  db_query(query, db_get_int_cb, &id);
  return id;
}
