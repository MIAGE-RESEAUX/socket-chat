#include "client_ui.h"
#include <arpa/inet.h>
#include <libgen.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_FILE_SIZE 10485760  // 10 MB
#define MEDIA_DIR "./medias"

// FILE_TRANSFER: début - Fonctions utilitaires

// Créer le répertoire medias s'il n'existe pas
void ensure_media_directory() {
  struct stat st = {0};
  if (stat(MEDIA_DIR, &st) == -1) {
    mkdir(MEDIA_DIR, 0700);
  }
}

// Extraire le nom de fichier d'un chemin
void extract_filename_from_path(const char *path, char *filename) {
  char *path_copy = strdup(path);
  char *base = basename(path_copy);
  strcpy(filename, base);
  free(path_copy);
}

// Valider la taille du fichier
int validate_file_size(uint32_t size) { return size <= MAX_FILE_SIZE; }

// Lire un fichier local
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

// Sauvegarder un fichier reçu
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

// FILE_TRANSFER: fin

void erreur(const char *msg) {
  ui_set_raw_mode(0);
  perror(msg);
  exit(1);
}

int connecter_au_serveur(const char *hostname, int port) {
  int sock;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    erreur("Erreur socket");
  server = gethostbyname(hostname);
  if (server == NULL) {
    fprintf(stderr, "Hôte introuvable\n");
    exit(0);
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  serv_addr.sin_port = htons(port);

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    erreur("Connexion échouée");
  return sock;
}

void phase_authentification(int sock) {
  char buffer[BUFFER_SIZE];
  ui_show_banner();
  printf(C_YELLOW "  Commandes: LOGIN/SIGNUP user pass\n" C_RESET "\n");

  while (1) {
    printf(C_GREEN " AUTH " C_RESET "> ");
    fflush(stdout);
    if (!fgets(buffer, BUFFER_SIZE, stdin))
      exit(0);
    buffer[strcspn(buffer, "\n")] = '\0';
    if (strlen(buffer) == 0)
      continue;

    send(sock, buffer, strlen(buffer), 0);
    int valread = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (valread <= 0)
      exit(1);
    buffer[valread] = '\0';

    if (strncmp(buffer, "SUCCES_SESSION", 14) == 0) {
      printf(C_GREEN " ✔ Succès !" C_RESET "\n");
      sleep(1);
      return;
    } else if (strncmp(buffer, "SUCCES_INSCRIPTION", 18) == 0) {
      printf(C_GREEN " %s" C_RESET "\n", buffer);
    } else {
      printf(C_RED " %s" C_RESET "\n", buffer);
    }
  }
}

void phase_chat(int sock) {
  char buffer[BUFFER_SIZE];
  char temp_msg[BUFFER_SIZE];
  fd_set sockets_actifs;
  int max_sd = sock;

  ui_show_banner();
  printf(C_GREEN C_BOLD " --- CHAT ACTIF (Canal: Général) --- " C_RESET "\n");
  printf(C_WHITE C_ITALIC
         " (/commandes pour l'aide, Flèches pour historique)" C_RESET "\n\n");
  ui_refresh_prompt();
  ui_set_raw_mode(1);

  while (1) {
    FD_ZERO(&sockets_actifs);
    FD_SET(STDIN_FILENO, &sockets_actifs);
    FD_SET(sock, &sockets_actifs);

    if (select(max_sd + 1, &sockets_actifs, NULL, NULL, NULL) < 0)
      continue;

    if (FD_ISSET(sock, &sockets_actifs)) {
      memset(buffer, 0, BUFFER_SIZE);
      int n = recv(sock, buffer, BUFFER_SIZE - 1, 0);
      if (n <= 0) {
        ui_print_pretty_msg("!!! Serveur déconnecté.\n");
        break;
      }

      // FILE_TRANSFER: début - Réception avec length-prefix (SIMPLE!)
      // Détecter si c'est un transfert de fichier (commence par 4 bytes)
      if (n >= 4) {
        uint32_t potential_size;
        memcpy(&potential_size, buffer, 4);
        uint32_t message_size = ntohl(potential_size);

        // Si taille raisonnable, c'est un fichier
        if (message_size > 50 && message_size < (MAX_FILE_SIZE + 1024)) {
          // Allouer mémoire pour le message complet
          unsigned char *full_message = malloc(message_size);
          if (!full_message) {
            ui_print_pretty_msg("[Erreur] Mémoire insuffisante\n");
            ui_refresh_prompt();
            continue;
          }

          // Copier ce qui a déjà été reçu (après les 4 bytes de taille)
          int already = n - 4;
          if (already > 0) {
            memcpy(full_message, buffer + 4, already);
          }

          // Recevoir le reste du message (BOUCLE SIMPLE)
          uint32_t received = already;
          while (received < message_size) {
            int nr =
                recv(sock, full_message + received, message_size - received, 0);
            if (nr <= 0) {
              free(full_message);
              ui_print_pretty_msg("[Erreur] Réception interrompue\n");
              ui_refresh_prompt();
              goto skip_file;
            }
            received += nr;
          }

          // Maintenant on a TOUT le message, parser facilement !
          char filename[256];
          char extension[10];
          char username[64];
          uint32_t file_size;

          if (sscanf((char *)full_message, "FILE|%255[^|]|%u|%9[^|]|%63[^\n]",
                     filename, &file_size, extension, username) == 4) {

            // Trouver le début des données (après le \n)
            char *data_start = strchr((char *)full_message, '\n');
            if (data_start) {
              data_start++; // Sauter le \n

              // Sauvegarder le fichier
              if (save_received_file(filename, (unsigned char *)data_start,
                                     file_size)) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "\n[%s] 📎 Fichier reçu: %s (%u octets)\n", username,
                         filename, file_size);
                ui_print_pretty_msg(msg);
                ui_refresh_prompt();
              } else {
                ui_print_pretty_msg("[Erreur] Échec sauvegarde\n");
                ui_refresh_prompt();
              }
            }
          }

          free(full_message);
        skip_file:
          continue; // Pas de fall-through vers le texte normal
        }
      }
      // FILE_TRANSFER: fin

      // Message texte normal
      buffer[n] = '\0';
      ui_print_pretty_msg(buffer);
    }

    if (FD_ISSET(STDIN_FILENO, &sockets_actifs)) {
      char ch;
      if (read(STDIN_FILENO, &ch, 1) > 0) {

        if (ch == '\033') {
          char seq[2];
          if (read(STDIN_FILENO, &seq[0], 1) == 0)
            continue;
          if (read(STDIN_FILENO, &seq[1], 1) == 0)
            continue;

          if (seq[0] == '[') {
            if (seq[1] == 'A') {
              ui_history_up();
            } else if (seq[1] == 'B') {
              ui_history_down();
            }
          }
          continue;
        }

        if (ch == '\n' || ch == '\r') {
          if (input_len > 0) {
            ui_history_add(input_buffer);

            memset(temp_msg, 0, BUFFER_SIZE);
            strcpy(temp_msg, input_buffer);

            ui_reset_input();

            // FILE_TRANSFER: début - Commande /sendfile
            if (strncmp(temp_msg, "/sendfile ", 10) == 0) {
              char *filepath = temp_msg + 10;
              char filename[256];
              char extension[10];
              uint32_t file_size;

              // Lire le fichier depuis le disque
              unsigned char *file_data = read_local_file(filepath, &file_size);
              if (!file_data) {
                ui_print_pretty_msg(
                    "[Erreur] Impossible de lire le fichier (chemin invalide "
                    "ou taille > 10MB)\n");
                ui_refresh_prompt();
              } else {
                // Extraire le nom du fichier
                extract_filename_from_path(filepath, filename);

                // Extraire l'extension
                char *dot = strrchr(filename, '.');
                if (dot) {
                  strncpy(extension, dot, sizeof(extension) - 1);
                  extension[sizeof(extension) - 1] = '\0';
                } else {
                  strcpy(extension, "");
                }

                // Préparer le header
                char header[BUFFER_SIZE];
                snprintf(header, sizeof(header), "FILE|%s|%u|%s\n", filename,
                         file_size, extension);

                // Calculer taille totale
                const char *file_end = "FILE_END\n";
                uint32_t total_size =
                    strlen(header) + file_size + strlen(file_end);

                // 1. Envoyer la taille totale (network byte order)
                uint32_t net_size = htonl(total_size);
                send(sock, &net_size, sizeof(net_size), 0);

                // 2. Envoyer le header
                send(sock, header, strlen(header), 0);

                // 3. Envoyer les données binaires
                uint32_t sent = 0;
                while (sent < file_size) {
                  int n = send(sock, file_data + sent, file_size - sent, 0);
                  if (n <= 0) {
                    ui_print_pretty_msg("[Erreur] Échec d'envoi\n");
                    ui_refresh_prompt();
                    free(file_data);
                    goto cleanup_send;
                  }
                  sent += n;
                }

                // 4. Envoyer le marqueur de fin
                send(sock, file_end, strlen(file_end), 0);

                // Confirmation
                char msg[BUFFER_SIZE];
                snprintf(msg, sizeof(msg),
                         "[Moi] 📎 Fichier envoyé: %s (%u octets)\n", filename,
                         file_size);
                ui_print_pretty_msg(msg);
                ui_refresh_prompt();

              cleanup_send:
                free(file_data);
              }
            }
            // FILE_TRANSFER: fin
            else if (strcmp(temp_msg, "/commandes") == 0) {
              ui_print_help();
            } else if (strcmp(temp_msg, "/quit") == 0) {
              break;
            } else {
              char my_formatted_msg[BUFFER_SIZE + 10];
              snprintf(my_formatted_msg, sizeof(my_formatted_msg), "[Moi] %s",
                       temp_msg);
              ui_print_pretty_msg(my_formatted_msg);

              send(sock, temp_msg, strlen(temp_msg), 0);
            }
          } else {
            printf("\r\n");
            ui_refresh_prompt();
          }
        } else if (ch == 127 || ch == '\b') {
          ui_delete_char();
        } else if (ch == 3) {
          break;
        } else if (ch >= 32 && ch <= 126) {
          if (input_len < BUFFER_SIZE - 1) {
            input_buffer[input_len++] = ch;
            input_buffer[input_len] = '\0';
            ui_print_char(ch);
          }
        }
      }
    }
  }
  ui_set_raw_mode(0);
}

int main(int argc, char **argv) {
  char *hostname = "127.0.0.1";
  int port = PORT;

  if (argc >= 2)
    hostname = argv[1];
  if (argc >= 3)
    port = atoi(argv[2]);

  ui_clear_screen();
  int sock = connecter_au_serveur(hostname, port);
  phase_authentification(sock);
  phase_chat(sock);
  close(sock);
  printf("\nBye!\n");
  return 0;
}
