/*-----------------------------------------------------------
Client a lancer apres le serveur avec la commande :
client <adresse-serveur>
------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct hostent hostent;
typedef struct servent servent;

/* Envoie un message et reçoit la réponse - retourne 0 si ok, -1 si erreur */
int send_message(int socket, const char *message, int len) {
    char buffer[256];
    int bytes_read;

    /* envoi du message */
    if (write(socket, message, len) < 0) {
        perror("erreur : impossible d'envoyer le message");
        return -1;
    }

    /* lecture de la réponse */
    if ((bytes_read = read(socket, buffer, sizeof(buffer))) > 0) {
        printf("serveur: ");
        fflush(stdout);
        write(1, buffer, bytes_read);
    }

    return 0;
}

/* Boucle principale de communication */
int chat_loop(int socket) {
    char buffer[256];

    printf("Connecté. Tapez vos messages (/quit pour quitter)\n");

    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        int len = strlen(buffer);

        /* vérification de la commande /quit */
        if (strncmp(buffer, "/quit", 5) == 0) {
            write(socket, "/quit", 5);
            return 0;
        }

        if (send_message(socket, buffer, len) < 0) {
            return -1;
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    int socket_descriptor; /* descripteur de socket */
    sockaddr_in adresse_locale; /* adresse de socket local */
    hostent * ptr_host; /* info sur une machine hote */
    servent * ptr_service; /* info sur service */
    char * prog; /* nom du programme */
    char * host; /* nom de la machine distante */
    if (argc != 2) {
        perror("usage : client <adresse-serveur>");
        exit(1);
    }
    prog = argv[0];
    host = argv[1];
    printf("nom de l'executable : %s \n", prog);
    printf("adresse du serveur : %s \n", host);
    if ((ptr_host = gethostbyname(host)) == NULL) {perror("erreur : impossible de trouver le serveur a partir de son adresse.");
        exit(1);
    }
/* copie caractere par caractere des infos de ptr_host vers adresse_locale */
    bcopy((char*)ptr_host->h_addr, (char*)&adresse_locale.sin_addr,
          ptr_host->h_length);
    adresse_locale.sin_family = AF_INET; /* ou ptr_host->h_addrtype; */
/* 2 facons de definir le service que l'on va utiliser a distance */
/* (commenter l'une ou l'autre des solutions) */
/*-----------------------------------------------------------*/
/* SOLUTION 1 : utiliser un service existant, par ex. "irc" */
/*
if ((ptr_service = getservbyname("irc","tcp")) == NULL) {
perror("erreur : impossible de recuperer le numero de port du service
desire.");
exit(1);
}
adresse_locale.sin_port = htons(ptr_service->s_port);
*/
/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/
/* SOLUTION 2 : utiliser un nouveau numero de port */
    adresse_locale.sin_port = htons(5000);
/*-----------------------------------------------------------*/
    printf("numero de port pour la connexion au serveur : %d \n",
           ntohs(adresse_locale.sin_port));
/* creation de la socket */
    if ((socket_descriptor = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("erreur : impossible de creer la socket de connexion avec le serveur.");
        exit(1);
    }
/* tentative de connexion au serveur dont les infos sont dans
adresse_locale */
    if ((connect(socket_descriptor, (sockaddr*)(&adresse_locale),
                 sizeof(adresse_locale))) < 0) {
        perror("erreur : impossible de se connecter au serveur.");
        exit(1);
    }
    printf("connexion etablie avec le serveur.\n");

    /* boucle de communication */
    chat_loop(socket_descriptor);

    close(socket_descriptor);
    printf("connexion avec le serveur fermee, fin du programme.\n");
    exit(0);
}