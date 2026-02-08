#ifndef RENDERER_H
#define RENDERER_H

/**
 * @file renderer.h
 * @brief Rendu d'images dans le terminal.
 *
 * Ce module utilise stb_image pour charger des images et les afficher
 * en utilisant des caractères ASCII ou des codes couleurs ANSI (si supporté).
 */

/**
 * @brief Affiche une image dans le terminal.
 * Redimensionne l'image pour qu'elle tienne dans la largeur spécifiée
 * tout en conservant le ratio d'aspect.
 *
 * @param path Chemin vers le fichier image (jpg, png, etc.).
 * @param max_width Largeur maximale en caractères (colonnes). Si 0, utilise une valeur par défaut ou auto-détectée.
 */
void img_render_file(const char *path, int max_width);

#endif
