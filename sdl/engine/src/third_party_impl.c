/* Seul endroit où les bibliothèques à en-tête unique sont implémentées.
 * Code tiers : compilé sans -Werror (voir engine/CMakeLists.txt). */
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
