#pragma once

#include <stdbool.h>

#include "engine/debug_lines.h"
#include "engine/renderer.h"

/* Dessin des lignes de debug (M1-11) : envoie un EngDebugLines au GPU par un buffer de streaming
 * (une écriture par image, sans allocation) et met en file au plus deux dessins, un par liste :
 *   - lignes testées en profondeur : pipeline avec test, SANS écriture de profondeur (elles ne
 *     cachent rien) ;
 *   - lignes superposées : sans profondeur, dessinées après les premières.
 * Les lignes font 1 pixel de large (SDL_GPU ne gère pas d'épaisseur) et sont opaques.
 *
 * À enregistrer EN DERNIER dans la file de l'image : les lignes testées en profondeur ne sont
 * cachées que par ce qui est déjà dessiné, et les superposées recouvrent tout.
 *
 *     EngDebugLinesGpu gpu;  eng_debug_lines_gpu_create(&gpu, &renderer, 16384, 0);
 *     chaque image : eng_debug_lines_gpu_draw(&gpu, &renderer, queue, &lines, view_proj);
 *     eng_debug_lines_gpu_destroy(&gpu, &renderer);                                        */

typedef struct EngDebugLinesGpu {
    EngBufferHandle buffer;
    EngPipelineHandle
        depth_pipeline; /* test de profondeur (sans test si le renderer n'a pas de depth buffer) */
    EngPipelineHandle overlay_pipeline; /* sans profondeur */
    uint32_t max_lines;                 /* lignes par liste */
} EngDebugLinesGpu;

/* Crée le buffer (2 × max_lines × 2 sommets) et les deux pipelines. `color_format` : format de la
 * cible (SDL_GPUTextureFormat), 0 pour la swapchain. Échec (false, journalisé) : tout est détruit.
 */
bool eng_debug_lines_gpu_create(EngDebugLinesGpu *gpu, EngRenderer *renderer, uint32_t max_lines,
                                uint32_t color_format);

/* Sans effet sur une structure nulle ou déjà détruite. */
void eng_debug_lines_gpu_destroy(EngDebugLinesGpu *gpu, EngRenderer *renderer);

/* Écrit les lignes dans le buffer de streaming et met en file les dessins non vides. `view_proj` :
 * monde → découpage. Retourne le nombre de dessins acceptés par la file (0 à 2). Rien n'est fait
 * (0) si `lines->capacity` dépasse `max_lines` du modèle GPU ou si l'écriture échoue. */
uint32_t eng_debug_lines_gpu_draw(const EngDebugLinesGpu *gpu, EngRenderer *renderer,
                                  EngRenderQueue *queue, const EngDebugLines *lines,
                                  mat4 view_proj);
