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
  printf(" %-40s : Envoyer une image\n", "/image [chemin]");
  printf(" %-40s : Envoyer un fichier\n", "/sendfile [chemin]");
  printf(" %-40s : Historique\n", "Flèches Haut/Bas");
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
    char *first_close = strchr(msg, ']');
    if (first_close) {
        char *second_open = strchr(first_close + 1, '[');
        /**
         * @brief Vérifie si le format est probablement "[TIMESTAMP] [Utilisateur]".
         * Ceci est déterminé par une seconde parenthèse ouvrante apparaissant peu après la première fermante.
         */
        if (second_open && (second_open - first_close < 3)) {
            char *second_close = strchr(second_open, ']');
            if (second_close) {
                /** @brief Longueur de la chaîne timestamp, sans les crochets. */
                int ts_len = first_close - msg - 1;
                
                /** @brief Longueur de la chaîne utilisateur, avec les crochets. */
                int user_len = second_close - second_open + 1;
                /** @brief Pointeur vers le contenu réel du message après le tag utilisateur. */
                char *content = second_close + 1;
                
                /** @brief Longueur du nom d'utilisateur extrait, sans les crochets. */
                int name_len = user_len - 2;
                /** @brief Buffer pour stocker le nom d'utilisateur extrait pour comparaison. */
                char name_extracted[64];
                if (name_len > 63) name_len = 63;
                strncpy(name_extracted, second_open + 1, name_len);
                name_extracted[name_len] = '\0';
                
                /**
                 * @brief Drapeau indiquant si le message vient de l'utilisateur actuel ("Moi").
                 * Compare le nom extrait avec `current_username` ou le littéral "[Moi]".
                 */
                int is_me = 0;
                if (strlen(current_username) > 0 && strcmp(name_extracted, current_username) == 0) {
                    is_me = 1;
                }
                if (strncmp(second_open, "[Moi]", 5) == 0) is_me = 1;

                /** @brief Buffer pour stocker la chaîne timestamp formatée. */
                char ts_buf[32];
                struct tm ts_tm = {0};
                int year, month, day, hour, min, sec;
                /** @brief Parse le timestamp depuis la chaîne du message. */
                if (sscanf(msg + 1, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &min, &sec) == 6) {
                    ts_tm.tm_year = year - 1900; ts_tm.tm_mon = month - 1; ts_tm.tm_mday = day;
                    ts_tm.tm_hour = hour; ts_tm.tm_min = min; ts_tm.tm_sec = sec; ts_tm.tm_isdst = -1;
                    time_t ts_time = mktime(&ts_tm);
                    time_t now = time(NULL);
                    double diff = difftime(now, ts_time);
                    struct tm *now_tm = localtime(&now);
                    
                    /** @brief Formate le timestamp selon son ancienneté (aujourd'hui, hier, jours, semaines, mois). */
                    if (now_tm->tm_year == ts_tm.tm_year && now_tm->tm_mon == ts_tm.tm_mon && now_tm->tm_mday == ts_tm.tm_mday) {
                         snprintf(ts_buf, sizeof(ts_buf), "%02d:%02d", hour, min);
                    } else {
                         int days = (int)(diff / (60*60*24));
                         if (days == 0) snprintf(ts_buf, sizeof(ts_buf), "Hier");
                         else if (days == 1) snprintf(ts_buf, sizeof(ts_buf), "1 jour");
                         else if (days < 7) snprintf(ts_buf, sizeof(ts_buf), "%d jours", days);
                         else if (days < 30) snprintf(ts_buf, sizeof(ts_buf), "%d sem.", days / 7);
                         else snprintf(ts_buf, sizeof(ts_buf), ">1 mois");
                    }
                } else {
                    /** @brief Fallback si le parsing du timestamp échoue. */
                    snprintf(ts_buf, sizeof(ts_buf), "%.*s", (ts_len > 10 ? 10 : ts_len), msg + 1);
                }

                if (is_me) {
                    /** @brief Récupère la largeur du terminal pour l'alignement à droite. */
                    struct winsize w;
                    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0) {
                        w.ws_col = 80;
                    }
                    int term_width = w.ws_col;
                    
                    /** @brief Calcule la longueur du contenu, en retirant le saut de ligne final si présent. */
                    int content_len = strlen(content);
                    if (content_len > 0 && content[content_len-1] == '\n') content_len--;
                    
                    /** @brief Buffer pour le tag utilisateur affiché (ex: "[Moi]"). */
                    char display_user[64];
                    if (is_me) snprintf(display_user, 64, "[Moi]");
                    else snprintf(display_user, 64, "%.*s", user_len, second_open);
                    
                    /** @brief Longueur totale du message à afficher sur une ligne. */
                    int total_len = strlen(ts_buf) + 1 + strlen(display_user) + content_len;
                    
                    /** @brief Calcule la colonne cible pour l'alignement à droite. */
                    int target_col = term_width - total_len - 1; 
                    if (target_col < 1) target_col = 1;
                    
                    /** @brief Déplace le curseur à la colonne cible. */
                    printf("\033[%dG", target_col);
                    
                    /** @brief Affiche le message aligné à droite. */
                    printf(C_WHITE C_ITALIC "%s " C_RESET, ts_buf);
                    printf(C_GREEN C_BOLD "%s" C_RESET "%s", display_user, content);
                } else {
                    /** @brief Affiche le message aligné à gauche pour les autres utilisateurs. */
                    printf(C_WHITE C_ITALIC "%s " C_RESET, ts_buf);
                    printf(C_CYAN C_BOLD "%.*s" C_RESET "%s", user_len, second_open, content);
                }
                goto prompt_refresh;
            }
        }
    }

    /** @brief Fallback vers l'ancien format "[Utilisateur] Message" si le nouveau format n'est pas détecté. */
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

prompt_refresh:
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
