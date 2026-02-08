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

// Inclusion de vos modules (assurez-vous qu'ils existent)
#include "auth/auth.h"
#include "database/database.h"
#include "file_transfer/file_transfer.h"

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100

// Structure pour représenter un client et son état
typedef struct {
  int socket;
  bool authenticated; // false = phase auth, true = phase chat
  char username[64];
  int channel_id; // Current channel ID
} ClientContext;

// Tableau global des clients
ClientContext clients[MAX_CLIENTS];

// --- Fonctions de gestion de la liste des clients ---

void init_clients() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    clients[i].socket = 0;
    clients[i].authenticated = false;
    clients[i].channel_id = 0;
    memset(clients[i].username, 0, 64);
  }
}

int ajouter_client(int sock) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].socket == 0) {
      clients[i].socket = sock;
      clients[i].authenticated = false; // Par défaut, non authentifié
      clients[i].channel_id = 1;        // Default to 'general' (ID 1)
      return i;
    }
  }
  return -1;
}

void supprimer_client(int index) {
  if (index >= 0 && index < MAX_CLIENTS) {
    close(clients[index].socket);
    clients[index].socket = 0;
    clients[index].authenticated = false;
    clients[index].channel_id = 0;
    memset(clients[index].username, 0, 64);
  }
}

// --- Fonctions Réseau ---

void send_to_client(int sock, const char *msg) {
  send(sock, msg, strlen(msg), 0);
}

// Diffuse un message à tous les AUTRES clients AUTHENTIFIÉS
void broadcast_message(char *message, int sender_index) {
  char formatted_msg[BUFFER_SIZE + 70];

  // Formatage : [User] Message
  snprintf(formatted_msg, sizeof(formatted_msg), "[%s] %s",
           clients[sender_index].username, message);

  int current_channel = clients[sender_index].channel_id;

  for (int i = 0; i < MAX_CLIENTS; i++) {
    // On envoie seulement si le socket existe, n'est pas l'expéditeur, est
    // authentifié, et est dans le même canal
    if (clients[i].socket != 0 && i != sender_index &&
        clients[i].authenticated && clients[i].channel_id == current_channel) {
      send(clients[i].socket, formatted_msg, strlen(formatted_msg), 0);
    }
  }
}

// FILE_TRANSFER: début - Broadcast de fichiers
// Diffuse un fichier à tous les AUTRES clients AUTHENTIFIÉS du canal
void broadcast_file(const char *filename, const char *extension,
                    unsigned char *file_data, uint32_t file_size,
                    int sender_index) {
  char header[BUFFER_SIZE];
  const char *file_end = "FILE_END\n";

  // Préparer le header avec le nom de l'expéditeur
  snprintf(header, sizeof(header), "FILE|%s|%u|%s|%s\n", filename, file_size,
           extension, clients[sender_index].username);

  // Calculer taille totale pour length-prefix
  uint32_t total_size = strlen(header) + file_size + strlen(file_end);
  uint32_t net_size = htonl(total_size);

  int current_channel = clients[sender_index].channel_id;

  // Broadcaster à tous les clients du canal (sauf expéditeur)
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].socket != 0 && i != sender_index &&
        clients[i].authenticated && clients[i].channel_id == current_channel) {

      // Envoyer: taille + header + données + FILE_END
      send(clients[i].socket, &net_size, sizeof(net_size), 0);
      send(clients[i].socket, header, strlen(header), 0);
      send(clients[i].socket, file_data, file_size, 0);
      send(clients[i].socket, file_end, strlen(file_end), 0);
    }
  }

  printf("[FILE_TRANSFER] %s a broadcasté %s (%u bytes) dans le canal %d\n",
         clients[sender_index].username, filename, file_size, current_channel);
}
// FILE_TRANSFER: fin

// --- Callbacks Base de Données ---

