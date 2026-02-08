#include "auth.h"
#include "../database/database.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/**
 * @file auth.c
 * @brief Implémentation du module d'authentification.
 *
 * Gère le hachage des mots de passe (simulation) et les opérations
 * de connexion et d'inscription en interagissant avec la base de données.
 */

/**
 * @brief Fonction de hachage de mot de passe (simulée).
 * @param password Mot de passe en clair.
 * @param output_hash Buffer de sortie pour le hash.
 * @param hash_size Taille du buffer de sortie.
 */
void auth_hash_password(const char *password, char *output_hash,
                        size_t hash_size) {
  snprintf(output_hash, hash_size, "SHA256_SIMULATED_%s_KEY", password);
}

/**
 * @brief Callback SQLite pour récupérer un hash d'utilisateur.
 * Copie le premier résultat (hash) dans le buffer de sortie.
 *
 * @param out Pointeur vers le buffer de sortie (char*).
 * @param argc Nombre de colonnes.
 * @param argv Valeurs des colonnes.
 * @param colname Noms des colonnes.
 * @return 0 Toujours 0.
 */
static int sqlite_get_hash_cb(void *out, int argc, char **argv,
                              char **colname) {
  (void)colname;
  if (argc > 0 && argv[0]) {
    strncpy((char *)out, argv[0], HASH_SIZE);
  }
  return 0;
}

/**
 * @brief Récupère le hash stocké pour un utilisateur donné.
 *
 * @param username Nom d'utilisateur.
 * @param out_hash Buffer de sortie pour le hash.
 * @param size Taille du buffer de sortie.
 * @return true si le hash a été trouvé, false sinon.
 */
static bool sqlite_get_hash(const char *username, char *out_hash, size_t size) {
  (void)size;
  char query[256];
  snprintf(query, sizeof(query), "SELECT hash FROM users WHERE username='%s';",
           username);

  out_hash[0] = '\0';

  db_query(query, sqlite_get_hash_cb, out_hash);

  return out_hash[0] != '\0';
}

/**
 * @brief Insère un nouvel utilisateur dans la base de données.
 *
 * @param username Nom d'utilisateur.
 * @param hash Hash du mot de passe.
 * @return true si l'insertion a réussi, false sinon.
 */
static bool sqlite_insert_user(const char *username, const char *hash) {
  char query[256];

  snprintf(query, sizeof(query),
           "INSERT INTO users (username, hash) VALUES ('%s', '%s');", username,
           hash);

  return db_exec(query);
}

/**
 * @brief Tente de connecter un utilisateur.
 * Vérifie si le nom d'utilisateur existe et si le mot de passe correspond.
 *
 * @param username Nom d'utilisateur.
 * @param password Mot de passe.
 * @return true si authentification réussie, false sinon.
 */
bool auth_login(const char *username, const char *password) {
  char entered_hash[HASH_SIZE];
  char stored_hash[HASH_SIZE] = {0};

  auth_hash_password(password, entered_hash, HASH_SIZE);

  if (sqlite_get_hash(username, stored_hash, HASH_SIZE)) {
    if (strcmp(stored_hash, entered_hash) == 0) {
      printf("[AUTH] Login réussi pour: %s\n", username);
      return true;
    }
    printf("[AUTH] Mot de passe incorrect pour: %s\n", username);
  } else {
    printf("[AUTH] Utilisateur non trouvé: %s\n", username);
  }

  return false;
}

/**
 * @brief Inscrit un nouvel utilisateur.
 * Vérifie d'abord si l'utilisateur existe déjà.
 *
 * @param username Nom d'utilisateur.
 * @param password Mot de passe.
 * @return true si inscription réussie, false si utilisateur existant ou erreur.
 */
bool auth_signup(const char *username, const char *password) {
  char hash[HASH_SIZE];
  char tmp[HASH_SIZE] = {0};

  if (sqlite_get_hash(username, tmp, HASH_SIZE)) {
    printf("[AUTH] L'utilisateur %s existe déjà.\n", username);
    return false;
  }

  auth_hash_password(password, hash, HASH_SIZE);

  if (sqlite_insert_user(username, hash)) {
    printf("[AUTH] Inscription réussie pour: %s\n", username);
    return true;
  }

  return false;
}
