/**
 * @file server.c
 * @brief Serveur principal pour l'application Chat-Socket.
 *
 * Ce fichier contient la logique principale du serveur, y compris :
 * - La gestion des connexions clients (sockets).
 * - L'authentification des utilisateurs.
 * - La gestion des canaux de discussion.
 * - Le routage des messages et des fichiers.
 * - L'interaction avec la base de données.
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "auth/auth.h"
#include "database/database.h"
#include "file_transfer/file_transfer.h"

#define PORT 8080         /**< Port d'écoute par défaut du serveur */
#define BUFFER_SIZE 1024  /**< Taille du buffer pour les messages */
#define MAX_CLIENTS 100   /**< Nombre maximum de clients connectés simultanément */

/**
 * @brief Structure représentant le contexte d'un client connecté.
 */
typedef struct {
  int socket;           /**< Descripteur de fichier du socket client */
  bool authenticated;   /**< État d'authentification (true = connecté, false = invité) */
  char username[64];    /**< Nom d'utilisateur (si authentifié) */
  int channel_id;       /**< ID du canal actuel (0 = aucun, 1 = général) */
} ClientContext;

/**
 * @brief Tableau global stockant l'état de tous les clients potentiels.
 */
ClientContext clients[MAX_CLIENTS];

/**
 * @brief Initialise le tableau des clients au démarrage.
 * Met tous les sockets à 0 et les états à "non authentifié".
 */
void init_clients() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    clients[i].socket = 0;
    clients[i].authenticated = false;
    clients[i].channel_id = 0;
    memset(clients[i].username, 0, 64);
  }
}

/**
 * @brief Ajoute un nouveau client au tableau.
 * Cherche un emplacement libre dans le tableau `clients`.
 *
 * @param sock Le socket du nouveau client.
 * @return L'index du client dans le tableau, ou -1 si le serveur est plein.
 */
int ajouter_client(int sock) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].socket == 0) {
      clients[i].socket = sock;
      clients[i].authenticated = false; // Par défaut, non authentifié
      clients[i].channel_id = 1;        // Par défaut : canal 'général' (ID 1)
      return i;
    }
  }
  return -1;
}

/**
 * @brief Supprime un client et libère son emplacement.
 * Ferme le socket associé et réinitialise la structure ClientContext.
 *
 * @param index L'index du client à supprimer.
 */
void supprimer_client(int index) {
  if (index >= 0 && index < MAX_CLIENTS) {
    close(clients[index].socket);
    clients[index].socket = 0;
    clients[index].authenticated = false;
    clients[index].channel_id = 0;
    memset(clients[index].username, 0, 64);
  }
}

/**
 * @brief Envoie un message texte simple à un client spécifique.
 *
 * @param sock Le socket du destinataire.
 * @param msg Le message à envoyer (doit être null-terminated).
 */
void send_to_client(int sock, const char *msg) {
  send(sock, msg, strlen(msg), 0);
}

/**
 * @brief Diffuse un message à tous les autres clients du même canal.
 *
 * @param message Le contenu du message.
 * @param sender_index L'index de l'expéditeur (pour ne pas lui renvoyer le message).
 */
void broadcast_message(char *message, int sender_index) {
  char formatted_msg[BUFFER_SIZE + 70];

  snprintf(formatted_msg, sizeof(formatted_msg), "[%s] %s",
           clients[sender_index].username, message);

  int current_channel = clients[sender_index].channel_id;

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].socket != 0 && i != sender_index &&
        clients[i].authenticated && clients[i].channel_id == current_channel) {
      send(clients[i].socket, formatted_msg, strlen(formatted_msg), 0);
    }
  }
}

/**
 * @brief Diffuse un fichier binaire à tous les autres clients du canal.
 * Utilise un protocole spécifique avec un header et une taille préfixée.
 *
 * @param filename Nom du fichier.
 * @param extension Extension du fichier.
 * @param file_data Pointeur vers les données brutes du fichier.
 * @param file_size Taille du fichier en octets.
 * @param sender_index L'index de l'expéditeur.
 */
