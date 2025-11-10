# socket-chat

**Ingénierie des Réseaux**
M1 MIAGE 2025-2026

Projet de communication client-serveur basique utilisant les sockets, écrit en language C.

## Compilation

```bash
gcc Serveur.c -o serveur
gcc Client.c -o client
```

## Utilisation

**Démarrer le serveur :**
```bash
./serveur
```

**Démarrer un client :**
```bash
./client <adresse-serveur> <message>
```

**Exemple :**
```bash
./client localhost "Bonjour serveur"
```

## Développement

**Formater le code :**
```bash
clang-format -i *.c
```
