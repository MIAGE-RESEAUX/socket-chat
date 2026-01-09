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

// Extensions de fichiers autorisées
const char *ALLOWED_EXTENSIONS[] = {".jpg", ".jpeg", ".png", ".pdf", ".txt",
                                    NULL};

// --- Fonctions utilitaires ---

void get_file_extension(const char *filename, char *ext, size_t ext_len) {
  const char *dot = strrchr(filename, '.');
  if (dot && dot != filename) {
    strncpy(ext, dot, ext_len - 1);
    ext[ext_len - 1] = '\0';
    // Convertir en minuscules
    for (int i = 0; ext[i]; i++) {
      ext[i] = tolower(ext[i]);
    }
  } else {
    ext[0] = '\0';
  }
}

int validate_file_extension(const char *ext) {
  for (int i = 0; ALLOWED_EXTENSIONS[i] != NULL; i++) {
    if (strcmp(ext, ALLOWED_EXTENSIONS[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

int validate_file_size(uint32_t size) { return size <= MAX_FILE_SIZE; }

// --- Fonctions côté client ---

void ensure_media_directory() {
  struct stat st = {0};
  if (stat(MEDIA_DIR, &st) == -1) {
    mkdir(MEDIA_DIR, 0700);
  }
}

void extract_filename_from_path(const char *path, char *filename) {
  char *path_copy = strdup(path);
  char *base = basename(path_copy);
  strcpy(filename, base);
  free(path_copy);
}

unsigned char *read_local_file(const char *filepath, uint32_t *file_size) {
  FILE *file = fopen(filepath, "rb");
  if (!file) {
    return NULL;
  }

  // Obtenir la taille du fichier
  fseek(file, 0, SEEK_END);
  *file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  // Vérifier la taille
  if (!validate_file_size(*file_size)) {
    fclose(file);
    return NULL;
  }

  // Allouer et lire
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

// --- Fonctions réseau (protocole length-prefixed) ---

int send_file_message(int sock, const char *filename, const char *extension,
                      unsigned char *file_data, uint32_t file_size) {
  char header[1024];
  const char *file_end = "FILE_END\n";

  // Préparer le header
  snprintf(header, sizeof(header), "FILE|%s|%u|%s\n", filename, file_size,
           extension);

  // Calculer taille totale
  uint32_t total_size = strlen(header) + file_size + strlen(file_end);

  // 1. Envoyer la taille totale (network byte order)
  uint32_t net_size = htonl(total_size);
  if (send(sock, &net_size, sizeof(net_size), 0) != sizeof(net_size)) {
    return -1;
  }

  // 2. Envoyer le header
  if (send(sock, header, strlen(header), 0) != (ssize_t)strlen(header)) {
    return -1;
  }

  // 3. Envoyer les données binaires
  uint32_t sent = 0;
  while (sent < file_size) {
    int n = send(sock, file_data + sent, file_size - sent, 0);
    if (n <= 0) {
      return -1;
    }
    sent += n;
  }

  // 4. Envoyer le marqueur de fin
  if (send(sock, file_end, strlen(file_end), 0) != (ssize_t)strlen(file_end)) {
    return -1;
  }

  return 0; // Succès
}

int receive_file_message(int sock, unsigned char *initial_buffer,
                         int initial_size, unsigned char **full_message) {
  // Extraire la taille du message (4 premiers bytes)
  uint32_t net_size;
  memcpy(&net_size, initial_buffer, 4);
  uint32_t message_size = ntohl(net_size);

  // Vérifier que la taille est raisonnable
  if (message_size == 0 || message_size > (MAX_FILE_SIZE + 2048)) {
    return -1;
  }

  // Allouer mémoire pour le message complet
  *full_message = malloc(message_size);
  if (!*full_message) {
    return -1;
  }

  // Copier ce qui a déjà été reçu (après les 4 bytes de taille)
  int already = initial_size - 4;
  if (already > 0) {
    memcpy(*full_message, initial_buffer + 4, already);
  }

  // Recevoir le reste du message
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

  return message_size; // Succès
}

int parse_file_message(unsigned char *message, char *filename,
                       uint32_t *file_size, char *extension, char *username,
                       unsigned char **data_start) {
  // Parser le header - 2 formats possibles :
  // Format client→serveur : FILE|filename|filesize|extension\n
  // Format serveur→client : FILE|filename|filesize|extension|username\n

  int parsed = sscanf((char *)message, "FILE|%255[^|]|%u|%9[^|\n]|%63[^\n]",
                      filename, file_size, extension, username);

  if (parsed < 3) {
    return 0; // Échec du parsing (minimum: filename, filesize, extension)
  }

  // Si username n'est pas présent (envoi du client), le mettre vide
  if (parsed == 3) {
    username[0] = '\0';
  }

  // Trouver le début des données (après le \n du header)
  char *data_ptr = strchr((char *)message, '\n');
  if (!data_ptr) {
    return 0;
  }

  *data_start = (unsigned char *)(data_ptr + 1); // Sauter le \n
  return 1; // Succès
}
