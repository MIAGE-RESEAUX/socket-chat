/*-----------------------------------------------------------
Client TCP - À lancer après le serveur
Usage : ./client <adresse-serveur> <message-a-transmettre>
Exemple : ./client localhost "Bonjour serveur"
------------------------------------------------------------*/

/* Bibliothèques nécessaires pour la programmation réseau */
#include <netdb.h>      /* gethostbyname, structures hostent/servent */
#include <stdio.h>      /* printf, perror */
#include <stdlib.h>     /* exit */
#include <string.h>     /* strlen, bcopy */
#include <sys/socket.h> /* socket, connect */
#include <sys/types.h>  /* types système */
#include <unistd.h>     /* read, write, close, sleep */

#define PORT_SERVEUR 5000
#define TAILLE_BUFFER 256

/* Alias de types pour simplifier la syntaxe */
typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct hostent hostent;
typedef struct servent servent;

/*------------------------------------------------------
 * Fonction principale du client
 *
 * Cycle de vie du client TCP :
 *   1. Création de la socket (socket)
 *   2. Connexion au serveur (connect)
 *   3. Envoi de données (write)
 *   4. Réception de la réponse (read)
 *   5. Fermeture de la connexion (close)
 *------------------------------------------------------*/
int
main (int argc, char **argv)
{
  /* Descripteur de socket : identifiant unique de la connexion */
  int socket_descriptor;

  /* Longueur des données lues */
  int longueur;

  /* Structure d'adresse du serveur distant
   * Contient : famille d'adresses (IPv4), port, adresse IP */
  sockaddr_in adresse_locale;

  /* Informations sur l'hôte et le service */
  hostent *ptr_host;
  servent *ptr_service;

  /* Buffer de réception */
  char buffer[TAILLE_BUFFER];

  /* Arguments du programme */
  char *prog; /* Nom de l'exécutable */
  char *host; /* Adresse du serveur (IP ou nom d'hôte) */
  char *mesg; /* Message à envoyer */

  /* ÉTAPE 0 : Vérification des arguments */
  if (argc != 3)
    {
      perror ("Usage : client <adresse-serveur> <message-a-transmettre>");
      exit (1);
    }

  /* Récupération des arguments de la ligne de commande */
  prog = argv[0]; /* argv[0] = nom du programme */
  host = argv[1]; /* argv[1] = adresse du serveur */
  mesg = argv[2]; /* argv[2] = message à envoyer */

  /* Suppression du warning pour variable non utilisée */
  (void)ptr_service;

  printf ("=== CLIENT TCP ===\n");
  printf ("Programme : %s\n", prog);
  printf ("Serveur cible : %s\n", host);
  printf ("Message a envoyer : %s\n\n", mesg);

  /* ÉTAPE 1 : Résolution du nom d'hôte en adresse IP
   * gethostbyname() peut accepter :
   *   - Un nom d'hôte : "localhost", "www.example.com"
   *   - Une adresse IP : "127.0.0.1", "192.168.1.1"
   * Il interroge le DNS ou le fichier /etc/hosts */
  if ((ptr_host = gethostbyname (host)) == NULL)
    {
      perror ("Erreur : impossible de resoudre l'adresse du serveur");
      exit (1);
    }
  printf ("Resolution DNS reussie.\n");

  /* ÉTAPE 2 : Configuration de la structure d'adresse du serveur */

  /* Copie de l'adresse IP dans la structure
   * bcopy() = copie mémoire byte par byte (ancienne fonction POSIX) */
  bcopy ((char *)ptr_host->h_addr, (char *)&adresse_locale.sin_addr,
         ptr_host->h_length);

  /* Famille d'adresses : AF_INET = protocole IPv4 */
  adresse_locale.sin_family = AF_INET;

  /* Configuration du port de destination
   * Deux méthodes possibles (commenter l'une ou l'autre) : */

  /*-----------------------------------------------------------*/
  /* SOLUTION 1 : Utiliser un service réseau standard existant
   * Exemple avec "irc" (Internet Relay Chat) qui utilise le port 6667
   * getservbyname() recherche dans /etc/services
   * Le client doit utiliser le MÊME port que le serveur ! */
  /*
  if ((ptr_service = getservbyname("irc","tcp")) == NULL) {
    perror("Erreur : impossible de recuperer le numero de port du service
  desire."); exit(1);
  }
  adresse_locale.sin_port = htons(ptr_service->s_port);
  */
  /*-----------------------------------------------------------*/

  /*-----------------------------------------------------------*/
  /* SOLUTION 2 : Utiliser un numéro de port personnalisé
   * Port 5000 : doit correspondre au port sur lequel le serveur écoute
   * htons() = Host TO Network Short : convertit en format big-endian
   * Exemple : 5000 (0x1388) → 0x8813 en mémoire (little-endian) */
  adresse_locale.sin_port = htons (PORT_SERVEUR);
  /*-----------------------------------------------------------*/

  printf ("Port de connexion : %d\n\n", ntohs (adresse_locale.sin_port));

  /* ÉTAPE 3 : Création de la socket
   * - AF_INET : protocole IPv4
   * - SOCK_STREAM : socket TCP (connexion fiable, bi-directionnelle, ordonnée)
   * - 0 : protocole par défaut (TCP)
   * Retourne un descripteur de fichier (entier positif) */
  if ((socket_descriptor = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
      perror ("Erreur : creation de la socket impossible");
      exit (1);
    }
  printf ("Socket creee avec succes (descripteur %d)\n", socket_descriptor);

  /* ÉTAPE 4 : Connexion au serveur (connect)
   * Établit une connexion TCP avec le serveur (handshake à 3 voies) :
   *   1. Client → Serveur : SYN
   *   2. Serveur → Client : SYN-ACK
   *   3. Client → Serveur : ACK
   * Cette fonction BLOQUE jusqu'à ce que la connexion soit établie ou échoue
   */
  printf ("Tentative de connexion au serveur %s:%d...\n", host, PORT_SERVEUR);
  if ((connect (socket_descriptor, (sockaddr *)(&adresse_locale),
                sizeof (adresse_locale)))
      < 0)
    {
      perror ("Erreur : connexion au serveur impossible");
      close (socket_descriptor);
      exit (1);
    }
  printf ("Connexion etablie avec le serveur !\n\n");

  /* ÉTAPE 5 : Envoi du message au serveur (write)
   * write() écrit des données dans la socket
   * Les données sont automatiquement encapsulées dans des paquets TCP */
  printf ("Envoi du message au serveur...\n");
  if ((write (socket_descriptor, mesg, strlen (mesg))) < 0)
    {
      perror ("Erreur : envoi du message impossible");
      close (socket_descriptor);
      exit (1);
    }

  /* Simulation d'un délai de transmission (3 secondes) */
  sleep (3);
  printf ("Message envoye avec succes.\n\n");

  /* ÉTAPE 6 : Réception de la réponse du serveur (read)
   * read() bloque jusqu'à réception de données ou fermeture de la connexion
   * Retourne :
   *   - > 0 : nombre d'octets lus
   *   - 0 : connexion fermée par le serveur
   *   - < 0 : erreur */
  printf ("En attente de la reponse du serveur...\n");
  while ((longueur = read (socket_descriptor, buffer, sizeof (buffer))) > 0)
    {
      printf ("Reponse recue du serveur :\n");
      /* write(1, ...) = écriture sur la sortie standard (stdout) */
      write (1, buffer, longueur);
    }

  printf ("\n\nReception terminee.\n");

  /* ÉTAPE 7 : Fermeture de la connexion
   * Envoie un segment TCP FIN pour terminer proprement la connexion */
  close (socket_descriptor);
  printf ("Connexion fermee. Fin du programme.\n");

  exit (0);
}
