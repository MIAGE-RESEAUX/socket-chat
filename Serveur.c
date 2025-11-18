/*-----------------------------------------------------------
Serveur à lancer avant le client avec la commande :
serveur
------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <linux/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct hostent hostent;
typedef struct servent servent;

int main(int argc, char **argv) {
    int socket_descriptor;              /* descripteur de socket */
    int nouv_socket_descriptor;         /* [nouveau] descripteur de socket */
    socklen_t longueur;                 /* longueur d'un buffer utilisé */
    sockaddr_in adresse_locale;         /* adresse de socket local */
    hostent *ptr_hote;                  /* les infos recuperees sur la machine */
    servent *ptr_service;               /* les infos recuperees sur le service de la machine */
    char buffer[256];
    char *mesg = "Communication réussie";
    char *prog;                         /* nom du programme */

    if (argc != 1) {
        perror("usage : serveur");
        exit(1);
    }

    prog = argv[0];

    printf("nom de l'executable : %s \n", prog);

    if ((ptr_hote = gethostbyname("localhost")) == NULL) {
        perror("erreur : impossible de trouver le serveur a partir de son nom.");
        exit(1);
    }

    /* copie caractere par caractere des infos de ptr_hote vers adresse_locale */
    bcopy((char*)ptr_hote->h_addr, (char*)&adresse_locale.sin_addr, ptr_hote->h_length);
    adresse_locale.sin_family = ptr_hote->h_addrtype;    /* ou AF_INET */
    adresse_locale.sin_addr.s_addr = INADDR_ANY;         /* ou AF_INET */

    /* 2 facons de definir le service que l'on va utiliser a distance (commenter l'une ou l'autre des solutions) */
    /*-----------------------------------------------------------*/
    /* SOLUTION 1 : utiliser un service existant, par ex. "irc" */
    /*
    if ((ptr_service = getservbyname("irc","tcp")) == NULL) {
        perror("erreur : impossible de recuperer le numero de port du service desire.");
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
        perror("erreur : impossible de creer la socket de connexion avec le client.");
        exit(1);
    }

    /* association du socket socket_descriptor à la structure d'adresse adresse_locale */
    if ((bind(socket_descriptor, (sockaddr*)(&adresse_locale), sizeof(adresse_locale))) < 0) {
        perror("erreur : impossible de lier la socket a l'adresse de connexion.");
        exit(1);
    }

    /* initialisation de la file d'ecoute */
    listen(socket_descriptor, 5);

    /* attente des connexions et traitement des donnees recues */
    for(;;) {
        longueur = sizeof(adresse_locale);

        /* adresse_locale sera renseigné par accept via les infos du connect */
        if ((nouv_socket_descriptor = accept(socket_descriptor, (sockaddr*)(&adresse_locale), &longueur)) < 0) {
            perror("erreur : impossible d'accepter la connexion avec le client.");
            exit(1);
        }

        /* traitement du message */
        printf("message du client : \n");

        int bytes_read;
        while((bytes_read = read(nouv_socket_descriptor, buffer, sizeof(buffer))) > 0) {
            write(1, buffer, bytes_read);
        }

        printf("\nenvoi d'un message au client \n");

        /* envoi du message vers le client */
        if ((write(nouv_socket_descriptor, mesg, strlen(mesg))) < 0) {
            perror("erreur : impossible d'ecrire le message destine au client.");
            exit(1);
        }

        close(nouv_socket_descriptor);
    }
}