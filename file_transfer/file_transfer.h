#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include <stddef.h>
#include <stdint.h>

// Constantes
#define MAX_FILE_SIZE 10485760  // 10 MB
#define MEDIA_DIR "./medias"

// Extensions de fichiers autorisées
extern const char *ALLOWED_EXTENSIONS[];

// --- Fonctions utilitaires ---

// Extraire l'extension d'un fichier (avec conversion en minuscules)
void get_file_extension(const char *filename, char *ext, size_t ext_len);

// Valider si l'extension est autorisée (jpg, jpeg, png, pdf, txt)
int validate_file_extension(const char *ext);

// Valider la taille du fichier (< 10 MB)
int validate_file_size(uint32_t size);

// --- Fonctions côté client ---

// Lire un fichier local depuis le disque
unsigned char *read_local_file(const char *filepath, uint32_t *file_size);

// Sauvegarder un fichier reçu dans ./medias/
int save_received_file(const char *filename, unsigned char *data,
                       uint32_t size);

// Créer le répertoire medias s'il n'existe pas
void ensure_media_directory();

// Extraire le nom de fichier d'un chemin complet
void extract_filename_from_path(const char *path, char *filename);

// --- Fonctions réseau (protocole length-prefixed) ---

// Envoyer un message de fichier avec length-prefix
// Retourne 0 en cas de succès, -1 en cas d'erreur
int send_file_message(int sock, const char *filename, const char *extension,
                      unsigned char *file_data, uint32_t file_size);

// Recevoir un message de fichier avec length-prefix
// Retourne la taille du message reçu, ou -1 en cas d'erreur
// full_message doit être libéré par l'appelant
int receive_file_message(int sock, unsigned char *initial_buffer,
                         int initial_size, unsigned char **full_message);

// Parser un message de fichier (header + données)
// Retourne 1 si succès, 0 sinon
int parse_file_message(unsigned char *message, char *filename, uint32_t *file_size,
                      char *extension, char *username, unsigned char **data_start);

#endif // FILE_TRANSFER_H
