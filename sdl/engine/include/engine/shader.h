#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/gpu.h"

/* Chargement des shaders compilés hors ligne (cmake/Shaders.cmake). Pour chaque shader « nom »,
 * la build produit dans <dossier de l'exécutable>/shaders/ :
 *     nom.spv    SPIR-V   (Vulkan)
 *     nom.dxil   DXIL     (Direct3D 12)
 *     nom.msl    MSL      (Metal)
 *     nom.json   réflexion : nombre de samplers, textures et tampons de stockage, tampons uniformes
 * `nom` inclut l'étage : « triangle.vert », « triangle.frag ». Le format chargé est celui que le
 * périphérique GPU accepte : le même dossier sert donc tous les pilotes. */

typedef enum EngShaderStage {
    ENG_SHADER_VERTEX,
    ENG_SHADER_FRAGMENT,
} EngShaderStage;

/* Ressources déclarées par un shader, lues dans le JSON de réflexion. */
typedef struct EngShaderCounts {
    uint32_t samplers;
    uint32_t storage_textures;
    uint32_t storage_buffers;
    uint32_t uniform_buffers;
} EngShaderCounts;

/* Format retenu pour un périphérique. */
typedef struct EngShaderFormat {
    uint32_t sdl_format;   /* SDL_GPUShaderFormat */
    const char *extension; /* « spv », « dxil », « msl » */
    const char
        *entry_point; /* « main » (SPIR-V, DXIL) ou « main0 » (MSL : nom imposé par SPIRV-Cross) */
} EngShaderFormat;

/* Choisit le format à charger parmi ceux que le périphérique accepte (`sdl_formats` :
 * SDL_GetGPUShaderFormats). Ordre de préférence : SPIR-V, DXIL, MSL. false si aucun. */
bool eng_shader_choose_format(uint32_t sdl_formats, EngShaderFormat *out);

/* Lit le nombre de ressources dans le texte JSON de réflexion. false si une des quatre clés
 * manque ou n'est pas un entier positif (`counts` est alors inchangé). */
bool eng_shader_parse_counts(const char *json, EngShaderCounts *counts);

/* Charge et crée le shader « nom » (ex. « triangle.vert ») depuis le dossier `dir` (relatif à
 * l'exécutable, ou absolu ; NULL : « shaders »). `scratch` sert à lire les fichiers ; il est
 * rendu dans son état d'origine. NULL (avec un message dans le journal) si un fichier
 * manque, si le JSON est illisible ou si le périphérique refuse le shader. Le résultat est un
 * SDL_GPUShader*, à libérer avec eng_shader_release. */
struct SDL_GPUShader *eng_shader_load(EngGpu *gpu, EngArena *scratch, const char *dir,
                                      const char *name, EngShaderStage stage);

void eng_shader_release(EngGpu *gpu, struct SDL_GPUShader *shader);
