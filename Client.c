/*-----------------------------------------------------------
Client simple pour le serveur Auth+Chat en C (SQLite)
------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erreur socket");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Adresse invalide");
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connexion échouée");
        exit(1);
    }

    printf("--- Client connecté au serveur Auth/Chat ---\n");
    printf("Commandes: LOGIN user pass  |  SIGNUP user pass\n");

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        printf("> ");
        fflush(stdout);

        if (!fgets(buffer, BUFFER_SIZE, stdin)) {
            printf("Fin entrée\n");
            break;
        }

        buffer[strcspn(buffer, "\n")] = '\0';

        if (send(sock, buffer, strlen(buffer), 0) < 0) {
            perror("Erreur send");
            break;
        }

        memset(buffer, 0, BUFFER_SIZE);

        int valread = read(sock, buffer, BUFFER_SIZE - 1);
        if (valread <= 0) {
            printf("Connexion fermée par le serveur.\n");
            break;
        }

        buffer[valread] = '\0';
        printf("[SERVEUR] %s\n", buffer);

        if (strncmp(buffer, "SUCCES_SESSION", 15) == 0) break;
    }

    printf("--- Session de chat ---\n");

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        printf("message> "); fflush(stdout);

        if (!fgets(buffer, BUFFER_SIZE, stdin)) break;
        buffer[strcspn(buffer, "\n")] = '\0';

        if (strcmp(buffer, "/quit") == 0) {
            printf("Déconnexion...\n");
            break;
        }

        send(sock, buffer, strlen(buffer), 0);

        memset(buffer, 0, BUFFER_SIZE);
        int valread = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (valread <= 0) break;

        buffer[valread] = '\0';
        printf("[SERVEUR] %s\n", buffer);
    }

    close(sock);
    return 0;
}
