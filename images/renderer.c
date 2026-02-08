#define STB_IMAGE_IMPLEMENTATION
#include "renderer.h"
#include "stb_image.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <unistd.h>
#include <wordexp.h>
#include <sys/stat.h>
#include <errno.h>

// Structure pour pixel
typedef struct {
  unsigned char r, g, b;
} Pixel;

void img_render_file(const char *path, int max_width) {
  // 1. Expansion du chemin (pour gérer ~)
  wordexp_t exp_result;
  char *real_path = (char *)path;
  bool expanded = false;



  if (wordexp(path, &exp_result, 0) == 0) {
    if (exp_result.we_wordc > 0) {
      real_path = exp_result.we_wordv[0];
      expanded = true;
    }
  }



  // 3. Charger l'image
  int w, h, c;
  unsigned char *img = stbi_load(real_path, &w, &h, &c, 3); // Force RGB

  if (!img) {
    printf("Erreur: Impossible de charger l'image %s (Raison: %s)\n", real_path, stbi_failure_reason());
    if (expanded)
      wordfree(&exp_result);
    return;
  }

  if (expanded)
    wordfree(&exp_result);

  // 3. Détecter la taille du terminal ou utiliser max_width
  int target_w = max_width;
  if (target_w <= 0) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1) {
      target_w = ws.ws_col / 2; // Default logic
    } else {
      target_w = 40;
    }
  }

  // Contraintes
  if (target_w > 80)
    target_w = 80; // Maximum pour la lecture
  if (target_w < 20)
    target_w = 20;

  // 3. Calcul target_h (Ratio)
  if (w < target_w) {
    target_w = w;
  }
  float scale = (float)target_w / w;
  int target_h = (int)(h * scale);
  if (target_h % 2 != 0)
    target_h++;

  // 4. Redimensionnement (RMS)
  Pixel *resized = (Pixel *)malloc(target_w * target_h * sizeof(Pixel));

  for (int y = 0; y < target_h; y++) {
    for (int x = 0; x < target_w; x++) {
      float src_x_start = (float)x * w / target_w;
      float src_x_end = (float)(x + 1) * w / target_w;
      float src_y_start = (float)y * h / target_h;
      float src_y_end = (float)(y + 1) * h / target_h;

      int isx = (int)src_x_start;
      int iex = (int)src_x_end;
      if (iex == isx)
        iex++;

      int isy = (int)src_y_start;
      int iey = (int)src_y_end;
      if (iey == isy)
        iey++;

      double r_acc = 0, g_acc = 0, b_acc = 0;
      int count = 0;

      for (int iy = isy; iy < iey && iy < h; iy++) {
        for (int ix = isx; ix < iex && ix < w; ix++) {
          int idx = (iy * w + ix) * 3;
          r_acc += img[idx] * img[idx];
          g_acc += img[idx + 1] * img[idx + 1];
          b_acc += img[idx + 2] * img[idx + 2];
          count++;
        }
      }

      int out_idx = y * target_w + x;
      if (count > 0) {
        resized[out_idx].r = (unsigned char)sqrt(r_acc / count);
        resized[out_idx].g = (unsigned char)sqrt(g_acc / count);
        resized[out_idx].b = (unsigned char)sqrt(b_acc / count);
      } else {
        resized[out_idx].r = 0;
        resized[out_idx].g = 0;
        resized[out_idx].b = 0;
      }
    }
  }

  // 5. Affichage
  for (int y = 0; y < target_h; y += 2) {
    for (int x = 0; x < target_w; x++) {
      Pixel p1 = resized[y * target_w + x];
      Pixel p2 = {0, 0, 0};
      if (y + 1 < target_h) {
        p2 = resized[(y + 1) * target_w + x];
      }
      printf("\033[38;2;%d;%d;%dm\033[48;2;%d;%d;%dm\xE2\x96\x80", p1.r, p1.g,
             p1.b, p2.r, p2.g, p2.b);
    }
    printf("\033[0m\r\n"); // \r\n for raw mode compatibility
  }

  free(resized);
  stbi_image_free(img);
}
