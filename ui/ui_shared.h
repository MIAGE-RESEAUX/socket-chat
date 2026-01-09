#ifndef UI_SHARED_H
#define UI_SHARED_H

#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

#define BUFFER_SIZE 1024
#define HISTORY_MAX 50

// --- COULEURS ---
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

// Variables partagées
extern char input_buffer[BUFFER_SIZE];
extern int input_len;
extern int cursor_pos; // Nouvelle variable pour la position du curseur
extern pthread_mutex_t print_lock;
extern char current_username[64];

// Fonctions Core (ui_core.c)
void ui_clear_screen();
void ui_set_raw_mode(int enable);
void ui_show_banner();

// Fonctions Display (ui_display.c)
void ui_print_pretty_msg(const char *msg);
void ui_print_help();
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
