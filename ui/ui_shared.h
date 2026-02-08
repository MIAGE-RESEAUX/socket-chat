#ifndef UI_SHARED_H
#define UI_SHARED_H

/**
 * @file ui_shared.h
 * @brief Définitions et prototypes partagés pour l'interface utilisateur.
 *
 * Contient les constantes de couleurs ANSI, les structures partagées
 * et les prototypes des fonctions d'UI.
 */

#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

#define BUFFER_SIZE 1024  /**< Taille commune du buffer */
#define HISTORY_MAX 50    /**< Nombre max de commandes dans l'historique */

// --- COULEURS ANSI ---
#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_CYAN    "\033[36m"
#define C_MAGENTA "\033[35m"
#define C_WHITE   "\033[37m"
#define C_ITALIC  "\033[3m"

// Variables partagées (définies dans client.c ou un module source)
extern char input_buffer[BUFFER_SIZE]; /**< Buffer d'entrée courant */
extern int input_len;                 /**< Longueur actuelle de l'entrée */
extern int cursor_pos;                /**< Position du curseur dans l'entrée */
extern pthread_mutex_t print_lock;    /**< Mutex pour l'affichage thread-safe (non utilisé actuellement) */
extern char current_username[64];     /**< Nom de l'utilisateur connecté */

// Fonctions Core (ui_core.c)
/**
 * @brief Efface l'écran du terminal.
 */
void ui_clear_screen();
/**
 * @brief Configure le terminal en mode "raw" ou "cooked".
 * @param enable 1 pour activer le mode raw (pas d'echo, lecture caractère par caractère), 0 pour désactiver.
 */
void ui_set_raw_mode(int enable);
/**
 * @brief Affiche la bannière ASCII du chat.
 */
void ui_show_banner();

// Fonctions Display (ui_display.c)
/**
 * @brief Affiche un message formaté proprement, en préservant la ligne de prompt.
 * @param msg Message à afficher.
 */
void ui_print_pretty_msg(const char *msg);
void ui_print_help();
/**
 * @brief Réaffiche la ligne de prompt (AUTH> ou CHAT>).
 */
void ui_refresh_prompt();
void ui_print_char(char ch);
void ui_print_channel_header(int channel_id);

// Fonctions Input (ui_input.c)
void ui_reset_input();
void ui_delete_char();
void ui_insert_char(char ch);
void ui_move_cursor_left();
void ui_move_cursor_right();
void ui_history_add(const char *cmd);
void ui_history_up();
void ui_history_down();

#endif
