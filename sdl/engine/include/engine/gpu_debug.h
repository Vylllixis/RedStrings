#ifndef ENGINE_GPU_DEBUG_H
#define ENGINE_GPU_DEBUG_H

/* Débogage GPU (M1-19) : noms de passes et de ressources lisibles dans RenderDoc, Nsight, PIX.
 *
 * Deux outils :
 *   - les GROUPES : eng_gpu_debug_push / eng_gpu_debug_pop entourent une passe ou une série de
 *     commandes (« Frame 12 » > « Shadow map », « Scene », « Tonemap », « Overlay »...) ;
 *   - les NOMS de ressources : eng_gpu_debug_name_texture / _buffer et eng_gpu_debug_props
 *     (échantillonneurs, pipelines, shaders), à la création.
 *
 * Tout passe par SDL_GPU : aucun appel direct au pilote. Sans outil attaché, un groupe ne coûte
 * qu'un appel SDL (un no-op si le pilote n'a pas VK_EXT_debug_utils). Les NOMS de ressources ne
 * sont transmis au pilote que si le périphérique a été créé avec `EngGpuDesc.debug` (limite de
 * SDL).
 *
 * Les groupes sont comptés (eng_gpu_debug_stats) et peuvent être observés par un ENREGISTREUR :
 * c'est ainsi que les tests vérifient l'ordre, l'imbrication et l'équilibre, car SDL ne permet pas
 * de relire ce qui a été envoyé au pilote.
 *
 * Fil unique : état global, à utiliser depuis le fil de rendu seulement. Activer ou désactiver
 * (eng_gpu_debug_set_enabled) entre deux images ; un pop sans push correspondant est ignoré et
 * compté (unbalanced_pops), jamais transmis au pilote.
 */

#include <stdbool.h>
#include <stdint.h>

struct SDL_GPUCommandBuffer;
struct SDL_GPUDevice;
struct SDL_GPUTexture;
struct SDL_GPUBuffer;

#define ENG_GPU_DEBUG_NAME_MAX 64U /* octets, zéro final compris ; au-delà, le nom est tronqué */

/* Noms des passes de l'image, dans l'ordre où ils apparaissent (docs/gpu-debug.md). Anglais et
 * ASCII : certains outils affichent mal les accents. */
#define ENG_GPU_PASS_FRAME_FORMAT "Frame %llu" /* groupe racine ; argument : unsigned long long */
#define ENG_GPU_PASS_UPLOAD_STREAMS "Upload streams" /* copie des buffers de streaming */
#define ENG_GPU_PASS_SHADOW "Shadow map"
#define ENG_GPU_PASS_OVERLAY_UPLOAD "Overlay upload" /* copie des données de l'interface */
#define ENG_GPU_PASS_SCENE "Scene"
#define ENG_GPU_PASS_TONEMAP "Tonemap" /* résolution HDR vers la swapchain */
#define ENG_GPU_PASS_OVERLAY "Overlay"

typedef enum EngGpuDebugEvent {
    ENG_GPU_DEBUG_PUSH = 0, /* ouverture d'un groupe : name = nom du groupe */
    ENG_GPU_DEBUG_POP,      /* fermeture du dernier groupe ouvert : name = son nom */
    ENG_GPU_DEBUG_LABEL,    /* repère ponctuel dans le flux de commandes */
    ENG_GPU_DEBUG_NAME, /* nom donné à une ressource : kind = « texture », « buffer »... */
} EngGpuDebugEvent;

/* `depth` : profondeur de groupes APRÈS l'événement (0 hors de tout groupe). `kind` : NULL sauf
 * pour ENG_GPU_DEBUG_NAME. Le texte n'est valable que pendant l'appel. */
typedef void (*EngGpuDebugRecorder)(void *user, EngGpuDebugEvent event, const char *kind,
                                    const char *name, uint32_t depth);

typedef struct EngGpuDebugStats {
    uint32_t pushes;
    uint32_t pops;
    uint32_t labels;
    uint32_t names; /* noms de ressources transmis */
    uint32_t
        unbalanced_pops; /* pop sans groupe ouvert, débogage actif : ignoré (bug de l'appelant) */
    uint32_t depth;      /* groupes ouverts maintenant */
    uint32_t max_depth;
} EngGpuDebugStats;

/* Actif par défaut. Désactivé : plus aucun push, label ni nom n'est émis (les pop des groupes
 * encore ouverts le sont quand même, pour ne pas laisser un groupe béant). */
void eng_gpu_debug_set_enabled(bool enabled);
bool eng_gpu_debug_enabled(void);

/* NULL retire l'enregistreur. */
void eng_gpu_debug_set_recorder(EngGpuDebugRecorder fn, void *user);

EngGpuDebugStats eng_gpu_debug_stats(void);
void eng_gpu_debug_reset_stats(void); /* remet les compteurs à zéro ; la profondeur est conservée */

/* Ouvre un groupe nommé sur ce tampon de commandes. NULL pour `name` : « ? ». Sans effet si
 * `commands` est NULL ou si le débogage est désactivé. */
void eng_gpu_debug_push(struct SDL_GPUCommandBuffer *commands, const char *name);

/* Idem avec un nom formaté (printf), tronqué à ENG_GPU_DEBUG_NAME_MAX - 1 octets. */
void eng_gpu_debug_push_fmt(struct SDL_GPUCommandBuffer *commands, const char *format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

/* Ferme le dernier groupe ouvert. Sans groupe ouvert : ignoré et compté. */
void eng_gpu_debug_pop(struct SDL_GPUCommandBuffer *commands);

/* Repère ponctuel (ex. « début des particules »). */
void eng_gpu_debug_label(struct SDL_GPUCommandBuffer *commands, const char *text);

/* Noms de ressources. `name` NULL ou vide : rien. */
void eng_gpu_debug_name_texture(struct SDL_GPUDevice *device, struct SDL_GPUTexture *texture,
                                const char *name);
void eng_gpu_debug_name_buffer(struct SDL_GPUDevice *device, struct SDL_GPUBuffer *buffer,
                               const char *name);

/* Propriétés SDL portant un nom, à passer au champ `props` d'une création (échantillonneur,
 * pipeline graphique, shader). `key` : la constante SDL_PROP_GPU_*_CREATE_NAME_STRING ; `kind` :
 * « sampler », « pipeline », « shader » (pour le journal et l'enregistreur). Renvoie 0 si `name`
 * est NULL/vide, si le débogage est désactivé ou si SDL échoue : la création se fait alors sans
 * nom. À détruire avec eng_gpu_debug_props_free (0 accepté) APRÈS la création. */
uint32_t eng_gpu_debug_props(const char *kind, const char *key, const char *name);
void eng_gpu_debug_props_free(uint32_t props);

#endif /* ENGINE_GPU_DEBUG_H */
