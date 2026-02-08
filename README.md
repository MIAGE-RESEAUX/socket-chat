#### V1 Multi Client/Serveur

- Faire un serveur qui écoute sur un socket et renvoie un message à tout les clients connectés
- Faire un client qui envoie un message sur un socket et écoute une réponse, le client passe un username qu'il choisi lors de la connexion
- Rajouter un peu d'ui côté utilisateur
#### V2 Multi client/Serveur
- Connexion / Inscription (si utilisateur inexistant inscription automatique en fonction du username), mise en place de la BD.
- Rajouter des chats privés via un code unique généré par le serveur. (ex `#4353`)
- Mettre à jour l'UI en fonction des nouvelles implémentations

#### V3 Informations serveur + Partage de fichiers / images
- Rajouter un attribut privé / public sur les canaux Lister les canaux disponibles en public 
- Lister les utilisateurs disponibles
- Partager un fichier pdf, txt ou png pour un client donnée
- Mettre à jour l'ui en conséquence.



#### Prérequis : 
1. Installer le driver mongodb : 
- MACOS : `brew install mongo-c-driver`
- Linux : `sudo apt install libmongoc-dev``
