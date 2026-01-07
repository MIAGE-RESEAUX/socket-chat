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

// --- Fonctions utilitaires pour transfert de fichiers ---

#define MAX_FILE_SIZE 10485760  // 10 MB

// Extensions de fichiers autorisées
const char *ALLOWED_EXTENSIONS[] = {".jpg", ".jpeg", ".png", ".pdf", ".txt",
                                    NULL};

// Extraire l'extension d'un fichier
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

// Valider l'extension du fichier
int validate_file_extension(const char *ext) {
  for (int i = 0; ALLOWED_EXTENSIONS[i] != NULL; i++) {
    if (strcmp(ext, ALLOWED_EXTENSIONS[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

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
// Utilise un protocole length-prefixed (bonne pratique réseau)
void broadcast_file(const char *filename, const char *extension,
                    unsigned char *file_data, uint32_t file_size,
                    int sender_index) {
  char header[BUFFER_SIZE];
  const char *file_end = "FILE_END\n";

  // Formatage du header : FILE|filename|filesize|extension|username
  snprintf(header, sizeof(header), "FILE|%s|%u|%s|%s\n", filename, file_size,
           extension, clients[sender_index].username);

  // Calculer la taille totale du message
  uint32_t header_len = strlen(header);
  uint32_t total_size = header_len + file_size + strlen(file_end);

  int current_channel = clients[sender_index].channel_id;

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].socket != 0 && i != sender_index &&
        clients[i].authenticated && clients[i].channel_id == current_channel) {

      // 1. Envoyer la taille totale (network byte order - bonne pratique)
      uint32_t net_size = htonl(total_size);
      send(clients[i].socket, &net_size, sizeof(net_size), 0);

      // 2. Envoyer le header
      send(clients[i].socket, header, header_len, 0);

      // 3. Envoyer les données binaires
      send(clients[i].socket, file_data, file_size, 0);

      // 4. Envoyer le marqueur de fin
      send(clients[i].socket, file_end, strlen(file_end), 0);
    }
  }

  printf("[FILE_TRANSFER] %s a broadcasté %s (%u bytes) dans le canal %d\n",
         clients[sender_index].username, filename, file_size, current_channel);
}
// FILE_TRANSFER: fin

// --- Callbacks Base de Données ---

int send_hist_cb(void *ctx, int argc, char **argv, char **col) {
  (void)col;
  int index = *(int *)ctx;
  if (argc >= 3) {
    char hist_msg[BUFFER_SIZE];
    // argv[0]=username, argv[1]=content, argv[2]=timestamp
    snprintf(hist_msg, sizeof(hist_msg), "[%s] %s", argv[0], argv[1]);
    if (clients[index].socket > 0) {
      send(clients[index].socket, hist_msg, strlen(hist_msg), 0);
      usleep(1000);
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
      send_to_client(
          socket,
          "SUCCES_INSCRIPTION: Vous pouvez maintenant vous connecter.\n");
    } else {
      send_to_client(socket,
                     "ECHEC_AUTH: Utilisateur existe deja ou erreur db.\n");
    }
    // Do NOT set success = true here. We want explicit LOGIN after signup.
    return;
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
    send_to_client(socket,
                   "SUCCES_SESSION: Bienvenue sur le chat. Canal: Global\n");

    // Send history callback
    db_get_history(1, 50, send_hist_cb, &index);
  }
}

// Callback for listing channels
int send_channel_list_cb(void *ctx, int argc, char **argv, char **col) {
  (void)col;
  int socket = *(int *)ctx;
  if (argc >= 2) {
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
    // Vérifier si c'est un transfert de fichier (commence par 4 bytes = taille)
    // On détecte cela si le premier recv a reçu exactement 4 bytes ou si buffer
    // commence par des bytes binaires
    if (n >= 4) {
      // Essayer de lire comme une taille (network byte order)
      uint32_t potential_size;
      memcpy(&potential_size, buffer, 4);
      uint32_t message_size = ntohl(potential_size);

      // Si la taille est raisonnable (< 11 MB), c'est probablement un fichier
      if (message_size > 0 && message_size < (MAX_FILE_SIZE + 1024)) {
        // C'est un transfert de fichier !

        // Allouer mémoire pour le message complet
        unsigned char *full_message = malloc(message_size);
        if (!full_message) {
          send_to_client(sock, "Erreur: Mémoire insuffisante.\n");
          return;
        }

        // Copier ce qui a déjà été reçu après les 4 bytes de taille
        int already_received = n - 4;
        if (already_received > 0) {
          memcpy(full_message, buffer + 4, already_received);
        }

        // Recevoir le reste du message
        uint32_t total_received = already_received;
        while (total_received < message_size) {
          int nr = recv(sock, full_message + total_received,
                        message_size - total_received, 0);
          if (nr <= 0) {
            free(full_message);
            printf("[FILE_TRANSFER] Erreur réception\n");
            supprimer_client(index);
            return;
          }
          total_received += nr;
        }

        // Parser le header dans le message complet
        char filename[256];
        char extension[10];
        uint32_t file_size;
        if (sscanf((char *)full_message, "FILE|%255[^|]|%u|%9s", filename,
                   &file_size, extension) == 3) {

          // Valider l'extension
          if (!validate_file_extension(extension)) {
            send_to_client(sock,
                           "Erreur: Type de fichier non autorisé (jpg, jpeg, "
                           "png, pdf, txt uniquement).\n");
            free(full_message);
            return;
          }

          // Valider la taille
          if (file_size > MAX_FILE_SIZE) {
            send_to_client(sock, "Erreur: Fichier trop gros (max 10 MB).\n");
            free(full_message);
            return;
          }

          // Trouver le début des données (après le \n du header)
          char *data_start = strchr((char *)full_message, '\n');
          if (data_start) {
            data_start++; // Sauter le \n

            printf("[FILE_TRANSFER] Reçu %s (%u bytes) de %s\n", filename,
                   file_size, clients[index].username);

            // Broadcaster le fichier
            broadcast_file(filename, extension, (unsigned char *)data_start,
                           file_size, index);
          }
        }

        free(full_message);
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
          send_to_client(sock, "Usage: /create [nom_canal] [public/private] "
                               "[mdp (si private)]\n");
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
            // Validate directly with ID
            if (db_validate_channel_password(cid, pass)) {
              clients[index].channel_id = cid;
              send_to_client(sock, "Vous avez rejoint le canal.\n");

              db_get_history(cid, 50, send_hist_cb, &index);

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
        send_to_client(sock, "--- Canaux Publics ---\n");
        db_list_public_channels(send_channel_list_cb, &sock);
        send_to_client(sock, "----------------------\n");
      } else if (strcmp(cmd, "/users") == 0) {
        char msg[BUFFER_SIZE];
        int cid = clients[index].channel_id;
        snprintf(msg, sizeof(msg), "--- Utilisateurs (Canal %d) ---\n", cid);
        send_to_client(sock, msg);

        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (clients[i].socket != 0 && clients[i].authenticated &&
              clients[i].channel_id == cid) {
            snprintf(msg, sizeof(msg), "- %s\n", clients[i].username);
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