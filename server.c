#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "database/database.h"
#include "auth/auth.h"

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_PENDING_CONNECTIONS 5

typedef struct {
    char command[16];   // LOGIN, SIGNUP
    char username[64];
    char password[64];
} auth_request_t;

void send_response(int socket_fd, const char *message) {
    send(socket_fd, message, strlen(message), 0);
}

int parse_request(const char *buffer, auth_request_t *request) {
    if (sscanf(buffer, "%15s %63s %63s",
               request->command,
               request->username,
               request->password) != 3) {
        return -1;
    }
    return 0;
}

void handle_user_session(int client_socket, const char *username) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t valread;

    printf("[SESSION] %s connecté.\n", username);
    send_response(client_socket, "SUCCES_SESSION: Connecté.\n");

    while ((valread = read(client_socket, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[valread] = '\0';
        printf("[MESSAGE %s] %s\n", username, buffer);

        char echo_msg[BUFFER_SIZE + 10];
        snprintf(echo_msg, sizeof(echo_msg), "ECHO: %s", buffer);
        send_response(client_socket, echo_msg);
        memset(buffer, 0, BUFFER_SIZE);
    }

    printf("[SESSION] %s déconnecté.\n", username);
}

void handle_auth_request(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t valread;
    auth_request_t request;
    bool authenticated = false;

    while (!authenticated) {
        memset(buffer, 0, BUFFER_SIZE);
        valread = read(client_socket, buffer, BUFFER_SIZE - 1);

        if (valread <= 0) {
            close(client_socket);
            return;
        }

        buffer[valread] = '\0';
        printf("[AUTH] Requête: %s\n", buffer);

        if (parse_request(buffer, &request) != 0) {
            send_response(client_socket, "ERREUR: Format (LOGIN/SIGNUP user pass).\n");
            continue;
        }

        bool success = false;

        if (strcmp(request.command, "LOGIN") == 0) {
            success = auth_login(request.username, request.password);
            // 💡 MODIFICATION : On envoie un message SEULEMENT si ça échoue
            if (!success) {
                send_response(client_socket, "ECHEC_AUTH: Login invalide.\n");
            }
        }
        else if (strcmp(request.command, "SIGNUP") == 0) {
            success = auth_signup(request.username, request.password);
            // 💡 MODIFICATION : Idem, silence si succès, message si erreur
            if (!success) {
                 send_response(client_socket, "ECHEC_AUTH: Erreur inscription.\n");
            }
        }
        else {
            send_response(client_socket, "ERREUR: Commande inconnue.\n");
        }

        if (success) {
            authenticated = true;
            // C'est cette fonction qui va envoyer "SUCCES_SESSION..."
            // Le client recevra donc directement le bon signal.
            handle_user_session(client_socket, request.username);
        }
    }
    
    close(client_socket);
}


int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    printf("Database db %s",DATABASE_PATH);

    if (!db_open(DATABASE_PATH)) {
        fprintf(stderr, "[SERVER] ERREUR: Impossible d'ouvrir SQLite.\n");
        exit(1);
    }
    printf("[SERVER] Base SQLite chargée.\n");

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket"); exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind"); exit(1);
    }

    if (listen(server_fd, MAX_PENDING_CONNECTIONS) < 0) {
        perror("listen"); exit(1);
    }

    printf("Serveur Auth+Chat en C (SQLite) sur port %d. PID=%d\n", PORT, getpid());

    while (1) {
        printf("En attente de connexion...\n");

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept"); continue;
        }

        printf("[CONNEXION] %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        handle_auth_request(new_socket);
    }

    db_close();
    close(server_fd);
    return 0;
}