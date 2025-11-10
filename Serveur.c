/*----------------------------------------------
Serveur TCP - À lancer avant le client
Écoute sur le port 5000 et traite les messages reçus
------------------------------------------------*/

/* Bibliothèques nécessaires pour la programmation réseau */
#include <linux/types.h> /* types de données pour sockets */
#include <netdb.h>       /* structures hostent, servent */
#include <stdio.h>       /* printf, perror */
#include <stdlib.h>      /* exit, malloc */
#include <string.h>      /* strlen, bcopy */
#include <sys/socket.h>  /* socket, bind, listen, accept */
#include <unistd.h>      /* read, write, close, sleep, gethostname */

#define TAILLE_MAX_NOM 256
#define PORT_SERVEUR 5000
#define TAILLE_BUFFER 256

/* Alias de types pour simplifier la syntaxe */
typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct hostent hostent;
typedef struct servent servent;

/*------------------------------------------------------
 * Fonction : renvoi
 * Rôle : Traite le message reçu d'un client et renvoie une réponse
 *
 * Paramètre :
 *   - sock : descripteur de la socket connectée au client
 *
 * Fonctionnement :
 *   1. Lit le message du client
 *   2. Modifie le début du message (remplace par "RE")
 *   3. Ajoute un marqueur '#' à la fin
 *   4. Renvoie le message traité au client
 *------------------------------------------------------*/
void
renvoi (int sock)
{
  char buffer[TAILLE_BUFFER];
  int longueur;

  /* Lecture du message depuis la socket
   * read() bloque jusqu'à réception de données
   * Retourne le nombre d'octets lus, ou ≤0 si erreur/fin de connexion */
  if ((longueur = read (sock, buffer, sizeof (buffer))) <= 0)
    return;

  printf ("Message recu du client : %s \n", buffer);

  /* Traitement du message : modification du début */
  buffer[0] = 'R';
  buffer[1] = 'E';
  buffer[longueur] = '#';      /* Ajout d'un marqueur de fin */
  buffer[longueur + 1] = '\0'; /* Terminaison de chaîne C */

  printf ("Message apres traitement : %s \n", buffer);
  printf ("Envoi de la reponse au client...\n");

  /* Simulation d'un délai de traitement (3 secondes) */
  sleep (3);

  /* Écriture de la réponse sur la socket
   * write() envoie les données au client connecté */
  write (sock, buffer, strlen (buffer) + 1);
  printf ("Reponse envoyee avec succes.\n");
}

/*------------------------------------------------------
 * Fonction principale du serveur
 *
 * Cycle de vie du serveur TCP :
 *   1. Création de la socket (socket)
 *   2. Liaison à une adresse/port (bind)
 *   3. Mise en écoute (listen)
 *   4. Acceptation des connexions (accept)
 *   5. Communication avec le client (read/write)
 *   6. Fermeture de la connexion (close)
 *------------------------------------------------------*/
