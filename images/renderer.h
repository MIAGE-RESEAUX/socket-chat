#ifndef RENDERER_H
#define RENDERER_H

/**
 *
 * @param path Path to the image file.
 * @param max_width Maximum width in characters (columns). If 0, auto-detects or
 * uses default.
 */
void img_render_file(const char *path, int max_width);

#endif
