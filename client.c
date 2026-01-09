#include "client_ui.h"
#include "file_transfer/file_transfer.h"
#include "images/renderer.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

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

      // FILE_TRANSFER: début - Réception avec length-prefix (MODULE!)
      if (n >= 4) {
        uint32_t potential_size;
        memcpy(&potential_size, buffer, 4);
        uint32_t message_size = ntohl(potential_size);

        // Si taille raisonnable, c'est un fichier
        if (message_size > 50 && message_size < (MAX_FILE_SIZE + 2048)) {
          unsigned char *full_message = NULL;

          // Recevoir le message complet avec le module
          int received_size = receive_file_message(
              sock, (unsigned char *)buffer, n, &full_message);

          if (received_size > 0) {
            // Parser le message
            char filename[256], extension[10], username[64];
            uint32_t file_size;
            unsigned char *data_start = NULL;

            if (parse_file_message(full_message, filename, &file_size,
                                   extension, username, &data_start)) {

              // Sauvegarder le fichier
              if (save_received_file(filename, data_start, file_size)) {
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

            free(full_message);
          } else {
            ui_print_pretty_msg("[Erreur] Réception interrompue\n");
            ui_refresh_prompt();
            if (full_message)
              free(full_message);
          }
          continue;
        }
      }
      // FILE_TRANSFER: fin

      // Message texte normal
      buffer[n] = '\0';
      buffer[n] = '\0';
      ui_print_pretty_msg(buffer);

      // Détection d'image dans le message reçu
      // Format attendu: "[User] [IMG] path/to/image"
      char *img_tag = strstr(buffer, "[IMG] ");
      if (img_tag) {
        char *path = img_tag + 6; // Skip "[IMG] "
        // Trouver la fin du path (fin de string ou autre)
        // Ici on suppose que le path va jusqu'au bout
        // On supprime d'éventuels caractères de contrôle
        path[strcspn(path, "\n")] = 0;
        path[strcspn(path, "\r")] = 0;

        // Render
        printf("\r\033[K"); // Clear current line (prompt)
        fflush(stdout);
        img_render_file(path, 80);
        ui_refresh_prompt(); // Redraw prompt below image
      }
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

              // Lire le fichier depuis le disque (utilise le module)
              unsigned char *file_data = read_local_file(filepath, &file_size);
              if (!file_data) {
                ui_print_pretty_msg(
                    "[Erreur] Impossible de lire le fichier (chemin invalide "
                    "ou taille > 10MB)\n");
                ui_refresh_prompt();
              } else {
                // Extraire le nom et l'extension
                extract_filename_from_path(filepath, filename);
                get_file_extension(filename, extension, sizeof(extension));

                // Envoyer le fichier avec le module (protocole length-prefixed)
                if (send_file_message(sock, filename, extension, file_data,
                                      file_size) == 0) {
                  char msg[BUFFER_SIZE];
                  snprintf(msg, sizeof(msg),
                           "[Moi] 📎 Fichier envoyé: %s (%u octets)\n",
                           filename, file_size);
                  ui_print_pretty_msg(msg);
                  ui_refresh_prompt();
                } else {
                  ui_print_pretty_msg("[Erreur] Échec d'envoi\n");
                  ui_refresh_prompt();
                }

                free(file_data);
              }
            }
            // FILE_TRANSFER: fin
            else if (strcmp(temp_msg, "/commandes") == 0) {
              ui_print_help();
            } else if (strncmp(temp_msg, "/image ", 7) == 0) {
              // Extract path
              char *path = temp_msg + 7;
              // Remove potential trailing newline
              path[strcspn(path, "\n")] = 0;

              // Affichage Local
              char info_msg[256];
              snprintf(info_msg, sizeof(info_msg),
                       "[INFO] Affichage de l'image: %s", path);
              ui_print_pretty_msg(info_msg);

              printf("\r\033[K"); // Clear current line (prompt)
              fflush(stdout);
              img_render_file(path, 80); // Largeur max 80
              ui_refresh_prompt();       // Redraw prompt below image

              // Envoyer le signal aux autres clients
              char send_buf[BUFFER_SIZE];
              snprintf(send_buf, sizeof(send_buf), "[IMG] %s", path);
              send(sock, send_buf, strlen(send_buf), 0);

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
