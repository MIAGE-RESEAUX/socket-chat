#ifndef CLIENT_UI_H
#define CLIENT_UI_H

#include <pthread.h>

#define BUFFER_SIZE 1024
#define HISTORY_MAX 50

#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_ITALIC  "\033[3m"
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_MAGENTA "\033[35m"
#define C_CYAN    "\033[36m"
#define C_WHITE   "\033[37m"

extern char input_buffer[BUFFER_SIZE];
extern int input_len;
extern pthread_mutex_t print_lock;

void ui_clear_screen();
void ui_show_banner();
void ui_set_raw_mode(int enable);
void ui_refresh_prompt();
void ui_print_pretty_msg(const char *msg);
void ui_print_char(char ch);
void ui_delete_char();
void ui_reset_input();
void ui_print_help();
void ui_history_add(const char *cmd);
void ui_history_up();
void ui_history_down();

#endif