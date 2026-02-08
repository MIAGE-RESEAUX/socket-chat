#ifndef AUTH_H
#define AUTH_H

/**
 * @file auth.h
 * @brief Module d'authentification.
 *
 * Gère l'inscription et la connexion des utilisateurs.
 * Utilise la base de données pour stocker les crédentials (actuellement en clair ou simple hash).
 */

#include <stdbool.h>
#include <stddef.h> // Pour size_t

#define HASH_SIZE 128 /**< Taille du buffer pour le hash du mot de passe */

/**
 * @brief Hache un mot de passe.
 * Note: Cette fonction est un placeholder. En production, utiliser Argon2 ou BCrypt.
 *
 * @param password Mot de passe en clair.
 * @param output_hash Buffer de sortie pour le hash.
 * @param hash_size Taille du buffer de sortie.
 */
void auth_hash_password(const char *password, char *output_hash, size_t hash_size);

/**
 * @brief Tente de connecter un utilisateur.
 * Vérifie si le couple user/pass correspond à une entrée en base.
 *
 * @param username Nom d'utilisateur.
 * @param password Mot de passe.
 * @return true si l'authentification réussit, false sinon.
 */
bool auth_login(const char *username, const char *password);

/**
 * @brief Inscrit un nouvel utilisateur.
 * Crée une entrée dans la table users si le nom n'est pas déjà pris.
 *
 * @param username Nom d'utilisateur souhaité.
 * @param password Mot de passe.
 * @return true si l'inscription réussit, false si l'utilisateur existe déjà.
 */
bool auth_signup(const char *username, const char *password);

#endif