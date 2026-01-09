#include "ui_shared.h"
#include <time.h>
#include <sys/ioctl.h>

void ui_refresh_prompt() {
  pthread_mutex_lock(&print_lock);
  // Clear line and reprint prompt
  printf("\r\033[K" C_BLUE C_BOLD " ➤ Saisie " C_RESET "> %s", input_buffer);

  // Correction position curseur
  if (input_len > cursor_pos) {
      printf("\033[%dD", input_len - cursor_pos);
  }
  
  fflush(stdout);
  pthread_mutex_unlock(&print_lock);
}

void ui_print_channel_header(int channel_id) {
  pthread_mutex_lock(&print_lock);
  
  // Simple Clear & Print
  printf("\033[H\033[J"); // Clear everything

  // Print Banner
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
         "v2.0 - Canaux" C_RESET "\n\n");

  // HEADER
  if (channel_id <= 1) {
      printf(C_GREEN C_BOLD " --- CHAT ACTIF (Canal: Général) --- " C_RESET "\n");
  } else {
      printf(C_MAGENTA C_BOLD " --- CHAT ACTIF (Canal: #%d) --- " C_RESET "\n", channel_id);
  }
  
  // SHORT HELP
  printf(C_WHITE C_ITALIC " (/commandes pour l'aide, Flèches pour historique)" C_RESET "\n");
  printf("--------------------------------------------------------------------------------\n");

  // Reprint prompt at normal position (end of stream)
  printf(C_BLUE C_BOLD " ➤ Saisie " C_RESET "> %s", input_buffer);
  if (input_len > cursor_pos) {
      printf("\033[%dD", input_len - cursor_pos);
  }
  
  fflush(stdout);
  pthread_mutex_unlock(&print_lock);
}

void ui_print_help() {
  pthread_mutex_lock(&print_lock);
  printf("\r\033[K");
  printf(C_YELLOW " --- AIDE ---\n" C_RESET);
  // Using fixed width -35 for command, then colon description
  printf(" %-40s : Quitter\n", "/quit");
  printf(" %-40s : Ce menu\n", "/commandes");
  printf(" %-40s : Créer un canal\n", "/create [nom] [public/private]");
  printf(" %-40s : Rejoindre un canal\n", "/join [id]");
  printf(" %-40s : Lister les canaux\n", "/list");
  printf(" %-40s : Utilisateurs connectés\n", "/users");
  printf(" %-40s : Quitter le canal\n", "/leave");
  printf(" %-40s : Historique\n", "Flèches Haut/Bas");
  printf(" %-40s : Navigation curseur\n", "Flèches Gauche/Droite");
  
  // Reprint prompt
  printf(C_BLUE C_BOLD " ➤ Saisie " C_RESET "> %s", input_buffer);
  if (input_len > cursor_pos) {
      printf("\033[%dD", input_len - cursor_pos);
  }
  fflush(stdout);
  pthread_mutex_unlock(&print_lock);
}

// Helper to format timestamp
void print_smart_timestamp(const char *ts_str, int len) {
    if (len < 10) {
        printf(C_WHITE C_ITALIC "%.*s " C_RESET, len, ts_str);
        return;
    }

    // Parse timestamp
    struct tm ts_tm = {0};
    
    // Support formats: YYYY-MM-DD HH:MM:SS
    // We only care about date parts really for relative check
    int year, month, day, hour, min, sec;
    if (sscanf(ts_str, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &min, &sec) == 6) {
        ts_tm.tm_year = year - 1900;
        ts_tm.tm_mon = month - 1;
        ts_tm.tm_mday = day;
        ts_tm.tm_hour = hour;
        ts_tm.tm_min = min;
        ts_tm.tm_sec = sec;
        ts_tm.tm_isdst = -1;
        
        time_t ts_time = mktime(&ts_tm);
        time_t now = time(NULL);
        
        if (ts_time == -1) { 
             // Error parsing, fallback
            printf(C_WHITE C_ITALIC "%.10s " C_RESET, ts_str);
            return;
        }

        double diff = difftime(now, ts_time);
        struct tm *now_tm = localtime(&now);
        
        // Same day check
        if (now_tm->tm_year == ts_tm.tm_year && now_tm->tm_mon == ts_tm.tm_mon && now_tm->tm_mday == ts_tm.tm_mday) {
             printf(C_WHITE C_ITALIC "%02d:%02d " C_RESET, hour, min);
        } else {
             // Calculate days difference roughly
             // Reset hours to compare dates strictly
             // But simpler:
             int days = (int)(diff / (60*60*24));
             if (days == 0) { 
                 // It's "yesterday" but less than 24h? Or just diff day.
                 // If dates don't match but diff < 24h, it might be yesterday.
                 printf(C_WHITE C_ITALIC "Hier " C_RESET);
             } else if (days == 1) {
                 printf(C_WHITE C_ITALIC "1 jour " C_RESET);
             } else if (days < 7) {
                 printf(C_WHITE C_ITALIC "%d jours " C_RESET, days);
             } else if (days < 30) {
                 printf(C_WHITE C_ITALIC "%d sem. " C_RESET, days / 7);
             } else {
                 printf(C_WHITE C_ITALIC ">1 mois " C_RESET);
             }
        }
    } else {
         // Fallback if parsing fails
         printf(C_WHITE C_ITALIC "%.10s " C_RESET, ts_str);
    }
}

