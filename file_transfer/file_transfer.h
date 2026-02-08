#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

/**
 * @file file_transfer.h
 * @brief Gestion du transfert de fichiers.
 *
 * Ce module gère l'envoi et la réception de fichiers binaires ou texte
 * via le socket, en utilisant un protocole "length-prefixed" (taille + header + données).
 */

#include <stddef.h>
#include <stdint.h>

// Constantes
#define MAX_FILE_SIZE 10485760  /**< Taille maximale d'un fichier (10 MB) */
#define MEDIA_DIR "./medias"    /**< Répertoire de sauvegarde des fichiers reçus */

/**
 * @brief Liste des extensions de fichiers autorisées (jpg, png, pdf, txt, etc.)
 */
extern const char *ALLOWED_EXTENSIONS[];

// --- Fonctions utilitaires ---

/**
 * @brief Extrait l'extension d'un nom de fichier.
 *
 * @param filename Nom du fichier complet.
 * @param ext Buffer de sortie pour l'extension.
 * @param ext_len Taille du buffer.
 */
void get_file_extension(const char *filename, char *ext, size_t ext_len);

/**
 * @brief Vérifie si une extension est autorisée.
 *
 * @param ext Extension à vérifier (sans le point).
 * @return 1 si autorisée, 0 sinon.
 */
int validate_file_extension(const char *ext);

/**
 * @brief Vérifie si la taille du fichier est acceptée.
 *
 * @param size Taille en octets.
 * @return 1 si < MAX_FILE_SIZE, 0 sinon.
 */
int validate_file_size(uint32_t size);

// --- Fonctions côté client ---

/**
 * @brief Lit un fichier local en mémoire.
 *
 * @param filepath Chemin vers le fichier.
 * @param file_size Pointeur pour stocker la taille du fichier lu.
 * @return Pointeur vers les données allouées (à free()), ou NULL en cas d'erreur.
 */
unsigned char *read_local_file(const char *filepath, uint32_t *file_size);

/**
 * @brief Sauvegarde un fichier reçu sur le disque.
 * Le fichier est enregistré dans `MEDIA_DIR`.
 *
 * @param filename Nom du fichier.
 * @param data Données du fichier.
 * @param size Taille des données.
 * @return 1 en cas de succès, 0 sinon.
 */
int save_received_file(const char *filename, unsigned char *data,
                       uint32_t size);

/**
 * @brief Crée le répertoire medias s'il n'existe pas.
 */
void ensure_media_directory();

/**
 * @brief Extrait le nom de fichier (basename) d'un chemin complet.
 * Gère les séparateurs '/' et '\'.
 *
 * @param path Chemin complet.
 * @param filename Buffer de sortie pour le nom.
 */
void extract_filename_from_path(const char *path, char *filename);

// --- Fonctions réseau (protocole length-prefixed) ---

/**
 * @brief Envoie un message contenant un fichier.
 * Structure du paquet : [Network Size (4B)] [Header] [Data] [FILE_END]
 *
 * @param sock Socket de destination.
 * @param filename Nom du fichier.
 * @param extension Extension.
 * @param file_data Données binaires.
 * @param file_size Taille des données.
 * @return 0 en cas de succès, -1 en cas d'erreur.
 */
int send_file_message(int sock, const char *filename, const char *extension,
                      unsigned char *file_data, uint32_t file_size);

/**
 * @brief Reçoit un message contenant un fichier.
 * Lit la taille réseau puis le reste du paquet.
 *
 * @param sock Socket source.
 * @param initial_buffer Début des données déjà lues (le cas échéant).
 * @param initial_size Taille de `initial_buffer`.
 * @param full_message Pointeur de sortie pour le buffer complet alloué.
 * @return La taille totale du message reçu, ou -1 en cas d'erreur.
 */
int receive_file_message(int sock, unsigned char *initial_buffer,
                         int initial_size, unsigned char **full_message);

/**
 * @brief Extrait les métadonnées et les données d'un message fichier brut.
 * Parse le header "FILE|name|size|ext|user".
 *
 * @param message Le buffer complet reçu.
 * @param filename Buffer de sortie pour le nom.
 * @param file_size Pointeur de sortie pour la taille.
 * @param extension Buffer de sortie pour l'extension.
 * @param username Buffer de sortie pour l'expéditeur.
 * @param data_start Pointeur de sortie vers le début des données binaires dans `message`.
 * @return 1 en cas de succès, 0 sinon.
 */
int parse_file_message(unsigned char *message, char *filename, uint32_t *file_size,
                      char *extension, char *username, unsigned char **data_start);

#endif // FILE_TRANSFER_H