void broadcast_file(const char *filename, const char *extension,
                    unsigned char *file_data, uint32_t file_size,
                    int sender_index) {
  char header[BUFFER_SIZE];
  const char *file_end = "FILE_END\n";

  snprintf(header, sizeof(header), "FILE|%s|%u|%s|%s\n", filename, file_size,
           extension, clients[sender_index].username);

  uint32_t total_size = strlen(header) + file_size + strlen(file_end);
  uint32_t net_size = htonl(total_size);

  int current_channel = clients[sender_index].channel_id;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].socket != 0 && i != sender_index &&
        clients[i].authenticated && clients[i].channel_id == current_channel) {

      send(clients[i].socket, &net_size, sizeof(net_size), 0);
      send(clients[i].socket, header, strlen(header), 0);
      send(clients[i].socket, file_data, file_size, 0);
      send(clients[i].socket, file_end, strlen(file_end), 0);
    }
  }

  printf("[FILE_TRANSFER] %s a broadcasté %s (%u bytes) dans le canal %d\n",
         clients[sender_index].username, filename, file_size, current_channel);
}

/**
 * @brief Callback pour l'envoi de l'historique des messages.
 * Utilisé par `db_get_history`.
 */
int send_hist_cb(void *ctx, int argc, char **argv, char **col) {
  (void)col;
  int index = *(int *)ctx;
  if (argc >= 3) {
    char hist_msg[BUFFER_SIZE];
    snprintf(hist_msg, sizeof(hist_msg), "[%s] [%s] %s\n", argv[2], argv[0], argv[1]);
    if (clients[index].socket > 0) {
      send(clients[index].socket, hist_msg, strlen(hist_msg), 0);
      usleep(1000);
    }
  }
  return 0;
}

/**
 * @brief Gère l'authentification d'un client (LOGIN ou SIGNUP).
 * Parse la commande reçue et interroge la base de données.
 *
 * @param index L'index du client dans le tableau `clients`.
 * @param buffer Le message reçu contenant la commande d'auth.
 */
void traiter_auth(int index, char *buffer) {
  char command[16], user[64], pass[64];
  bool success = false;
  int socket = clients[index].socket;

  if (sscanf(buffer, "%15s %63s %63s", command, user, pass) != 3) {
    send_to_client(
        socket,
        "ERREUR: Format attendu (LOGIN user pass | SIGNUP user pass)\n");
    return;
  }

  if (strcmp(command, "LOGIN") == 0) {
    success = auth_login(user, pass);
    if (!success)
      send_to_client(socket, "ECHEC_AUTH: Login ou mdp incorrect.\n");
  } else if (strcmp(command, "SIGNUP") == 0) {
    if (auth_signup(user, pass)) {
      success = true; 
    } else {
      send_to_client(socket,
                     "ECHEC_AUTH: Utilisateur existe deja ou erreur db.\n");
      return; 
    }
  } else {
    send_to_client(socket, "ERREUR: Commande inconnue.\n");
    return;
  }

  if (success) {
    clients[index].authenticated = true;
    strncpy(clients[index].username, user, 63);
    clients[index].channel_id = 1;
    printf("[AUTH] %s connecté (Socket %d)\n", user, socket);

    if (strcmp(command, "SIGNUP") == 0) {
         send_to_client(socket, "SUCCES_SESSION: Compte créé. Bienvenue !\n");
    } else {
         send_to_client(socket, "SUCCES_SESSION: Bienvenue sur le chat. Canal: Global\n");
    }
  }
}

/**
 * @brief Callback utilisé pour envoyer la liste des canaux au client.
 *
 * Cette fonction est appelée pour chaque ligne retournée par la requête SQL
 * lors de l'exécution de la commande /list. Elle formate les informations
 * du canal (Nom, ID, Type) et les envoie au socket client.
 *
 * @param ctx Contexte (ici, pointeur vers le socket client `int *`).
 * @param argc Nombre de colonnes dans le résultat.
 * @param argv Tableau des valeurs (ex: argv[0]=Nom, argv[1]=ID, argv[2]=Type).
 * @param col Tableau des noms de colonnes (non utilisé).
 * @return 0 pour continuer l'itération.
 */
