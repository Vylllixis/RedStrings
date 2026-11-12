#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/files.h"
#include "engine/gpu.h"
#include "engine/shader.h"

/* Shader rechargeable à chaud.
 *
 * Build « hot » (OW_SHADER_HOT_RELOAD, par défaut en Debug seulement) : le `.hlsl` source est
 * surveillé ; à chaque modification il est recompilé À L'EXÉCUTION (DXC + SPIRV-Cross liés au
 * moteur) et le nouveau SDL_GPUShader remplace l'ancien. Autres builds : le shader compilé hors
 * ligne par la build (M1-02) est chargé une fois, et rien n'est jamais rechargé : le même code
 * d'appel sert dans les deux cas.
 *
 * Utilisation (une fois par image) :
 *     if (eng_shader_hot_poll(&hot, &gpu, &scratch, now)) {
 *         // `hot.shader` est un NOUVEAU shader : recréer les pipelines qui l'utilisent
 *     }
 *
 * Garanties :
 *  - une erreur de compilation est journalisée (message de DXC) et l'ancien shader RESTE en
 *    service : rien ne plante, l'image continue à s'afficher ;
 *  - l'ancien shader n'est libéré qu'après son remplacement ; SDL_GPU autorise à libérer un
 *    shader dont un pipeline a déjà été créé : recréer le pipeline juste après le poll suffit ;
 *  - une modification n'est lue qu'une fois le fichier stable (voir EngFileWatch). */

typedef struct EngShaderHot {
    EngShaderStage stage;
    char source[512];      /* chemin du .hlsl (build hot) */
    char include_dir[512]; /* dossier des #include : celui du source */
    char name[128]; /* nom hors ligne (« triangle.frag »), repli et builds sans rechargement */
    char
        offline_dir[512]; /* dossier des shaders compilés (NULL à l'initialisation : « shaders ») */
    EngFileWatch watch;
    struct SDL_GPUShader *shader; /* shader actuel, propriété de la structure */
    uint32_t generation;          /* +1 à chaque nouveau shader (y compris le premier) */
    uint32_t failed_reloads;      /* recompilations échouées depuis le début */
    bool hot;                     /* le rechargement est actif dans cette build */
} EngShaderHot;

/* Vrai si cette build recompile les shaders à l'exécution. */
bool eng_shader_hot_available(void);

/* Crée le premier shader. `source_path` peut être NULL : le shader hors ligne est alors chargé
 * sans surveillance, même dans une build hot. Build hot : compilé depuis `source_path` ; s'il ne
 * compile pas, le shader hors ligne `name` (dans `offline_dir`, NULL : « shaders ») sert de repli.
 * Autres builds : le shader hors ligne directement. false (journalisé) si aucun des deux ne marche.
 * `scratch` est rendue dans son état d'origine. */
bool eng_shader_hot_init(EngShaderHot *hot, EngGpu *gpu, EngArena *scratch, const char *source_path,
                         const char *offline_dir, const char *name, EngShaderStage stage);

/* À appeler souvent avec un temps croissant en secondes. Vrai UNE fois quand `hot->shader` vient
 * d'être remplacé par un shader fraîchement compilé. */
bool eng_shader_hot_poll(EngShaderHot *hot, EngGpu *gpu, EngArena *scratch, double now_seconds);

/* Libère le shader. Sans effet sur une structure déjà libérée. */
void eng_shader_hot_release(EngShaderHot *hot, EngGpu *gpu);

/* Compile `hlsl` (point d'entrée « main ») et crée le shader. NULL + journal si l'étape échoue ou
 * si la build n'a pas le rechargement. Exposé pour les tests ; passe par la même voie que le poll.
 * `include_dir` peut être NULL. */
struct SDL_GPUShader *eng_shader_compile_hlsl(EngGpu *gpu, const char *hlsl,
                                              const char *include_dir, EngShaderStage stage,
                                              const char *debug_name);
