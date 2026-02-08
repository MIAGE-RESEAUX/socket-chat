/**
 * @file client.c
 * @brief Client principal pour l'application Socket-Chat.
 *
 * Ce fichier gère l'interface utilisateur en ligne de commande (CLI) et
 * les communications réseau avec le serveur. Il inclut :
 * - La connexion au serveur.
 * - L'authentification (phase 1).
 * - Le chat en temps réel et les commandes (phase 2).
 */

#include "ui/ui_shared.h"
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
#include <stdbool.h>

#define PORT 8080         /**< Port par défaut si non spécifié */
#define BUFFER_SIZE 1024  /**< Taille du buffer de réception */

/**
 * @brief Affiche un message d'erreur et quitte le programme.
 *
 * @param msg Le message d'erreur à afficher.
 */
void erreur(const char *msg) {
  ui_set_raw_mode(0); // Restaurer le mode terminal normal
  perror(msg);
  exit(1);
}

void trim_newline(char *str);

/**
 * @brief Établit une connexion TCP avec le serveur.
 * Résout le nom d'hôte et tente de se connecter au port spécifié.
 *
 * @param hostname Nom d'hôte ou adresse IP (ex: "127.0.0.1").
 * @param port Port du serveur.
 * @return Le descripteur de fichier du socket connecté.
 */
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

/**
 * @brief Gère la phase d'authentification (Login/Signup).
 * Boucle jusqu'à ce que le serveur renvoie "SUCCES_SESSION".
 *
 * @param sock Le socket connecté au serveur.
 */
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

    char last_cmd[BUFFER_SIZE];
    strncpy(last_cmd, buffer, BUFFER_SIZE);

    send(sock, buffer, strlen(buffer), 0);
    int valread = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (valread <= 0)
      exit(1);
    buffer[valread] = '\0';

    if (strncmp(buffer, "SUCCES_SESSION", 14) == 0) {
      char cmd[10], u[64], p[64];
      if (sscanf(last_cmd, "%s %s %s", cmd, u, p) >= 2) {
          strncpy(current_username, u, 63);
          trim_newline(current_username); 
      }
      printf(C_GREEN " ✔ Succès ! (Connecté en tant que: '%s')" C_RESET "\n", current_username);
      sleep(1);
      return;
    } else if (strncmp(buffer, "SUCCES_INSCRIPTION", 18) == 0) {
      printf(C_GREEN " %s" C_RESET "\n", buffer);
    } else {
      printf(C_RED " %s" C_RESET "\n", buffer);
    }
  }
}

/**
 * @brief Removes newline characters from string.
 */
void trim_newline(char *str) {
    char *pos;
    if ((pos = strchr(str, '\n')) != NULL) *pos = '\0';
    if ((pos = strchr(str, '\r')) != NULL) *pos = '\0';
}

/**
 * @brief Vérifie si une extension correspond à un format d'image supporté.
 * @param extension Extension du fichier (avec le .).
 * @return true si image, false sinon.
 */
static bool is_image_extension(const char *extension) {
    return (strcmp(extension, ".png") == 0 ||
            strcmp(extension, ".jpg") == 0 ||
            strcmp(extension, ".jpeg") == 0);
}

