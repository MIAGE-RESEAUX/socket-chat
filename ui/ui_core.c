/**
 * @file ui_core.c
 * @brief Fonctions de base de l'interface utilisateur.
 *
 * Gère le nettoyage de l'écran et la configuration du terminal (termios).
 */

#include "ui_shared.h"
#include <sys/ioctl.h>

/** @brief Mutex pour synchroniser l'affichage et éviter les mélanges de texte. */
pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;

/** @brief Stocke le nom de l'utilisateur courant pour l'affichage (ex: "Moi"). */
char current_username[64] = "";

/**
 * @brief Efface l'écran du terminal.
 * Utilise les séquences ANSI pour repositionner le curseur en haut à gauche et effacer l'écran.
 */
void ui_clear_screen() {
  printf("\033[H\033[J");
  fflush(stdout);
}

/**
 * @brief Affiche la bannière ASCII du projet.
 * Efface l'écran avant l'affichage.
 */
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
         "v3.0 - Final" C_RESET "\n\n");
}

/**
 * @brief Active ou désactive le mode brut (raw mode) du terminal.
 * Le mode brut permet de lire les entrées caractère par caractère sans attendre "Entrée"
 * et désactive l'écho local.
 *
 * @param enable 1 pour activer, 0 pour désactiver (restaurer les paramètres d'origine).
 */
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

/**
 * @brief Définit la zone de défilement du terminal.
 * Utilise la séquence DECSTBM (Set Top and Bottom Margins).
 *
 * @param top Ligne du haut (1-based).
 * @param bottom Ligne du bas (1-based). Si 0, utilise toute la hauteur.
 */
void ui_set_scroll_region(int top, int bottom) {
  struct winsize w;
  ioctl(STDIN_FILENO, TIOCGWINSZ, &w);
  int height = w.ws_row;

  if (bottom == 0)
    bottom = height;

  printf("\033[%d;%dr", top, bottom);
  fflush(stdout);
}

/**
 * @brief Réinitialise la zone de défilement à l'écran entier.
 */
void ui_reset_scroll_region() {
  printf("\033[r");
  fflush(stdout);
}