void ui_print_pretty_msg(const char *msg) {
  pthread_mutex_lock(&print_lock);
  printf("\r\033[K"); // Clear current line

  if (strncmp(msg, "!!!", 3) == 0 || strstr(msg, "ERREUR")) {
    printf(C_RED C_BOLD " ⚠ %s" C_RESET "\n", msg);
  } else if (msg[0] == '[') {
    // Attempt to detect [TIMESTAMP] [User] format
    char *first_close = strchr(msg, ']');
    if (first_close) {
        // Check if there is a second bracket shortly after
    // Check if there is a second bracket shortly after
        char *second_open = strchr(first_close + 1, '[');
        if (second_open && (second_open - first_close < 3)) {
            // Likely [TIMESTAMP] [User]
            char *second_close = strchr(second_open, ']');
            if (second_close) {
                // Extract parts
                int ts_len = first_close - msg - 1; // Exclude brackets
                // Msg starts at msg+1
                
                int user_len = second_close - second_open + 1;
                char *content = second_close + 1;
                
                // Extract just the name for comparison (skip [ and ])
                int name_len = user_len - 2;
                char name_extracted[64];
                if (name_len > 63) name_len = 63;
                strncpy(name_extracted, second_open + 1, name_len);
                name_extracted[name_len] = '\0';
                
                // Determine if it is Me
                // Check against current_username OR literal [Moi] for robustness (e.g. if user test used)
                int is_me = 0;
                if (strlen(current_username) > 0 && strcmp(name_extracted, current_username) == 0) {
                    is_me = 1;
                }
                if (strncmp(second_open, "[Moi]", 5) == 0) is_me = 1;

                // Debug trigger if needed (uncomment to debug)
                // printf("DEBUG: '%s' vs '%s' -> %d\n", name_extracted, current_username, is_me);

                // Get formatted timestamp string
                char ts_buf[32];
                struct tm ts_tm = {0};
                int year, month, day, hour, min, sec;
                if (sscanf(msg + 1, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &min, &sec) == 6) {
                    ts_tm.tm_year = year - 1900; ts_tm.tm_mon = month - 1; ts_tm.tm_mday = day;
                    ts_tm.tm_hour = hour; ts_tm.tm_min = min; ts_tm.tm_sec = sec; ts_tm.tm_isdst = -1;
                    time_t ts_time = mktime(&ts_tm);
                    time_t now = time(NULL);
                    double diff = difftime(now, ts_time);
                    struct tm *now_tm = localtime(&now);
                    
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
                    snprintf(ts_buf, sizeof(ts_buf), "%.*s", (ts_len > 10 ? 10 : ts_len), msg + 1);
                }

                if (is_me) {
                    // Calculate target position
                    struct winsize w;
                    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0) {
                        w.ws_col = 80; // Default fallback
                    }
                    int term_width = w.ws_col;
                    
                    // Len = timestamp + space + [Moi] + content
                    int content_len = strlen(content);
                    // Remove newline from content len if present for calc
                    if (content_len > 0 && content[content_len-1] == '\n') content_len--;
                    
                    char display_user[64];
                    if (is_me) snprintf(display_user, 64, "[Moi]");
                    else snprintf(display_user, 64, "%.*s", user_len, second_open);
                    
                    int total_len = strlen(ts_buf) + 1 + strlen(display_user) + content_len;
                    
                    // Position cursor to Right - TotalLen - Margin
                    // ANSI: \033[<Col>G
                    int target_col = term_width - total_len - 1; 
                    if (target_col < 1) target_col = 1;
                    
                    printf("\033[%dG", target_col); // Move to column
                    
                    printf(C_WHITE C_ITALIC "%s " C_RESET, ts_buf);
                    printf(C_GREEN C_BOLD "%s" C_RESET "%s", display_user, content);
                } else {
                    // Standard Left Align
                    printf(C_WHITE C_ITALIC "%s " C_RESET, ts_buf);
                    printf(C_CYAN C_BOLD "%.*s" C_RESET "%s", user_len, second_open, content);
                }
                goto prompt_refresh;
            }
        }
    }

    // Fallback to old [User] logic
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
  // Reprint prompt
  printf(C_BLUE C_BOLD " ➤ Saisie " C_RESET "> %s", input_buffer);
  if (input_len > cursor_pos) {
      printf("\033[%dD", input_len - cursor_pos);
  }
  fflush(stdout);
  pthread_mutex_unlock(&print_lock);
}

void ui_print_char(char ch) {
    (void)ch;
}
