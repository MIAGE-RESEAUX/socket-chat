# socket-chat


#### V1 Client/Serveur simple

- Faire un serveur qui écoute sur un socket et renvoie un message à tout les clients connectés
- Faire un client qui envoie un message sur un socket et écoute une réponse
- Rajouter un peu d'ui côté utilisateur

#### V2 Multi client/Serveur
- Connexion / Inscription (si utilisateur inexistant inscription automatique en fonction du username), mise en place de la BD.
- Rajouter la gestion du multi client côté serveur toujours en broadcast
- Rajouter la gestion du multi client côté client avec affichage des usernames sur le chat global
- Rajouter des chats privés via un code unique généré par le serveur. (ex `#4353`)
- Mettre à jour l'UI en fonction des nouvelles implémentations
#### V3 Historique
- Rajouter un historique de chat sauvé en Base avec un TTL de 3jour. (json par conv)
- Consultation de l'historique via des commandes.
#### V4 Partage de fichiers
- partager un fichier pdf, txt ou png pour un client donnée
- mettre à jour l'ui en conséquence.
