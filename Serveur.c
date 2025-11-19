// Serveur a lancer avant les client

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
#define MAX_SOCKETS FD_SETSIZE

typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef fd_set ListeSockets;  // ensemble de sockets a surveiller

// Tableau des sockets clients (0 = vide)
int clients[MAX_SOCKETS];
int nbClients = 0;

// Ajoute un client
int ajouterClient(int sock) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (clients[i] == 0) {
            clients[i] = sock;
            nbClients++;
            return 0;
        }
    }
    return -1;
}

// Supprime un client suite a déconection
void supprimerClient(int sock) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (clients[i] == sock) {
            clients[i] = 0;
            nbClients--;
            return;
        }
    }
}

// Envoie un message a tous les clients sauf l'expediteur
void broadcast(const char *message, int len, int expediteur) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (clients[i] != 0 && clients[i] != expediteur) {
            write(clients[i], message, len);
        }
    }
}

// Initialisation du serveur
int creerServeur(void) {
    int sock;
    sockaddr_in adresse;

	// Initialise un socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Erreur socket");
        exit(1);
    }

    // Permet de reutiliser le port immediatement apres fermeture
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // set up l'addresse
  	memset(&adresse, 0, sizeof(adresse));  // init à zéro
  	adresse.sin_family = AF_INET;          // IPv4
  	adresse.sin_addr.s_addr = INADDR_ANY;  // toutes addresse / interfaces
    // adresse.sin_addr.s_addr = inet_addr("127.0.0.1"); // pour forcer une addresse spécifique (localhost ou autres)
  	adresse.sin_port = htons(PORT);        // port 5000

	// attribue l'addresse au socket
    if (bind(sock, (sockaddr *)&adresse, sizeof(adresse)) < 0) {
        perror("Erreur bind");
        exit(1);
    }

    // Démarre l'écoute pour des tentative de connection au serveur par les clients
    listen(sock, 5);

    return sock;
}

// Accepte une nouvelle connexion
void accepterConnexion(int serveur) {
    sockaddr_in adresse;
    socklen_t len = sizeof(adresse);
    int nouveau;

    nouveau = accept(serveur, (sockaddr *)&adresse, &len);
    if (nouveau < 0) {
        perror("Erreur accept");
        return;
    }

    if (ajouterClient(nouveau) < 0) {
        printf("Serveur plein\n");
        close(nouveau);
    } else {
        printf("Client connecte (%d)\n", nbClients);
    }
}

// Traite un message d'un client
void traiterMessage(int client) {
    char buffer[BUFFER_SIZE];
    int n;

    n = read(client, buffer, BUFFER_SIZE - 1);

    if (n <= 0) {
        printf("Client deconnecte (%d)\n", nbClients - 1);
        close(client);
        supprimerClient(client);
        return;
    }

    buffer[n] = '\0';

    // Regarde si commande /quit
    if (strncmp(buffer, "/quit", 5) == 0) {
        printf("Client a quitte (%d)\n", nbClients - 1);
        close(client);
        supprimerClient(client);
        return;
    }

    printf("%s", buffer);
    broadcast(buffer, n, client);
}

// Boucle principale
void boucleServeur(int serveur) {
    ListeSockets socketsActifs;

    printf("Serveur demarre sur port %d\n", PORT);

    for (;;) {
        // Reinitialiser la liste des sockets a surveiller
        FD_ZERO(&socketsActifs);
        FD_SET(serveur, &socketsActifs);

        // Ajouter tous les clients connectes
        for (int i = 0; i < MAX_SOCKETS; i++) {
            if (clients[i] != 0)
                FD_SET(clients[i], &socketsActifs);
        }

        // Attendre qu'un socket ait de l'activite
        if (select(MAX_SOCKETS, &socketsActifs, NULL, NULL, NULL) < 0) {
            perror("Erreur select");
            continue;
        }

        // Nouvelle connexion ?
        if (FD_ISSET(serveur, &socketsActifs))
            accepterConnexion(serveur);

        // Message d'un client ?
        for (int i = 0; i < MAX_SOCKETS; i++) {
            if (clients[i] != 0 && FD_ISSET(clients[i], &socketsActifs))
                traiterMessage(clients[i]);
        }
    }
}

int main(int argc, char **argv) {
    int serveur = creerServeur();
    boucleServeur(serveur);
    return 0;
}