int send_channel_list_cb(void *ctx, int argc, char **argv, char **col) {
  (void)col;
  int socket = *(int *)ctx;
  
  if (argc >= 3) {
    char msg[256];
    char *name = argv[0];
    char *id = argv[1];
    char *type = argv[2];
    
    if (strcmp(type, "private") == 0) {
        snprintf(msg, sizeof(msg), "- %s \033[35m(privé)\033[0m (ID: %s)\n", name, id);
    } else {
        snprintf(msg, sizeof(msg), "- %s (ID: %s)\n", name, id);
    }
    send(socket, msg, strlen(msg), 0);
  } else if (argc >= 2) {
    char msg[256];
    snprintf(msg, sizeof(msg), "- %s (ID: %s)\n", argv[0], argv[1]);
    send(socket, msg, strlen(msg), 0);
  }
  return 0;
}

/**
 * @brief Traite les données reçues d'un client.
 * Gère les commandes (/join, /create, etc.) et les messages standards.
 *
 * @param index L'index du client.
 */
void traiter_donnees_client(int index) {
  char buffer[BUFFER_SIZE];
  int sock = clients[index].socket;

  memset(buffer, 0, BUFFER_SIZE);
  int n = recv(sock, buffer, BUFFER_SIZE - 1, 0);

  if (n <= 0) {
    printf("Client déconnecté (Socket %d)\n", sock);
    supprimer_client(index);
    return;
  }

  buffer[n] = '\0';

  if (strncmp(buffer, "/quit", 5) == 0) {
    printf("Client %s a quitté.\n", clients[index].username);
    supprimer_client(index);
    return;
  }

  if (!clients[index].authenticated) {
    traiter_auth(index, buffer);
  } else {
    if (n >= 4) {
      uint32_t potential_size;
      memcpy(&potential_size, buffer, 4);
      uint32_t message_size = ntohl(potential_size);

      if (message_size > 0 && message_size < (MAX_FILE_SIZE + 2048)) {
        unsigned char *full_message = NULL;

        int received_size =
            receive_file_message(sock, (unsigned char *)buffer, n, &full_message);

        if (received_size > 0) {
          char filename[256], extension[10], username[64];
          uint32_t file_size;
          unsigned char *data_start = NULL;

          if (parse_file_message(full_message, filename, &file_size, extension,
                                username, &data_start)) {
            if (!validate_file_extension(extension)) {
              send_to_client(sock,
                             "Erreur: Type de fichier non autorisé (jpg, jpeg, "
                             "png, pdf, txt uniquement).\n");

              free(full_message);
              return;
            }

            if (!validate_file_size(file_size)) {
              send_to_client(sock, "Erreur: Fichier trop gros (max 10 MB).\n");
              free(full_message);
              return;
            }

            printf("[FILE_TRANSFER] Reçu %s (%u bytes) de %s\n", filename,
                   file_size, clients[index].username);

            broadcast_file(filename, extension, data_start, file_size, index);
          }

          free(full_message);
        } else {
          send_to_client(sock, "Erreur: Réception fichier échouée.\n");
          if (full_message) free(full_message);
        }
        return;
      }
    }
    if (buffer[0] == '/') {
      char cmd[32], arg1[64], arg2[64], arg3[64];
      int args = sscanf(buffer, "%31s %63s %63s %63s", cmd, arg1, arg2, arg3);

      if (strcmp(cmd, "/create") == 0) {
        if (args < 3) {
          send_to_client(sock, "Usage: /create [nom_canal] [public/private]\n");
        } else {
          char *pass = (args >= 4) ? arg3 : NULL;
          int user_db_id = db_get_user_id(clients[index].username);
          if (db_create_channel(arg1, arg2, pass, user_db_id)) {
            int new_id = db_get_channel_id(arg1);
            char success_msg[128];
            snprintf(success_msg, sizeof(success_msg),
                     "Canal créé avec succès. ID: %d\n", new_id);
            send_to_client(sock, success_msg);
          } else {
            send_to_client(sock, "Erreur création canal (nom déjà pris ?).\n");
          }
        }
      } else if (strcmp(cmd, "/join") == 0) {
        if (args < 2) {
          send_to_client(sock, "Usage: /join [id_canal] [mdp (si private)]\n");
        } else {
          int cid = atoi(arg1);
          if (cid <= 0) {
            send_to_client(sock, "ID de canal invalide.\n");
          } else {
            char *pass = (args >= 3) ? arg2 : NULL;
            int uid = db_get_user_id(clients[index].username);
            if (db_validate_channel_password(cid, pass, uid)) {
              clients[index].channel_id = cid;
              char join_msg[128];

              snprintf(join_msg, sizeof(join_msg), "JOIN_SUCCESS %d\n", cid);
              send_to_client(sock, join_msg);

            } else {
              send_to_client(sock,
                             "Mot de passe incorrect ou canal introuvable.\n");
            }
          }
        }
      } else if (strcmp(cmd, "/leave") == 0) {
        clients[index].channel_id = 1;
        send_to_client(sock, "Retour au canal général.\n");
      } else if (strcmp(cmd, "/delete") == 0) {
        send_to_client(
            sock,
            "Suppression non implémentée (requiert vérification admin).\n");
      } else if (strcmp(cmd, "/list") == 0) {
        send_to_client(sock, "--- Liste des Canaux ---\n");
        int uid = db_get_user_id(clients[index].username);
        db_list_viewable_channels(uid, send_channel_list_cb, &sock);
        send_to_client(sock, "----------------------\n");
      } else if (strcmp(cmd, "/users") == 0) {
        char msg[BUFFER_SIZE];
        int cid = clients[index].channel_id;
        snprintf(msg, sizeof(msg), "--- Utilisateurs (Canal %d) ---\n", cid);
        send_to_client(sock, msg);

        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (clients[i].socket != 0 && clients[i].authenticated &&
              clients[i].channel_id == cid) {
            snprintf(msg, sizeof(msg), "  %s\n", clients[i].username);
            send(sock, msg, strlen(msg), 0);
          }
        }
        send_to_client(sock, "------------------------------\n");
      } else {
        send_to_client(sock, "Commande inconnue.\n");
      }

    } else {
      printf("[CHAT] (%d) %s: %s\n", clients[index].channel_id,
             clients[index].username, buffer);

      int uid = db_get_user_id(clients[index].username);
      db_save_message(uid, clients[index].channel_id, buffer);

      broadcast_message(buffer, index);
    }
  }
}