int send_hist_cb(void *ctx, int argc, char **argv, char **col) {
  (void)col; // Silence unused parameter warning
  int index = *(int *)ctx;
  if (argc >= 3) {
    char hist_msg[BUFFER_SIZE];
    // argv[0]=username, argv[1]=content, argv[2]=timestamp
    // Format: [TIMESTAMP] [User] Message
    snprintf(hist_msg, sizeof(hist_msg), "[%s] [%s] %s\n", argv[2], argv[0], argv[1]);
    if (clients[index].socket > 0) {
      send(clients[index].socket, hist_msg, strlen(hist_msg), 0);
      usleep(1000); // Slight delay to ensure order/buffering
    }
  }
  return 0;
}

// --- Logique Métier ---

// Traitement de l'authentification (LOGIN/SIGNUP)
void traiter_auth(int index, char *buffer) {
  char command[16], user[64], pass[64];
  bool success = false;
  int socket = clients[index].socket;

  // Parsing de la commande
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
      // Auto-login enabling: We do NOT send a separate success message here.
      // We let the unified success block below send SUCCES_SESSION.
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

  // Si authentification réussie
  if (success) {
    clients[index].authenticated = true;
    strncpy(clients[index].username, user, 63);
    clients[index].channel_id = 1; // Force general channel on login
    printf("[AUTH] %s connecté (Socket %d)\n", user, socket);

    // Ce message déclenche le passage en mode chat côté client
    // We can customize it slightly if we want, or just stick to standard.
    if (strcmp(command, "SIGNUP") == 0) {
         send_to_client(socket, "SUCCES_SESSION: Compte créé. Bienvenue !\n");
    } else {
         send_to_client(socket, "SUCCES_SESSION: Bienvenue sur le chat. Canal: Global\n");
    }

    // Send history callback
    // db_get_history(1, 50, send_hist_cb, &index);
    // send_to_client(socket, "HISTORY_END\n");
  }
}

// Callback for listing channels
int send_channel_list_cb(void *ctx, int argc, char **argv, char **col) {
  (void)col; // Silence unused parameter warning
  int socket = *(int *)ctx;
  
  if (argc >= 3) { // Expecting: Name, ID, Type
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
  } else if (argc >= 2) { // Fallback
    char msg[256];
    snprintf(msg, sizeof(msg), "- %s (ID: %s)\n", argv[0], argv[1]);
    send(socket, msg, strlen(msg), 0);
  }
  return 0;
}

