#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>

#define PORT 8080        // Port par défaut (celui du code Auth)
#define BUFFER_SIZE 1024

// --- Fonctions Utilitaires ---

void erreur(const char *msg) {
    perror(msg);
    exit(1);
}

// Fonction de connexion (supporte IP ou nom de domaine/localhost)
int connecter_au_serveur(const char *hostname, int port) {
    int sock;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    // Création socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        erreur("Erreur création socket");
    }

    // Résolution du nom d'hôte (ex: "localhost" ou "127.0.0.1")
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "Erreur: Hôte introuvable\n");
        exit(0);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);

    // Tentative de connexion
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        erreur("Connexion échouée");
    }

    return sock;
}

// --- Phase 1 : Authentification ---
void phase_authentification(int sock) {
    char buffer[BUFFER_SIZE];
    
    printf("--- Client connecté au serveur Auth/Chat ---\n");
    printf("Commandes: LOGIN user pass  |  SIGNUP user pass\n");

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        printf("> ");
        fflush(stdout);

        // Lecture clavier
        if (!fgets(buffer, BUFFER_SIZE, stdin)) {
            break;
        }
        buffer[strcspn(buffer, "\n")] = '\0'; // Retirer le \n

        // Envoi au serveur
        if (send(sock, buffer, strlen(buffer), 0) < 0) {
            erreur("Erreur d'envoi");
        }

        // Attente réponse (Blocant ici car on ne peut pas chatter sans auth)
        memset(buffer, 0, BUFFER_SIZE);
        int valread = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        
        if (valread <= 0) {
            printf("Connexion fermée par le serveur.\n");
            exit(1);
        }

        buffer[valread] = '\0';
        printf("[SERVEUR] %s\n", buffer);

        // Vérification du succès (protocole défini dans ton code 1)
        if (strncmp(buffer, "SUCCES_SESSION", 14) == 0) {
            printf("\n💡 Authentification réussie ! Entrée dans le chat...\n");
            printf("---------------------------------------------------\n");
            return; // On sort de la fonction pour aller au chat
        }
    }
}

// --- Phase 2 : Chat Asynchrone (Multiplexé) ---
void phase_chat(int sock) {
    char buffer[BUFFER_SIZE];
    fd_set sockets_actifs; // Liste des descripteurs de fichiers à surveiller
    
    printf("Tapez votre message ou /quit pour quitter.\n");

    while (1) {
        // 1. Réinitialiser la liste des sources à écouter
        FD_ZERO(&sockets_actifs);
        FD_SET(STDIN_FILENO, &sockets_actifs); // Écouter le clavier (entrée standard)
        FD_SET(sock, &sockets_actifs);         // Écouter le serveur (réseau)

        // 2. Attendre qu'il se passe quelque chose (bloquant jusqu'à activité)
        if (select(sock + 1, &sockets_actifs, NULL, NULL, NULL) < 0) {
            perror("Erreur select");
            break;
        }

        // 3. Cas A : Message reçu du SERVEUR
        if (FD_ISSET(sock, &sockets_actifs)) {
            memset(buffer, 0, BUFFER_SIZE);
            int n = recv(sock, buffer, BUFFER_SIZE - 1, 0);
            
            if (n <= 0) {
                printf("\nDéconnecté par le serveur.\n");
                break;
            }
            buffer[n] = '\0';
            // On affiche simplement le message (le serveur gère le format "[User] msg")
            printf("%s\n", buffer); 
            printf("message> "); // Réaffiche le prompt pour garder l'interface propre
            fflush(stdout);
        }

        // 4. Cas B : Saisie au CLAVIER
        if (FD_ISSET(STDIN_FILENO, &sockets_actifs)) {
            memset(buffer, 0, BUFFER_SIZE);
            if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) break;
            
            buffer[strcspn(buffer, "\n")] = '\0'; // Retirer le saut de ligne

            if (strcmp(buffer, "/quit") == 0) {
                printf("Déconnexion volontaire...\n");
                break;
            }

            // Envoi du message brut (le serveur sait qui on est grâce à l'auth préalable)
            send(sock, buffer, strlen(buffer), 0);
            
            // Petit effet visuel pour dire "j'ai envoyé"
            printf("\033[1A"); // Remonte le curseur (optionnel, pour style)
            printf("\033[K");  // Efface la ligne
            printf("[Moi] %s\n", buffer);
            printf("message> ");
            fflush(stdout);
        }
    }
}

int main(int argc, char **argv) {
    int sock;
    char *hostname = "127.0.0.1"; // Hôte par défaut
    int port = PORT;              // Port par défaut (8080)

    // Gestion des arguments : ./client [host] [port]
    if (argc >= 2) {
        hostname = argv[1];
    }
    if (argc >= 3) {
        port = atoi(argv[2]);
    }

    printf("Tentative de connexion à %s sur le port %d...\n", hostname, port);
    // 1. Connexion (avec les nouveaux paramètres)
    sock = connecter_au_serveur(hostname, port);
    // 2. Authentification
    phase_authentification(sock);
    // 3. Chat
    phase_chat(sock);

    close(sock);
    return 0;
}