/**
 * @brief Point d'entrée principal du serveur.
 *
 * Initialise la base de données, configure le socket serveur,
 * et lance la boucle principale (select) pour gérer les événements réseau.
 *
 * @param argc Nombre d'arguments.
 * @param argv Arguments (argv[1] = port optionnel).
 * @return 0 en cas de succès (ne retourne jamais en pratique).
 */
int main(int argc, char **argv) {
  int server_fd, new_socket, max_sd;
  struct sockaddr_in address;
  fd_set readfds;

  int port = PORT;
  if (argc > 1) {
    port = atoi(argv[1]);
  }

  printf("Ouverture de la DB: %s\n", DATABASE_PATH);
  if (!db_open(DATABASE_PATH)) {
    fprintf(stderr,
            "ERREUR CRITIQUE: Impossible d'ouvrir la base de données.\n");
    exit(1);
  }

  init_clients();

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("Socket failed");
    exit(1);
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("Bind failed");
    exit(1);
  }

  if (listen(server_fd, 5) < 0) {
    perror("Listen failed");
    exit(1);
  }

  printf("--- Serveur Multi-Client Auth+Chat démarré sur le port %d ---\n",
         port);

  while (1) {
    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);
    max_sd = server_fd;

    for (int i = 0; i < MAX_CLIENTS; i++) {
      int sd = clients[i].socket;
      if (sd > 0)
        FD_SET(sd, &readfds);
      if (sd > max_sd)
        max_sd = sd;
    }

    int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

    if ((activity < 0) && (errno != EINTR)) {
      perror("Select error");
    }

    if (FD_ISSET(server_fd, &readfds)) {
      socklen_t addrlen = sizeof(address);
      if ((new_socket =
               accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
        perror("Accept");
      } else {
        printf("Nouvelle connexion: ip %s, port %d\n",
               inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        if (ajouter_client(new_socket) == -1) {
          printf("Serveur plein, connexion rejetée.\n");
          close(new_socket);
        }
      }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
      int sd = clients[i].socket;
      if (sd > 0 && FD_ISSET(sd, &readfds)) {
        traiter_donnees_client(i);
      }
    }
  }

  db_close();
  return 0;
}