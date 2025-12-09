-- Schema SQLite pour la table des utilisateurs
-- À exécuter une seule fois lors de l'initialisation de la base

CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    hash TEXT NOT NULL
);