// Traitement principal d'un message reçu
void traiter_donnees_client(int index) {
  char buffer[BUFFER_SIZE];
  int sock = clients[index].socket;

  memset(buffer, 0, BUFFER_SIZE);
  int n = recv(sock, buffer, BUFFER_SIZE - 1, 0);

  // Déconnexion
  if (n <= 0) {
    printf("Client déconnecté (Socket %d)\n", sock);
    supprimer_client(index);
    return;
  }

  buffer[n] = '\0'; // Assurer la fin de chaîne

  // Commande de sortie
  if (strncmp(buffer, "/quit", 5) == 0) {
    printf("Client %s a quitté.\n", clients[index].username);
    supprimer_client(index);
    return;
  }

  // ROUTAGE selon l'état du client
  if (!clients[index].authenticated) {
    // Cas 1: Pas encore connecté -> On tente le LOGIN/SIGNUP
    traiter_auth(index, buffer);
  } else {
    // Cas 2: Déjà connecté

    // FILE_TRANSFER: début - Réception avec length-prefix
    if (n >= 4) {
      // Vérifier si c'est un transfert de fichier
      uint32_t potential_size;
      memcpy(&potential_size, buffer, 4);
      uint32_t message_size = ntohl(potential_size);

      // Si taille raisonnable, c'est probablement un fichier
      if (message_size > 0 && message_size < (MAX_FILE_SIZE + 2048)) {
        unsigned char *full_message = NULL;

        // Recevoir le message complet avec le module
        int received_size =
            receive_file_message(sock, (unsigned char *)buffer, n, &full_message);

        if (received_size > 0) {
          // Parser le message
          char filename[256], extension[10], username[64];
          uint32_t file_size;
          unsigned char *data_start = NULL;

          if (parse_file_message(full_message, filename, &file_size, extension,
                                username, &data_start)) {



            // Valider l'extension
            if (!validate_file_extension(extension)) {
              send_to_client(sock,
                             "Erreur: Type de fichier non autorisé (jpg, jpeg, "
                             "png, pdf, txt uniquement).\n");

              free(full_message);
              return;
            }

            // Valider la taille
            if (!validate_file_size(file_size)) {
              send_to_client(sock, "Erreur: Fichier trop gros (max 10 MB).\n");
              free(full_message);
              return;
            }

            printf("[FILE_TRANSFER] Reçu %s (%u bytes) de %s\n", filename,
                   file_size, clients[index].username);

            // Broadcaster le fichier
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
    // FILE_TRANSFER: fin

    // Commandes Channel
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
          int cid = atoi(arg1); // Interpret arg1 as ID
          if (cid <= 0) {
            send_to_client(sock, "ID de canal invalide.\n");
          } else {
            char *pass = (args >= 3) ? arg2 : NULL;
            int uid = db_get_user_id(clients[index].username);
            // Validate directly with ID and User ID
            if (db_validate_channel_password(cid, pass, uid)) {
              clients[index].channel_id = cid;
              char join_msg[128];
              // Note: We don't have the name easily here without another DB call.
              // We will send ID and let client handle or server sends simple confirmation.
              // Ideally: "JOIN_SUCCESS [ID]"
              snprintf(join_msg, sizeof(join_msg), "JOIN_SUCCESS %d\n", cid);
              send_to_client(sock, join_msg);

              // db_get_history(cid, 50, send_hist_cb, &index);
              // send_to_client(sock, "HISTORY_END\n");

            } else {
              send_to_client(sock,
                             "Mot de passe incorrect ou canal introuvable.\n");
            }
          }
        }
      } else if (strcmp(cmd, "/leave") == 0) {
        clients[index].channel_id = 1; // Retour general
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
      // Message standard
      printf("[CHAT] (%d) %s: %s\n", clients[index].channel_id,
             clients[index].username, buffer);

      // Save to DB
      int uid = db_get_user_id(clients[index].username);
      db_save_message(uid, clients[index].channel_id, buffer);

      broadcast_message(buffer, index);
    }
  }
}

int main(int argc, char **argv) {
  int server_fd, new_socket, max_sd;
  struct sockaddr_in address;
  fd_set readfds; // Ensemble des descripteurs de fichiers à lire

  // Gestion du port via argument ou défaut
  int port = PORT;
  if (argc > 1) {
    port = atoi(argv[1]);
  }

  // 1. Initialisation BDD
  printf("Ouverture de la DB: %s\n", DATABASE_PATH);
  if (!db_open(DATABASE_PATH)) {
    fprintf(stderr,
            "ERREUR CRITIQUE: Impossible d'ouvrir la base de données.\n");
    exit(1);
  }

  init_clients();

  // 2. Création du Socket Serveur
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("Socket failed");
    exit(1);
  }

  // Options pour réutiliser le port rapidement
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

  // 3. Boucle Principale
  while (1) {
    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);
    max_sd = server_fd;

    // Ajout des sockets clients au set
    for (int i = 0; i < MAX_CLIENTS; i++) {
      int sd = clients[i].socket;
      if (sd > 0)
        FD_SET(sd, &readfds);
      if (sd > max_sd)
        max_sd = sd;
    }

    // Attente d'activité (Select)
    int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

    if ((activity < 0) && (errno != EINTR)) {
      perror("Select error");
    }

    // A. Nouvelle connexion entrante
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

    // B. Activité sur un client existant (Message ou Auth)
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