/**
 * @brief Boucle principale du chat.
 * Gère les entrées utilisateur (clavier) et les messages reçus du serveur en utilisant `select()`.
 * Active le mode "raw" du terminal pour une gestion fine de l'interface.
 *
 * @param sock Le socket connecté au serveur.
 */
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
        ui_print_pretty_msg("!!! Serveur déconnecté.");
        break;
      }
      if (n >= 4) {
        uint32_t potential_size;
        memcpy(&potential_size, buffer, 4);
        uint32_t message_size = ntohl(potential_size);

        if (message_size > 50 && message_size < (MAX_FILE_SIZE + 2048)) {
           unsigned char *full_message = NULL;
           int received_size = receive_file_message(sock, (unsigned char *)buffer, n, &full_message);

           if (received_size > 0) {
             char filename[256], extension[10], username[64];
             uint32_t file_size;
             unsigned char *data_start = NULL;

             if (parse_file_message(full_message, filename, &file_size, extension, username, &data_start)) {
               if (save_received_file(filename, data_start, file_size)) {
                 char msg[512];
                 snprintf(msg, sizeof(msg), "\n[%s] 📎 Fichier reçu: %s (%u octets)\n", username, filename, file_size);
                 ui_print_pretty_msg(msg);

                 // Afficher l'image si c'est une image
                 if (is_image_extension(extension)) {
                   char saved_path[512];
                   snprintf(saved_path, sizeof(saved_path), "./medias/%s", filename);
                   
                   // On efface la ligne courante pour un affichage propre
                   printf("\r\033[K");
                   fflush(stdout);
                   
                   img_render_file(saved_path, 80);
                 }
                 
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
           }
           continue;
        }
      }
      
       buffer[n] = '\0';
      
       char *p = buffer;
       char *next_line;
       static int current_channel_id = 1;
       
       // On itère sur chaque ligne reçue dans le buffer
       while ((next_line = strchr(p, '\n')) != NULL) {
           *next_line = '\0'; // Termine la chaîne courante
           
           if (*p != '\0') { // Si la ligne n'est pas vide
               char *line = p;
               
               if (strncmp(line, "JOIN_SUCCESS", 12) == 0) {
                   int cid = 0;
                   if (sscanf(line, "JOIN_SUCCESS %d", &cid) == 1) {
                       current_channel_id = cid;
                       ui_print_channel_header(current_channel_id);
                   }
               } else if (strncmp(line, "Retour au canal", 15) == 0) {
                   current_channel_id = 1;
                   ui_print_channel_header(current_channel_id);
               } else if (strncmp(line, "HISTORY_END", 11) == 0) {
                   ui_print_pretty_msg("--- Fin de l'historique ---\n");
                   ui_refresh_prompt();
               } else if (strncmp(line, "SUCCES_SESSION", 14) == 0) {
                   current_channel_id = 1;
                   ui_print_pretty_msg(line);
               } else {
                   char *img_tag = strstr(line, "[IMG] ");
                   if (img_tag) {
                       char *path = img_tag + 6;
                       // Nettoyage fin de ligne potentiel (cr)
                       path[strcspn(path, "\r")] = 0;
                       
                       printf("\r\033[K"); 
                       fflush(stdout);
                       img_render_file(path, 80);
                       ui_refresh_prompt();
                   } else {
                       ui_print_pretty_msg(line);
                   }
               }
           }
           p = next_line + 1; // Avance au caractère après \n
       }
       
       // Cas où le buffer ne finit pas par \n (fragment)
       // Pour l'instant on l'affiche tel quel, mais idéalement il faudrait un buffer persistant.
       // Vu que le serveur envoie \n à la fin de chaque message history, ça devrait aller.
       if (*p != '\0') {
           ui_print_pretty_msg(p);
       }
     }

    if (FD_ISSET(STDIN_FILENO, &sockets_actifs)) {
      char ch;
      if (read(STDIN_FILENO, &ch, 1) > 0) {

        if (ch == '\033') {
          char seq[2];
          if (read(STDIN_FILENO, &seq[0], 1) == 0) continue;
          if (read(STDIN_FILENO, &seq[1], 1) == 0) continue;

          if (seq[0] == '[') {
            switch(seq[1]) {
              case 'A': ui_history_up(); break;
              case 'B': ui_history_down(); break;
              case 'C': ui_move_cursor_right(); break;
              case 'D': ui_move_cursor_left(); break;
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
            ui_refresh_prompt();

            if (strncmp(temp_msg, "/sendfile ", 10) == 0) {
              char *filepath = temp_msg + 10;
              char filename[256];
              char extension[10];
              uint32_t file_size;

              unsigned char *file_data = read_local_file(filepath, &file_size);
              if (!file_data) {
                ui_print_pretty_msg("[Erreur] Impossible de lire le fichier (chemin invalide ou taille > 10MB)\n");
              } else {
                extract_filename_from_path(filepath, filename);
                get_file_extension(filename, extension, sizeof(extension));

                if (send_file_message(sock, filename, extension, file_data, file_size) == 0) {
                  char msg[BUFFER_SIZE];
                  snprintf(msg, sizeof(msg), "[Moi] 📎 Fichier envoyé: %s (%u octets)\n", filename, file_size);
                  ui_print_pretty_msg(msg);

                  if (is_image_extension(extension)) {
                    printf("\r\033[K");
                    fflush(stdout);
                    img_render_file(filepath, 80);
                  }

                  ui_refresh_prompt();
                } else {
                  ui_print_pretty_msg("[Erreur] Échec d'envoi\n");
                }
                free(file_data);
              }
            } else if (strcmp(temp_msg, "/commandes") == 0) {
              ui_print_help();
            } else if (strcmp(temp_msg, "/history") == 0) {
              send(sock, "/history", 8, 0);
            } else if (strncmp(temp_msg, "/image ", 7) == 0) {
               char *path = temp_msg + 7;
               path[strcspn(path, "\n")] = 0;
               
               char info_msg[256];
               snprintf(info_msg, sizeof(info_msg), "[INFO] Affichage de l'image: %s", path);
               ui_print_pretty_msg(info_msg);
               
               printf("\r\033[K");
               fflush(stdout);
               img_render_file(path, 80);
               ui_refresh_prompt();
               
               char send_buf[BUFFER_SIZE];
               snprintf(send_buf, sizeof(send_buf), "[IMG] %s", path);
               send(sock, send_buf, strlen(send_buf), 0);
               
            } else if (strcmp(temp_msg, "/quit") == 0) {
              break;
            } else if (strcmp(temp_msg, "/leave") == 0) {
                send(sock, temp_msg, strlen(temp_msg), 0);
            } else {
              char my_formatted_msg[BUFFER_SIZE + 10];
              snprintf(my_formatted_msg, sizeof(my_formatted_msg), "[Moi] %s", temp_msg);
              ui_print_pretty_msg(my_formatted_msg);
              
              send(sock, temp_msg, strlen(temp_msg), 0);
            }
          } else {
             ui_refresh_prompt();
          }
        } else if (ch == 127 || ch == '\b') {
          ui_delete_char();
        } else if (ch == 3) {
          break;
        } else if (ch >= 32 && ch <= 126) {
           ui_insert_char(ch);
        }
      }
    }
  }
  ui_set_raw_mode(0);
}

/**
 * @brief Point d'entrée principal du client.
 *
 * Analyse les arguments (hostname, port), établit la connexion au serveur,
 * gère la phase d'authentification, puis lance la boucle principale de chat.
 *
 * @param argc Nombre d'arguments.
 * @param argv Arguments (argv[1] = hostname, argv[2] = port).
 * @return 0 en cas de succès.
 */
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
