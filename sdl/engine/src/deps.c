/* Affiche les versions des dépendances (les implémentations tierces sont dans
 * third_party_impl.c). */
#include <SDL3/SDL_version.h>
#include <cglm/cglm.h>
#include <stdio.h>
#include "engine/engine.h"

void eng_print_deps(void)
{
    int v = SDL_GetVersion();
    vec3 a = {1.0f, 2.0f, 3.0f};
    vec3 b = {4.0f, 5.0f, 6.0f};
    printf("SDL %d.%d.%d\n", SDL_VERSIONNUM_MAJOR(v), SDL_VERSIONNUM_MINOR(v),
           SDL_VERSIONNUM_MICRO(v));
    printf("cglm dot((1,2,3),(4,5,6)) = %.0f\n", (double)glm_vec3_dot(a, b));
    printf("cgltf et stb_image compilés\n");
}
