#include "ui_shared.h"

char input_buffer[BUFFER_SIZE];
int input_len = 0;
int cursor_pos = 0;

static char history[HISTORY_MAX][BUFFER_SIZE];
static int history_count = 0;
static int history_pos = 0;

void ui_reset_input() {
  memset(input_buffer, 0, BUFFER_SIZE);
  input_len = 0;
  cursor_pos = 0;
}

void ui_insert_char(char ch) {
    if (input_len < BUFFER_SIZE - 1) {
        // Shift content if not at end
        if (cursor_pos < input_len) {
            memmove(input_buffer + cursor_pos + 1, input_buffer + cursor_pos, input_len - cursor_pos);
        }
        input_buffer[cursor_pos] = ch;
        input_len++;
        cursor_pos++;
        input_buffer[input_len] = '\0';
        ui_refresh_prompt();
    }
}

void ui_delete_char() {
    if (cursor_pos > 0) {
        memmove(input_buffer + cursor_pos - 1, input_buffer + cursor_pos, input_len - cursor_pos);
        input_len--;
        cursor_pos--;
        input_buffer[input_len] = '\0';
        ui_refresh_prompt();
    }
}

void ui_move_cursor_left() {
    if (cursor_pos > 0) {
        cursor_pos--;
        ui_refresh_prompt();
    }
}

void ui_move_cursor_right() {
    if (cursor_pos < input_len) {
        cursor_pos++;
        ui_refresh_prompt();
    }
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
  cursor_pos = input_len; // Set cursor to end
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
