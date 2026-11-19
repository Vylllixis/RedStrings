#pragma once

#include <cglm/cglm.h>
#include <stdbool.h>
#include <stdint.h>

#include "engine/renderer.h"

/* Instancing GPU (M3-07) : un même maillage dessiné des milliers de fois en UN dessin.
 *
 * Chaque instance (EngInstance, 36 octets) donne la position, l'échelle uniforme, la rotation et la
 * teinte d'une copie. Elles vivent dans un buffer de sommets lu par le GPU au rythme « une par
 * instance » (flux 1 du pipeline, voir EngPipelineDesc.instance_stride) : pas d'uniforms par copie,
 * pas de dessin par copie. 50 000 rochers = 1 dessin par partie du maillage (statistique
 * EngRendererStats.instances).
 *
 * Le maillage est un EngModelGpu ordinaire ; on le dessine avec eng_model_gpu_draw_ex en passant
 * EngModelDrawOptions.instance_buffer / instance_count, et le pipeline
 * d'eng_instanced_pipeline_desc (vertex shader instanced.vert, fragment shader model.frag inchangé
 * : même matériau PBR, même éclairage, même brouillard-ready). Voir docs/instancing.md. */

typedef struct EngInstance {
    float position[3]; /* mètres, monde */
    float scale;       /* échelle uniforme, > 0 */
    float rotation[4]; /* quaternion unitaire (x, y, z, w) */
    uint8_t tint[4]; /* facteur RGBA 8 bits, multiplié à la couleur du matériau (255 : inchangé) */
} EngInstance;

#define ENG_INSTANCE_STRIDE 36U

/* Pipeline des maillages instanciés : comme eng_model_pipeline_desc, avec le flux d'instances. */
void eng_instanced_pipeline_desc(EngPipelineDesc *desc);

/* Rotation d'axe Y (cap), en radians : le cas des rochers et des arbres posés sur le sol. */
void eng_instance_make_yaw(EngInstance *instance, const float position[3], float scale, float yaw,
                           const uint8_t tint[4]);

/* Matrice modèle -> monde équivalente à l'instance (translation x rotation x échelle) : la
 * référence CPU du vertex shader (test engine.instancing_gpu) et de la boîte englobante. */
void eng_instance_matrix(const EngInstance *instance, mat4 out);

/* Une instance est valide si tout est fini, l'échelle > 0 et le quaternion unitaire (à 1e-3). */
bool eng_instance_valid(const EngInstance *instance);

/* Crée le buffer immuable des instances. false (journalisé) si `count` est 0, si une instance n'est
 * pas valide, ou si le renderer refuse. L'appelant le détruit avec eng_renderer_destroy_buffer. */
EngBufferHandle eng_instances_upload(EngRenderer *renderer, const EngInstance *instances,
                                     uint32_t count);
