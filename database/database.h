#ifndef DATABASE_H
#define DATABASE_H

/**
 * @file database.h
 * @brief Gestion de la base de données SQLite.
 *
 * Ce module fournit une abstraction pour interagir avec la base de données SQLite3.
 * Il gère les utilisateurs, les canaux et l'historique des messages.
 */

#define DATABASE_PATH "database/database.db" /**< Chemin du fichier de la base de données */

#include "sqlite3.h"
#include <stdbool.h>

/**
 * @brief Ouvre la connexion à la base de données.
 *
 * @param filename Le chemin vers le fichier .db.
 * @return 1 en cas de succès, 0 en cas d'échec.
 */
int db_open(const char *filename);

/**
 * @brief Ferme la connexion à la base de données.
 */
void db_close();

/**
 * @brief Exécute une commande SQL sans retour de données (INSERT, UPDATE, DELETE).
 *
 * @param sql La requête SQL à exécuter.
 * @return 1 en cas de succès, 0 en cas d'échec.
 */
int db_exec(const char *sql);

/**
 * @brief Exécute une requête SELECT avec un callback personnalisé.
 *
 * @param sql La requête SQL de sélection.
 * @param callback Fonction appelée pour chaque ligne de résultat.
 * @param data Pointeur de données passé au callback (contexte).
 * @return 1 en cas de succès, 0 en cas d'échec.
 */
int db_query(const char *sql, int (*callback)(void *, int, char **, char **),
             void *data);


/**
 * @brief Crée un nouveau canal de discussion.
 *
 * @param name Nom du canal (doit être unique).
 * @param type Type du canal ("public" ou "private").
 * @param password Mot de passe (requis si type="private", sinon NULL).
 * @param admin_id ID de l'utilisateur créateur (admin).
 * @return 1 si créé avec succès, 0 sinon.
 */
int db_create_channel(const char *name, const char *type, const char *password,
                      int admin_id);

/**
 * @brief Liste les canaux publics.
 */
int db_list_public_channels(int (*callback)(void *, int, char **, char **),
                            void *data);

/**
 * @brief Liste les canaux visibles pour un utilisateur donné (publics + ceux dont il est membre/admin ?).
 * Note: Actuellement implémenté pour lister tous les canaux publics.
 */
int db_list_viewable_channels(int user_id, int (*callback)(void *, int, char **, char **),
                              void *data);

/**
 * @brief Récupère l'ID d'un canal à partir de son nom.
 * @return L'ID du canal ou -1 si introuvable.
 */
int db_get_channel_id(const char *name);

/**
 * @brief Vérifie si un utilisateur peut accéder à un canal privé.
 *
 * @param channel_id ID du canal.
 * @param password Mot de passe fourni (peut être NULL).
 * @param user_id ID de l'utilisateur (pour vérifier s'il est admin ou déjà membre).
 * @return true si l'accès est autorisé.
 */
bool db_validate_channel_password(int channel_id, const char *password, int user_id);

/**
 * @brief Sauvegarde un message dans l'historique.
 */
int db_save_message(int user_id, int channel_id, const char *content);

/**
 * @brief Récupère l'historique des messages d'un canal.
 *
 * @param channel_id ID du canal.
 * @param limit Nombre maximum de messages à récupérer (0 = tous).
 * @param callback Fonction appelée pour chaque message.
 */
int db_get_history(int channel_id, int limit,
                   int (*callback)(void *, int, char **, char **), void *data);

/**
 * @brief Récupère l'ID d'un utilisateur depuis son nom.
 * @return L'ID ou -1 si introuvable.
 */
int db_get_user_id(const char *username);

/**
 * @brief Callback utilitaire pour afficher les lignes brute (debug).
 */
int print_row(void *unused, int argc, char **argv, char **colname);

#endif // DATABASE_H
