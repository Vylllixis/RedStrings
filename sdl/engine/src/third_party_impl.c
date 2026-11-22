/* Seul endroit où les bibliothèques à en-tête unique sont implémentées.
 * Code tiers : compilé sans -Werror (voir engine/CMakeLists.txt). */
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
/* Texte en champ de distance de l'écran de chargement (engine/text_sdf.h, 2026-11-22). */
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
