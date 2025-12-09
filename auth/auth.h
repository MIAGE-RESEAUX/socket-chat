#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>
#include <stddef.h> // Pour size_t

#define HASH_SIZE 128 // Taille du hash stocké

// Fonction de hachage (placeholder, à remplacer par BCrypt/Argon2)
void auth_hash_password(const char *password, char *output_hash, size_t hash_size);

// Tentative de connexion. Retourne true si succès.
bool auth_login(const char *username, const char *password);

// Inscription utilisateur. Retourne true si succès.
bool auth_signup(const char *username, const char *password);

#endif // AUTH_H