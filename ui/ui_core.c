#include "ui_shared.h"
#include <sys/ioctl.h>

pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;
char current_username[64] = "";

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
         "v2.0 - Canaux" C_RESET "\n\n");
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

void ui_set_scroll_region(int top, int bottom) {
    // DECSTBM: Set Top and Bottom Margins
    // If bottom is 0, it means "to end of screen" (roughly, usually handled by just omitting second arg or using known height)
    // Actually, 'r' takes two arguments. If we want 'top to end', we might need screen height.
    // simpler: \033[top;r sets top to end? No, usually \033[top;bottomr.
    // If we omit bottom: \033[topr ? No. 
    // Let's assume full height or leave it open? 
    // Standard is \033[start;endr. 
    // If we don't know height, we can try to detect or just set a large number?
    // Better: use ioctl to get rows.
    
    struct winsize w;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &w);
    int height = w.ws_row;
    
    if (bottom == 0) bottom = height;
    
    printf("\033[%d;%dr", top, bottom);
    fflush(stdout);
}

void ui_reset_scroll_region() {
    printf("\033[r"); // Resets to full screen
    fflush(stdout);
}
