#include "auth.h"
#include "../database/database.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

// ---------------------------------------------------------
// Fonction de hachage simulée (placeholder)
// ---------------------------------------------------------
void auth_hash_password(const char *password, char *output_hash, size_t hash_size) {
    snprintf(output_hash, hash_size, "SHA256_SIMULATED_%s_KEY", password);
}

// ---------------------------------------------------------
// CALLBACK SQLite pour récupérer un hash d'utilisateur
// ---------------------------------------------------------
static int sqlite_get_hash_cb(void *out, int argc, char **argv, char **colname) {
    if (argc > 0 && argv[0]) {
        strncpy((char*)out, argv[0], HASH_SIZE);
    }
    return 0;
}

// ---------------------------------------------------------
// Lecture du hash stocké pour un utilisateur
// ---------------------------------------------------------
static bool sqlite_get_hash(const char *username, char *out_hash, size_t size) {
    char query[256];
    snprintf(query, sizeof(query),
             "SELECT hash FROM users WHERE username='%s';",
             username);

    out_hash[0] = '\0';  // par défaut : non trouvé

    db_query(query, sqlite_get_hash_cb, out_hash);

    return out_hash[0] != '\0';
}

// ---------------------------------------------------------
// Insère un nouvel utilisateur
// ---------------------------------------------------------
static bool sqlite_insert_user(const char *username, const char *hash) {
    char query[256];

    snprintf(query, sizeof(query),
             "INSERT INTO users (username, hash) VALUES ('%s', '%s');",
             username, hash);

    return db_exec(query);
}

// ---------------------------------------------------------
// Authentification LOGIN
// ---------------------------------------------------------
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

// ---------------------------------------------------------
// Authentification SIGNUP
// ---------------------------------------------------------
bool auth_signup(const char *username, const char *password) {
    char hash[HASH_SIZE];
    char tmp[HASH_SIZE] = {0};

    // Vérifier si déjà existant
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
