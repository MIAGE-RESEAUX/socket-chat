#include "file_transfer.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * @brief Liste des extensions de fichiers autorisées.
 */
const char *ALLOWED_EXTENSIONS[] = {".jpg", ".jpeg", ".png", ".pdf", ".txt",
                                    NULL};

/**
 * @brief Extrait l'extension d'un nom de fichier.
 * @param filename Nom du fichier.
 * @param ext Buffer de sortie pour l'extension.
 * @param ext_len Taille du buffer.
 */
void get_file_extension(const char *filename, char *ext, size_t ext_len) {
  const char *dot = strrchr(filename, '.');
  if (dot && dot != filename) {
    strncpy(ext, dot, ext_len - 1);
    ext[ext_len - 1] = '\0';
    for (int i = 0; ext[i]; i++) {
      ext[i] = tolower(ext[i]);
    }
  } else {
    ext[0] = '\0';
  }
}

/**
 * @brief Vérifie si une extension est autorisée.
 * @param ext Extension à vérifier (avec le point).
 * @return 1 si autorisée, 0 sinon.
 */
int validate_file_extension(const char *ext) {
  for (int i = 0; ALLOWED_EXTENSIONS[i] != NULL; i++) {
    if (strcmp(ext, ALLOWED_EXTENSIONS[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

/**
 * @brief Vérifie si la taille du fichier est valide.
 * @param size Taille en octets.
 * @return 1 si valide, 0 sinon.
 */
int validate_file_size(uint32_t size) { return size <= MAX_FILE_SIZE; }

/**
 * @brief Crée le dossier média s'il n'existe pas.
 */
void ensure_media_directory() {
  struct stat st = {0};
  if (stat(MEDIA_DIR, &st) == -1) {
    mkdir(MEDIA_DIR, 0700);
  }
}

/**
 * @brief Extrait le nom de fichier d'un chemin complet.
 * @param path Chemin complet.
 * @param filename Buffer de sortie pour le nom de fichier.
 */
void extract_filename_from_path(const char *path, char *filename) {
  char *path_copy = strdup(path);
  char *base = basename(path_copy);
  strcpy(filename, base);
  free(path_copy);
}

/**
 * @brief Lit un fichier local en mémoire.
 * @param filepath Chemin du fichier.
 * @param file_size Pointeur pour stocker la taille du fichier.
 * @return Pointeur vers les données (doit être libéré), ou NULL en cas d'erreur.
 */
unsigned char *read_local_file(const char *filepath, uint32_t *file_size) {
  FILE *file = fopen(filepath, "rb");
  if (!file) {
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  *file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (!validate_file_size(*file_size)) {
    fclose(file);
    return NULL;
  }

  unsigned char *data = malloc(*file_size);
  if (!data) {
    fclose(file);
    return NULL;
  }

  size_t bytes_read = fread(data, 1, *file_size, file);
  fclose(file);

  if (bytes_read != *file_size) {
    free(data);
    return NULL;
  }

  return data;
}

/**
 * @brief Sauvegarde un fichier reçu dans le dossier média.
 * @param filename Nom du fichier.
 * @param data Données du fichier.
 * @param size Taille des données.
 * @return 1 en cas de succès, 0 sinon.
 */
int save_received_file(const char *filename, unsigned char *data,
                       uint32_t size) {
  ensure_media_directory();

  char filepath[512];
  snprintf(filepath, sizeof(filepath), "%s/%s", MEDIA_DIR, filename);

  FILE *file = fopen(filepath, "wb");
  if (!file) {
    return 0;
  }

  size_t bytes_written = fwrite(data, 1, size, file);
  fclose(file);

  return bytes_written == size;
}

/**
 * @brief Envoie un fichier via le socket.
 * Utilise un protocole simple : TailleTotale + Header + Données + Fin.
 *
 * @param sock Socket de destination.
 * @param filename Nom du fichier.
 * @param extension Extension du fichier.
 * @param file_data Contenu du fichier.
 * @param file_size Taille du fichier.
 * @return 0 en cas de succès, -1 en cas d'erreur.
 */
int send_file_message(int sock, const char *filename, const char *extension,
                      unsigned char *file_data, uint32_t file_size) {
  char header[1024];
  const char *file_end = "FILE_END\n";

  snprintf(header, sizeof(header), "FILE|%s|%u|%s\n", filename, file_size,
           extension);

  uint32_t total_size = strlen(header) + file_size + strlen(file_end);

  uint32_t net_size = htonl(total_size);
  if (send(sock, &net_size, sizeof(net_size), 0) != sizeof(net_size)) {
    return -1;
  }

  if (send(sock, header, strlen(header), 0) != (ssize_t)strlen(header)) {
    return -1;
  }

  uint32_t sent = 0;
  while (sent < file_size) {
    int n = send(sock, file_data + sent, file_size - sent, 0);
    if (n <= 0) {
      return -1;
    }
    sent += n;
  }

  if (send(sock, file_end, strlen(file_end), 0) != (ssize_t)strlen(file_end)) {
    return -1;
  }

  return 0;
}

/**
 * @brief Reçoit un message de fichier.
 * Lit d'abord la taille, puis le contenu complet.
 *
 * @param sock Socket source.
 * @param initial_buffer Buffer contenant les premières données reçues (taille).
 * @param initial_size Taille des données initiales.
 * @param full_message Double pointeur pour stocker le message complet alloué.
 * @return Taille du message reçu, ou -1 en cas d'erreur.
 */
int receive_file_message(int sock, unsigned char *initial_buffer,
                         int initial_size, unsigned char **full_message) {
  uint32_t net_size;
  memcpy(&net_size, initial_buffer, 4);
  uint32_t message_size = ntohl(net_size);

  if (message_size == 0 || message_size > (MAX_FILE_SIZE + 2048)) {
    return -1;
  }

  *full_message = malloc(message_size);
  if (!*full_message) {
    return -1;
  }

  int already = initial_size - 4;
  if (already > 0) {
    memcpy(*full_message, initial_buffer + 4, already);
  }

  uint32_t received = already;
  while (received < message_size) {
    int n = recv(sock, *full_message + received, message_size - received, 0);
    if (n <= 0) {
      free(*full_message);
      *full_message = NULL;
      return -1;
    }
    received += n;
  }

  return message_size;
}

/**
 * @brief Analyse un message de fichier reçu pour extraire les métadonnées.
 *
 * @param message Message complet reçu.
 * @param filename Buffer pour le nom du fichier.
 * @param file_size Pointeur pour la taille du fichier.
 * @param extension Buffer pour l'extension.
 * @param username Buffer pour le nom d'utilisateur (si présent).
 * @param data_start Pointeur vers le début des données binaires.
 * @return 1 en cas de succès, 0 sinon.
 */
int parse_file_message(unsigned char *message, char *filename,
                       uint32_t *file_size, char *extension, char *username,
                       unsigned char **data_start) {
  int parsed = sscanf((char *)message, "FILE|%255[^|]|%u|%9[^|\n]|%63[^\n]",
                      filename, file_size, extension, username);

  if (parsed < 3) {
    return 0;
  }

  if (parsed == 3) {
    username[0] = '\0';
  }

  char *data_ptr = strchr((char *)message, '\n');
  if (!data_ptr) {
    return 0;
  }

  *data_start = (unsigned char *)(data_ptr + 1);
  return 1;
}
