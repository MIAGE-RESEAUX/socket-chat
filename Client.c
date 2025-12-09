// Client a lancer apres le serveur avec la commande :
// client <adresse-serveur>

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#define PORT 5000
#define BUFFER_SIZE 256

typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct hostent hostent;
typedef fd_set ListeSockets;  // ensemble de sockets a surveiller

// Affiche une erreur et quitte
void erreur(const char *msg) {
    perror(msg);
    exit(1);
}

// Verifie si l'utilisateur veut quitter
int isQuit(const char *buffer) {
    return strncmp(buffer, "/quit", 5) == 0;
}

// Envoie un message formate avec le username
int envoyerMessage(int sock, const char *username, const char *buffer) {
    char message[512];
    snprintf(message, sizeof(message), "[%s] %s", username, buffer);
    return write(sock, message, strlen(message));
}

// Connexion au serveur
int connecter(const char *host) {
    int sock;
    sockaddr_in adresse;
    hostent *serveur;

    serveur = gethostbyname(host);
    if (serveur == NULL)
        erreur("Serveur introuvable");

    memset(&adresse, 0, sizeof(adresse));
    adresse.sin_family = AF_INET;
    adresse.sin_port = htons(PORT);
    memcpy(&adresse.sin_addr, serveur->h_addr, serveur->h_length);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        erreur("Erreur socket");

    if (connect(sock, (sockaddr *)&adresse, sizeof(adresse)) < 0)
        erreur("Connexion impossible");

    return sock;
}

// Boucle principale du chat
void boucleChat(int sock, const char *username) {
    char buffer[BUFFER_SIZE];
    ListeSockets socketsActifs;
    int n;

    printf("Connecte! (/quit pour quitter)\n");

    for (;;) {
        // Surveiller le clavier (0 = stdin) et le serveur
        FD_ZERO(&socketsActifs);
        FD_SET(0, &socketsActifs);      // clavier
        FD_SET(sock, &socketsActifs);   // serveur

        // Attendre une activite
        if (select(FD_SETSIZE, &socketsActifs, NULL, NULL, NULL) < 0) {
            perror("Erreur select");
            return;
        }

        // Message du serveur ?
        if (FD_ISSET(sock, &socketsActifs)) {
            n = read(sock, buffer, BUFFER_SIZE - 1);
            if (n <= 0) {
                printf("\nDeconnecte.\n");
                return;
            }
            buffer[n] = '\0';
            printf("%s", buffer);
            fflush(stdout);
        }

        // Entree clavier ?
        if (FD_ISSET(0, &socketsActifs)) {
            if (fgets(buffer, BUFFER_SIZE, stdin) == NULL)
                return;

            if (isQuit(buffer)) {
                write(sock, "/quit", 5);
                return;
            }

            envoyerMessage(sock, username, buffer);
        }
    }
}

int main(int argc, char **argv) {
    int sock;
    char username[32];

    // au lancement on doit avoir 2 arg (le programe "./client" et l'addr "localhost"
    if (argc != 2) {
        printf("Usage: %s <serveur>\n", argv[0]);
        exit(1);
    }

    printf("Connexion a %s...\n", argv[1]);
    sock = connecter(argv[1]);
    printf("Connecte!\n");

    printf("Votre nom: ");
    fflush(stdout);
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = '\0';

    boucleChat(sock, username);

    close(sock);
    return 0;
}
