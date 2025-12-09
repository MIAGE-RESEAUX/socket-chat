#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>
#include <stdbool.h>

// Inclusion de vos modules (assurez-vous qu'ils existent)
#include "database/database.h"
#include "auth/auth.h"

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100

// Structure pour représenter un client et son état
typedef struct {
    int socket;
    bool authenticated; // false = phase auth, true = phase chat
    char username[64];
} ClientContext;

// Tableau global des clients
ClientContext clients[MAX_CLIENTS];

// --- Fonctions de gestion de la liste des clients ---

void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = 0;
        clients[i].authenticated = false;
        memset(clients[i].username, 0, 64);
    }
}

int ajouter_client(int sock) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == 0) {
            clients[i].socket = sock;
            clients[i].authenticated = false; // Par défaut, non authentifié
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
    snprintf(formatted_msg, sizeof(formatted_msg), "[%s] %s", clients[sender_index].username, message);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        // On envoie seulement si le socket existe, n'est pas l'expéditeur, et est authentifié
        if (clients[i].socket != 0 && i != sender_index && clients[i].authenticated) {
            send(clients[i].socket, formatted_msg, strlen(formatted_msg), 0);
        }
    }
}

// --- Logique Métier ---

// Traitement de l'authentification (LOGIN/SIGNUP)
void traiter_auth(int index, char *buffer) {
    char command[16], user[64], pass[64];
    bool success = false;
    int socket = clients[index].socket;

    // Parsing de la commande
    if (sscanf(buffer, "%15s %63s %63s", command, user, pass) != 3) {
        send_to_client(socket, "ERREUR: Format attendu (LOGIN user pass | SIGNUP user pass)\n");
        return;
    }

    if (strcmp(command, "LOGIN") == 0) {
        success = auth_login(user, pass);
        if (!success) send_to_client(socket, "ECHEC_AUTH: Login ou mdp incorrect.\n");
    } 
    else if (strcmp(command, "SIGNUP") == 0) {
        success = auth_signup(user, pass);
        if (!success) send_to_client(socket, "ECHEC_AUTH: Utilisateur existe deja ou erreur db.\n");
    } 
    else {
        send_to_client(socket, "ERREUR: Commande inconnue.\n");
        return;
    }

    // Si authentification réussie
    if (success) {
        clients[index].authenticated = true;
        strncpy(clients[index].username, user, 63);
        printf("[AUTH] %s connecté (Socket %d)\n", user, socket);
        
        // Ce message déclenche le passage en mode chat côté client
        send_to_client(socket, "SUCCES_SESSION: Bienvenue sur le chat.\n");
    }
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
        // Cas 2: Déjà connecté -> C'est un message de chat
        printf("[CHAT] %s: %s\n", clients[index].username, buffer);
        broadcast_message(buffer, index);
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
        fprintf(stderr, "ERREUR CRITIQUE: Impossible d'ouvrir la base de données.\n");
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

    printf("--- Serveur Multi-Client Auth+Chat démarré sur le port %d ---\n", port);

    // 3. Boucle Principale
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        // Ajout des sockets clients au set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].socket;
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // Attente d'activité (Select)
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            perror("Select error");
        }

        // A. Nouvelle connexion entrante
        if (FD_ISSET(server_fd, &readfds)) {
            socklen_t addrlen = sizeof(address);
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
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