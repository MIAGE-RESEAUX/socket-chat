/**
 * @file ui_display.c
 * @brief Fonctions d'affichage de l'interface utilisateur.
 *
 * Gère l'affichage des messages, du prompt, de l'aide et de l'en-tête du canal.
 * Utilise des mutex pour éviter les conflits d'affichage entre threads.
 */

#include "ui_shared.h"
#include <time.h>
#include <sys/ioctl.h>

/**
 * @brief Rafraîchit la ligne de prompt (saisie).
 * Réaffiche le prompt et positionne le curseur correctement.
 */
void ui_refresh_prompt() {
  pthread_mutex_lock(&print_lock);
  printf("\r\033[K" C_BLUE C_BOLD " ➤ Saisie " C_RESET "> %s", input_buffer);

  if (input_len > cursor_pos) {
      printf("\033[%dD", input_len - cursor_pos);
  }
  
  fflush(stdout);
  pthread_mutex_unlock(&print_lock);
}

/**
 * @brief Affiche l'en-tête du canal (bannière ASCII, titre, aide courte).
 * Efface l'écran avant d'afficher.
 *
 * @param channel_id ID du canal actuel.
 */
void ui_print_channel_header(int channel_id) {
  pthread_mutex_lock(&print_lock);

  printf("\033[H\033[J");

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

  if (channel_id <= 1) {
    printf(C_GREEN C_BOLD " --- CHAT ACTIF (Canal: Général) --- " C_RESET "\n");
  } else {
      printf(C_MAGENTA C_BOLD " --- CHAT ACTIF (Canal: #%d) --- " C_RESET "\n", channel_id);
  }

  printf(C_WHITE C_ITALIC " (/commandes pour l'aide, Flèches pour historique)" C_RESET "\n");
  printf("--------------------------------------------------------------------------------\n");

  printf(C_BLUE C_BOLD " ➤ Saisie " C_RESET "> %s", input_buffer);
  if (input_len > cursor_pos) {
    printf("\033[%dD", input_len - cursor_pos);
  }

  fflush(stdout);
  pthread_mutex_unlock(&print_lock);
}

/**
 * @brief Affiche le menu d'aide avec la liste des commandes disponibles.
 * Ne nettoie pas l'écran entier, mais redessine la zone de message.
 */
void ui_print_help() {
  pthread_mutex_lock(&print_lock);
  printf("\r\033[K");
  printf(C_YELLOW " --- AIDE ---\n" C_RESET);
  printf(" %-40s : Quitter\n", "/quit");
  printf(" %-40s : Ce menu\n", "/commandes");
  printf(" %-40s : Créer un canal\n", "/create [nom] [public/private]");
  printf(" %-40s : Rejoindre un canal\n", "/join [id]");
  printf(" %-40s : Lister les canaux\n", "/list");
  printf(" %-40s : Utilisateurs connectés\n", "/users");
  printf(" %-40s : Quitter le canal\n", "/leave");
  printf(" %-40s : Afficher une image (locale)\n", "/image [path]");
  printf(" %-40s : Envoyer un fichier (jpg, png, pdf, txt)\n", "/sendfile [path]");
  printf(" %-40s : Voir l'historique\n", "/history");
  printf(" %-40s : Historique (Navigation)\n", "Flèches Haut/Bas");
  printf(" %-40s : Navigation curseur\n", "Flèches Gauche/Droite");

  printf(C_BLUE C_BOLD " ➤ Saisie " C_RESET "> %s", input_buffer);
  if (input_len > cursor_pos) {
    printf("\033[%dD", input_len - cursor_pos);
  }
  fflush(stdout);
  pthread_mutex_unlock(&print_lock);
}

/**
 * @brief Affiche un message formaté dans le chat.
 * Gère les couleurs, les alertes, et l'alignement (droite pour "Moi", gauche pour les autres).
 * Tente de parser le timestamp et l'utilisateur pour un affichage enrichi.
 *
 * @param msg Le message brut à afficher.
 */
void ui_print_pretty_msg(const char *msg) {
  pthread_mutex_lock(&print_lock);
  printf("\r\033[K");

  if (strncmp(msg, "!!!", 3) == 0 || strstr(msg, "ERREUR")) {
    printf(C_RED C_BOLD " ⚠ %s" C_RESET "\n", msg);
  } else if (msg[0] == '[') {
    // Tentative de parsing du format complet : [TIMESTAMP] [USER] MESSAGE
    char *first_close = strchr(msg, ']');
    if (first_close) {
        char *second_open = strchr(first_close, '[');
        // On vérifie si on a bien un deuxième bloc encadré par des crochets (le user)
        if (second_open && (second_open - first_close) <= 2) {
             char *second_close = strchr(second_open, ']');
             if (second_close) {
                 // Extraction du nom d'utilisateur
                 int user_len = second_close - second_open - 1;
                 char username[64];
                 if (user_len > 63) user_len = 63;
                 strncpy(username, second_open + 1, user_len);
                 username[user_len] = '\0';

                 // Vérification si c'est l'utilisateur courant
                 int is_me = 0;
                 if (strlen(current_username) > 0 && strcmp(username, current_username) == 0) is_me = 1;
                 if (strcmp(username, "Moi") == 0) is_me = 1;

                 // Affiche le Timestamp (Cyan/Blanc)
                 int ts_len = first_close - msg + 1;
                 printf(C_WHITE C_ITALIC "%.*s " C_RESET, ts_len, msg);

                 // Affiche le User et le message
                 char *content = second_close + 1;
                 if (is_me) {
                     // Cas "Moi" : Vert et nom remplacé
                     printf(C_GREEN C_BOLD "[Moi]" C_RESET "%s\n", content);
                 } else {
                     // Cas autres : Cyan et nom d'origine
                     printf(C_CYAN C_BOLD "[%s]" C_RESET "%s\n", username, content);
                 }
                 goto prompt_refresh;
             }
        }
    }

    // Fallback : Format simple [User] Message (ou si parsing échoue)
    char *end = strchr(msg, ']');
    if (end) {
      int len = end - msg + 1;
      char *content = end + 1;
      
      // On regarde si c'est [Moi]
      if (strncmp(msg, "[Moi]", 5) == 0) {
        printf(C_GREEN C_BOLD "%.*s" C_RESET "%s\n", len, msg, content);
      } else {
        printf(C_CYAN C_BOLD "%.*s" C_RESET "%s\n", len, msg, content);
      }
    } else {
      printf("%s\n", msg);
    }
  } else {
    // Message système ou autre
    printf(C_WHITE "%s" C_RESET "\n", msg);
  }

prompt_refresh:
  // Réaffichage propre du prompt
  printf(C_BLUE C_BOLD " ➤ Saisie " C_RESET "> %s", input_buffer);
  if (input_len > cursor_pos) {
      printf("\033[%dD", input_len - cursor_pos);
  }
  fflush(stdout);
  pthread_mutex_unlock(&print_lock);
}

/**
 * @brief Affiche un caractère à l'écran (fonction utilitaire, peu utilisée directement).
 * @param ch Le caractère à afficher.
 */
void ui_print_char(char ch) {
    (void)ch;
}