int
main (int argc, char **argv)
{
  /* Descripteurs de socket
   * - socket_descriptor : socket d'écoute principale
   * - nouv_socket_descriptor : socket créée pour chaque client connecté */
  int socket_descriptor;
  int nouv_socket_descriptor;

  /* Taille de la structure d'adresse du client */
  socklen_t longueur_adresse_courante;

  /* Structures d'adresse
   * sockaddr_in contient : famille d'adresses, port, adresse IP */
  sockaddr_in adresse_locale;         /* Adresse du serveur */
  sockaddr_in adresse_client_courant; /* Adresse du client connecté */

  /* Informations sur l'hôte et le service */
  hostent *ptr_hote;
  servent *ptr_service;
  char machine[TAILLE_MAX_NOM + 1];

  /* Suppression des warnings pour variables non utilisées */
  (void)argc;
  (void)argv;
  (void)ptr_service;

  printf ("=== DEMARRAGE DU SERVEUR TCP ===\n\n");

  /* ÉTAPE 1 : Récupération du nom de la machine locale */
  gethostname (machine, TAILLE_MAX_NOM);
  printf ("Nom de la machine : %s\n", machine);

  /* Résolution du nom d'hôte en adresse IP
   * gethostbyname() interroge le DNS ou /etc/hosts */
  if ((ptr_hote = gethostbyname (machine)) == NULL)
    {
      perror ("Erreur : impossible de resoudre le nom d'hote");
      exit (1);
    }

  /* ÉTAPE 2 : Configuration de la structure d'adresse du serveur */

  /* Copie de l'adresse IP de l'hôte dans la structure */
  bcopy ((char *)ptr_hote->h_addr, (char *)&adresse_locale.sin_addr,
         ptr_hote->h_length);

  /* Famille d'adresses : AF_INET = IPv4 */
  adresse_locale.sin_family = ptr_hote->h_addrtype;

  /* INADDR_ANY = 0.0.0.0 : écoute sur toutes les interfaces réseau
   * Permet d'accepter des connexions depuis n'importe quelle interface */
  adresse_locale.sin_addr.s_addr = INADDR_ANY;

  /* Configuration du port d'écoute du serveur
   * Deux méthodes possibles (commenter l'une ou l'autre) : */

  /*-----------------------------------------------------------*/
  /* SOLUTION 1 : Utiliser un service réseau standard existant
   * Exemple avec "irc" (Internet Relay Chat) qui utilise le port 6667
   * getservbyname() recherche dans /etc/services */
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
   * Port 5000 : port non privilégié (>1024), pas de droits root nécessaires
   * htons() = Host TO Network Short : convertit en format big-endian */
  adresse_locale.sin_port = htons (PORT_SERVEUR);
  /*-----------------------------------------------------------*/

  printf ("Port d'ecoute : %d\n\n", ntohs (adresse_locale.sin_port));

  /* ÉTAPE 3 : Création de la socket
   * - AF_INET : protocole IPv4
   * - SOCK_STREAM : socket TCP (connexion fiable, orientée flux)
   * - 0 : protocole par défaut pour SOCK_STREAM (TCP)
   * Retourne un descripteur de fichier (comme open()) */
  if ((socket_descriptor = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
      perror ("Erreur : creation de la socket impossible");
      exit (1);
    }
  printf ("Socket creee avec succes (descripteur %d)\n", socket_descriptor);

  /* ÉTAPE 4 : Liaison de la socket à l'adresse et au port (bind)
   * Associe la socket au port 5000 sur toutes les interfaces
   * Nécessaire pour qu'un client puisse nous trouver */
  if ((bind (socket_descriptor, (sockaddr *)(&adresse_locale),
             sizeof (adresse_locale)))
      < 0)
    {
      perror ("Erreur : liaison (bind) de la socket impossible");
      exit (1);
    }
  printf ("Socket liee a l'adresse %s:%d\n", "0.0.0.0", PORT_SERVEUR);

  /* ÉTAPE 5 : Mise en écoute (listen)
   * Prépare la socket à accepter des connexions entrantes
   * Paramètre 5 : taille de la file d'attente (max 5 connexions en attente) */
  listen (socket_descriptor, 5);
  printf ("Serveur en ecoute... (file d'attente : 5 connexions max)\n\n");

  /* ÉTAPE 6 : Boucle infinie d'acceptation de connexions
   * Le serveur traite les clients un par un (séquentiel, pas de
   * multithreading)
   */
  for (;;)
    {
      printf ("--- En attente d'un nouveau client ---\n");

      longueur_adresse_courante = sizeof (adresse_client_courant);

      /* accept() bloque jusqu'à l'arrivée d'un client
       * Crée une NOUVELLE socket dédiée à ce client
       * La socket d'origine continue d'écouter pour les prochains clients
       * adresse_client_courant sera remplie avec l'IP et le port du client */
      if ((nouv_socket_descriptor
           = accept (socket_descriptor, (sockaddr *)(&adresse_client_courant),
                     &longueur_adresse_courante))
          < 0)
        {
          perror ("Erreur : impossible d'accepter la connexion");
          exit (1);
        }

      printf ("Client connecte ! (descripteur %d)\n", nouv_socket_descriptor);

      /* ÉTAPE 7 : Traitement de la requête du client */
      renvoi (nouv_socket_descriptor);

      /* ÉTAPE 8 : Fermeture de la connexion avec ce client
       * La socket d'écoute principale reste ouverte */
      close (nouv_socket_descriptor);
      printf ("Connexion avec le client fermee.\n\n");

      /* Le serveur retourne à l'étape 6 et attend un nouveau client */
    }

  /* Note : ce code n'est jamais atteint (boucle infinie)
   * Dans un vrai serveur, il faudrait gérer l'arrêt propre avec des signaux */
}
