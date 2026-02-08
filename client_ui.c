#include "client_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

char input_buffer[BUFFER_SIZE];
int input_len = 0;
pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;

static char history[HISTORY_MAX][BUFFER_SIZE];
static int history_count = 0;
static int history_pos = 0;

void ui_clear_screen() {
  printf("\033[H\033[J");
  fflush(stdout);
}

void ui_show_banner() {
  ui_clear_screen();
  printf(C_CYAN C_BOLD);
  printf("   ______  __    __       ___   .___________.      _______.  ______  "
         "   ______  __  ___  _______ .___________.\n");
  printf("  /      ||  |  |  |     /   \\  |           |     /       | /  __  "
         "\\   /      ||  |/  / |   ____||           |\n");
  printf(" |  ,----'|  |__|  |    /  ^  \\ `---|  |----`    |   (----`|  |  |  "
         "| |  ,----'|  '  /  |  |__   `---|  |----`\n");
  printf(" |  |     |   __   |   /  /_\\  \\    |  |          \\   \\    |  |  "
         "|  | |  |     |    <   |   __|      |  |     \n");
  printf(" |  `----.|  |  |  |  /  _____  \\   |  |      .----)   |   |  `--'  "
         "| |  `----.|  .  \\  |  |____     |  |     \n");
  printf("  \\______||__|  |__| /__/     \\__\\  |__|      |_______/     "
         "\\______/   \\______||__|\\__\\ |_______|    |__|     \n");
  printf("\n" C_RESET);
  printf("                                      " C_ITALIC
         "v1.0 - Connected Edition" C_RESET "\n\n");
}

void ui_set_raw_mode(int enable) {
  static struct termios oldt, newt;
  if (enable) {
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  } else {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  }
}

void ui_refresh_prompt() {
  pthread_mutex_lock(&print_lock);
  printf("\r\033[K" C_BLUE C_BOLD " ➤ Saisie " C_RESET "> %s", input_buffer);
  fflush(stdout);
  pthread_mutex_unlock(&print_lock);
}

void ui_history_add(const char *cmd) {
  if (strlen(cmd) == 0)
    return;

  if (history_count < HISTORY_MAX) {
    strcpy(history[history_count], cmd);
    history_count++;
  } else {
    for (int i = 0; i < HISTORY_MAX - 1; i++) {
      strcpy(history[i], history[i + 1]);
    }
    strcpy(history[HISTORY_MAX - 1], cmd);
  }
  history_pos = history_count;
}

void ui_history_replace_buffer(const char *new_text) {
  memset(input_buffer, 0, BUFFER_SIZE);
  strcpy(input_buffer, new_text);
  input_len = strlen(input_buffer);
  ui_refresh_prompt();
}

void ui_history_up() {
  if (history_count == 0)
    return;

  if (history_pos > 0) {
    history_pos--;
    ui_history_replace_buffer(history[history_pos]);
  }
}

void ui_history_down() {
  if (history_count == 0)
    return;

  if (history_pos < history_count) {
    history_pos++;
    if (history_pos == history_count) {
      ui_history_replace_buffer("");
    } else {
      ui_history_replace_buffer(history[history_pos]);
    }
  }
}

// /create [nom_canal] [public/private]

// /join [nom du canal]

// /leave // leaves the current canal to go back to the main canal

// /delete [nom canal] // only if admin of cannal (creator by default)

void ui_print_help() {
  pthread_mutex_lock(&print_lock);
  printf("\r\033[K");
  printf(C_YELLOW " --- AIDE ---\n" C_RESET);
  printf(" /quit      : Quitter\n");
  printf(" /commandes : Ce menu\n");
  printf(" /create [nom_canal] [public/private] : Créer un canal \n");
  printf(" /join [id_canal] : Rejoindre un canal \n");
  printf(" /list      : Lister les canaux publics\n");
  printf(" /users     : Utilisateurs connectés au canal\n");
  printf(" /sendfile [path] : Envoyer un fichier (jpg, png, pdf, txt)\n");
  printf(" /image [path] : Afficher une image (locale)\n");
  printf(" /leave     : Quitter le canal\n");
  printf(" Flèches    : Historique\n");
  printf(C_BLUE C_BOLD " ➤ Saisie " C_RESET "> %s", input_buffer);
  fflush(stdout);
  pthread_mutex_unlock(&print_lock);
}

void ui_print_pretty_msg(const char *msg) {
  pthread_mutex_lock(&print_lock);
  printf("\r\033[K");

  if (strncmp(msg, "!!!", 3) == 0 || strstr(msg, "ERREUR")) {
    printf(C_RED C_BOLD " ⚠ %s" C_RESET "\n", msg);
  } else if (msg[0] == '[') {
    char *end = strchr(msg, ']');
    if (end) {
      int len = end - msg + 1;
      if (strncmp(msg, "[Moi]", 5) == 0) {
        printf(C_GREEN C_BOLD "%.*s" C_RESET "%s\n", len, msg, end + 1);
      } else {
        printf(C_CYAN C_BOLD "%.*s" C_RESET "%s\n", len, msg, end + 1);
      }
    } else {
      printf("%s\n", msg);
    }
  } else {
    printf(C_WHITE "%s" C_RESET "\n", msg);
  }

  printf(C_BLUE C_BOLD " ➤ Saisie " C_RESET "> %s", input_buffer);
  fflush(stdout);
  pthread_mutex_unlock(&print_lock);
}

void ui_print_char(char ch) {
  pthread_mutex_lock(&print_lock);
  putchar(ch);
  fflush(stdout);
  pthread_mutex_unlock(&print_lock);
}

void ui_delete_char() {
  if (input_len > 0) {
    input_buffer[--input_len] = '\0';
    ui_refresh_prompt();
  }
}

void ui_reset_input() {
  memset(input_buffer, 0, BUFFER_SIZE);
  input_len = 0;
}