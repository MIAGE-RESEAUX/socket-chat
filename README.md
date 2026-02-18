#### V1 Multi Client/Serveur

- Faire un serveur qui écoute sur un socket et renvoie un message à tout les clients connectés
- Faire un client qui envoie un message sur un socket et écoute une réponse, le client passe un username qu'il choisi lors de la connexion
- Rajouter un peu d'ui côté utilisateur
#### V2 Multi client/Serveur
- Connexion / Inscription (si utilisateur inexistant inscription automatique en fonction du username), mise en place de la BD.
- Rajouter des chats privés via un code unique généré par le serveur. (ex `#4353`)
- Mettre à jour l'UI en fonction des nouvelles implémentations

#### V3 Informations serveur + Partage de fichiers / images + Historique
- Rajouter un attribut privé / public sur les canaux Lister les canaux disponibles en public 
- Lister les utilisateurs disponibles
- Partager un fichier pdf, txt ou png pour un client donnée
- Mettre à jour l'ui en conséquence.
- Historique des messages



## Installation et Compilation

### Prérequis
Pour compiler et exécuter le projet, vous avez besoin de :
- **GCC** (Compilateur C)
- **Make** (Outil de build)
- **SQLite3** (Base de données) et ses bibliothèques de développement.

### Installer SQLite3

#### macOS (Homebrew)
```bash
brew update
brew install sqlite
```

#### Linux

##### Debian / Ubuntu
```bash
sudo apt update
sudo apt install sqlite3 libsqlite3-dev
```


Vérifier l'installation :
```bash
sqlite3 --version
```

### Compilation
Un `Makefile` est fourni pour simplifier la compilation.

1.  **Nettoyer le projet** (supprime les exécutables et fichiers objets) :
    ```bash
    make clean
    ```

2.  **Compiler le projet** (génère `server` et `client`) :
    ```bash
    make
    ```
    *Note : Cette commande initialise aussi la base de données si elle n'existe pas via `make initdb`.*

3.  **Réinitialiser la base de données** (⚠️ Supprime toutes les données !) :
    ```bash
    make db-reset
    ```

---

## Utilisation

### 1. Lancer le Serveur
Le serveur gère les connexions, la base de données et le transfert de messages.

```bash
./server [port]
```
- **Port par défaut** : `8080` (si aucun argument n'est fourni).
- **Exemple** : 
  ```bash
  ./server 8888
  ```

### 2. Lancer le Client
Le client permet de se connecter au serveur et de chatter. Lancez plusieurs clients dans des terminaux séparés pour simuler une conversation.

```bash
./client [adresse_ip] [port]
```
- **Adresse IP par défaut** : `127.0.0.1` (localhost).
- **Port par défaut** : `8080`.
- **Exemple** :
  ```bash
  ./client 127.0.0.1 8888
  ```
