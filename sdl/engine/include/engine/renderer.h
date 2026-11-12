#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/gpu.h"
#include "engine/pool.h"
#include "engine/tonemap.h"

/* Renderer (M1-05) : tout ce que le jeu et le moteur ont besoin pour dessiner, SANS exposer
 * SDL_GPU. Cet en-tête n'inclut aucun en-tête SDL (vérifié par tools/scripts/lint.sh).
 *
 *  - Ressources par HANDLES générationnels (buffers, pipelines) : un handle périmé est refusé
 *    proprement au lieu de pointer vers une autre ressource. Le handle d'un pipeline reste valide
 *    quand ses shaders sont rechargés à chaud : le renderer recrée le pipeline derrière.
 *  - FILE DE COMMANDES : on enregistre des dessins (EngDrawCmd) dans une EngRenderQueue, le
 *    renderer les exécute à la fin de l'image. L'enregistrement est de la donnée pure (fonctions
 *    `static inline` ci-dessous) : il pourra se faire depuis le code de jeu, qui ne lie pas le
 *    moteur.
 *
 * Utilisation :
 *     EngRenderer r;  eng_renderer_init(&r, &gpu, &memory.permanent, &memory.frame, &desc);
 *     EngBufferHandle vb = eng_renderer_create_buffer(&r, &buffer_desc);
 *     EngPipelineHandle pipe = eng_renderer_create_pipeline(&r, &pipeline_desc);
 *     chaque image :
 *         eng_renderer_update(&r, now);                    // rechargement des shaders (Debug)
 *         EngRenderQueue *q = eng_renderer_begin_frame(&r);
 *         eng_render_draw(q, &(EngDrawCmd){.pipeline = pipe, .vertex_buffer = vb, .count = 3});
 *         // avec des uniforms (matrice MVP...) copiés dans la file :
 *         eng_render_draw_uniforms(q, &cmd, &mvp, sizeof mvp);
 *         eng_renderer_end_frame(&r);                      // exécute la file, présente
 *     eng_renderer_shutdown(&r);                           // avant eng_gpu_shutdown */

/* ── Handles typés (on ne peut pas passer un buffer à la place d'un pipeline) ── */

typedef struct EngBufferHandle {
    EngHandle handle;
} EngBufferHandle;
typedef struct EngPipelineHandle {
    EngHandle handle;
} EngPipelineHandle;

typedef struct EngTextureHandle {
    EngHandle handle;
} EngTextureHandle;
typedef struct EngSamplerHandle {
    EngHandle handle;
} EngSamplerHandle;

#define ENG_BUFFER_NULL ((EngBufferHandle){ENG_HANDLE_NULL})
#define ENG_PIPELINE_NULL ((EngPipelineHandle){ENG_HANDLE_NULL})
#define ENG_TEXTURE_NULL ((EngTextureHandle){ENG_HANDLE_NULL})
#define ENG_SAMPLER_NULL ((EngSamplerHandle){ENG_HANDLE_NULL})

/* ── Buffers ── */

typedef enum EngBufferUsage {
    ENG_BUFFER_NONE = 0, /* non renseigné : refusé à la création */
    ENG_BUFFER_VERTEX = 1,
    ENG_BUFFER_INDEX = 2,
} EngBufferUsage;

typedef struct EngBufferDesc {
    const char
        *name; /* nom affiché dans les outils GPU (RenderDoc) ; NULL : « Vertex/Index buffer » */
    EngBufferUsage usage;
    uint32_t size;    /* octets */
    const void *data; /* contenu initial, `size` octets (obligatoire : buffers immuables) */
} EngBufferDesc;

/* ── Textures et samplers (M1-08) ──
 *
 * Une TEXTURE contient des pixels (et leurs mipmaps) ; un SAMPLER dit comment les lire (filtrage,
 * répétition, anisotropie). Les deux sont indépendants : un sampler sert à autant de textures qu'on
 * veut. Le dessin lie une texture et un sampler (EngDrawCmd), que le fragment shader lit avec
 *     Texture2D t : register(t0, space2);   SamplerState s : register(s0, space2);
 *
 * Coordonnées de texture : (0, 0) est le coin HAUT-GAUCHE de l'image (première ligne des pixels),
 * u vers la droite, v vers le BAS. */

typedef enum EngTextureFormat {
    ENG_TEXTURE_NONE = 0, /* non renseigné : refusé à la création */
    /* RGBA 8 bits, valeurs linéaires : cartes de normales, rugosité, masques... */
    ENG_TEXTURE_RGBA8,
    /* RGBA 8 bits, couleurs encodées en sRGB (images de couleur : albédo...). Le GPU les
     * décode en lumière linéaire à la lecture ; les mipmaps sont calculés en tenant compte de
     * l'encodage (engine/image.h). */
    ENG_TEXTURE_RGBA8_SRGB,
    /* Formats flottants 16 bits (IEEE 754 binary16), pour les données HDR : seulement avec
     * eng_renderer_create_texture_data (pas de mipmaps calculés ici). */
    ENG_TEXTURE_RGBA16F, /* 8 octets par texel */
    ENG_TEXTURE_RG16F,   /* 4 octets par texel */
    /* Profondeur 32 bits flottante : seulement la carte d'ombre (eng_renderer_create_shadow_map),
     * rendue par une passe d'ombre puis lue par les shaders. Refusée par les autres créations. */
    ENG_TEXTURE_DEPTH32F,
} EngTextureFormat;

typedef struct EngTextureDesc {
    const char *name; /* pour le journal */
    uint32_t width;
    uint32_t height;
    EngTextureFormat format;
    const void *pixels; /* width * height * 4 octets RGBA, première ligne = haut (EngImage) */
    /* Niveaux de mipmaps : 0 = chaîne complète (défaut), 1 = aucun mipmap, n = n niveaux (base
     * comprise). Refusé s'il dépasse la chaîne complète. */
    uint32_t mip_levels;
} EngTextureDesc;

/* Texture dont TOUS les niveaux sont fournis (données déjà calculées : cartes d'environnement,
 * LUT). `layers` : 1 (texture 2D) ou 6 (cubemap, `width == height`, faces dans l'ordre +X -X +Y -Y
 * +Z -Z, vues depuis l'intérieur, convention OpenGL / Vulkan / Direct3D : une direction (x, y, z)
 * lit la face de son axe dominant). `data` : pour chaque couche, les niveaux 0 à mip_levels-1 à la
 * suite, lignes serrées, première ligne = haut (v = 0). Un cube se lit dans le shader par une
 * direction : TextureCube.SampleLevel(sampler, direction, niveau). */
typedef struct EngTextureDataDesc {
    const char *name;
    uint32_t width;
    uint32_t height;
    uint32_t layers;
    uint32_t mip_levels; /* >= 1, au plus la chaîne complète */
    EngTextureFormat format;
    const void *data;
} EngTextureDataDesc;

#define ENG_MAX_ANISOTROPY 16.0F

/* Anisotropie effectivement demandée au GPU : `requested` plafonné à ENG_MAX_ANISOTROPY (tous les
 * GPU de bureau gèrent 16 ; au-delà, Vulkan refuse ou se comporte de façon indéfinie). */
static inline float eng_clamp_anisotropy(float requested)
{
    return requested > ENG_MAX_ANISOTROPY ? ENG_MAX_ANISOTROPY : requested;
}

typedef enum EngFilter {
    ENG_FILTER_LINEAR = 0, /* défaut : interpolation */
    ENG_FILTER_NEAREST,    /* texel le plus proche : pixel art, tests */
} EngFilter;

typedef enum EngMipFilter {
    ENG_MIP_LINEAR = 0, /* défaut : fondu entre deux niveaux (trilinéaire) */
    ENG_MIP_NEAREST,    /* niveau le plus proche */
    ENG_MIP_OFF,        /* uniquement le niveau 0 : pas de mipmaps, aliasing au loin */
} EngMipFilter;

typedef enum EngAddressMode {
    ENG_ADDRESS_REPEAT = 0, /* défaut : la texture se répète */
    ENG_ADDRESS_CLAMP,      /* le bord est prolongé */
    ENG_ADDRESS_MIRROR,     /* la texture se répète en miroir */
} EngAddressMode;

typedef struct EngSamplerDesc {
    const char *name; /* pour le journal */
    EngFilter min_filter;
    EngFilter mag_filter;
    EngMipFilter mip_filter;
    EngAddressMode address_u;
    EngAddressMode address_v;
    /* Filtrage anisotrope : 0 ou 1 = désactivé ; n > 1 = jusqu'à n échantillons le long de
     * l'axe où la texture est la plus comprimée (sols vus de biais). Plafonné à
     * ENG_MAX_ANISOTROPY (16) ; négatif ou non numérique : refusé. */
    float max_anisotropy;
} EngSamplerDesc;

/* ── Pipelines ── */

typedef enum EngVertexFormat {
    ENG_VERTEX_NONE = 0, /* non renseigné : refusé à la création du pipeline */
    ENG_VERTEX_FLOAT1 = 1,
    ENG_VERTEX_FLOAT2,
    ENG_VERTEX_FLOAT3,
    ENG_VERTEX_FLOAT4,
    ENG_VERTEX_UBYTE4_NORM, /* 4 octets lus comme 4 flottants dans [0, 1] (couleurs) */
} EngVertexFormat;

typedef struct EngVertexAttribute {
    uint32_t location; /* emplacement dans le shader : sémantique TEXCOORD<location> */
    EngVertexFormat format;
    uint32_t offset; /* octets depuis le début du sommet */
} EngVertexAttribute;

#define ENG_MAX_VERTEX_ATTRIBUTES 8

typedef enum EngTopology {
    ENG_TOPOLOGY_TRIANGLES = 0, /* valeur par défaut */
    ENG_TOPOLOGY_LINES,
} EngTopology;

typedef enum EngCullMode {
    ENG_CULL_BACK = 0, /* valeur par défaut */
    ENG_CULL_NONE,
    ENG_CULL_FRONT,
} EngCullMode;

typedef enum EngFrontFace {
    ENG_FRONT_COUNTER_CLOCKWISE = 0, /* valeur par défaut */
    ENG_FRONT_CLOCKWISE,
} EngFrontFace;

typedef struct EngPipelineDesc {
    const char *name;            /* pour le journal */
    const char *vertex_shader;   /* « triangle.vert » : voir docs/shaders.md */
    const char *fragment_shader; /* « triangle.frag » */
    uint32_t vertex_stride;      /* octets entre deux sommets (un seul flux de sommets) */
    EngVertexAttribute attributes[ENG_MAX_VERTEX_ATTRIBUTES];
    uint32_t attribute_count;
    EngTopology topology;
    EngCullMode cull_mode;
    EngFrontFace front_face;
    uint32_t color_format; /* SDL_GPUTextureFormat de la cible ; 0 : celui de la swapchain */
    /* Profondeur : si le renderer a un depth buffer (EngRendererDesc.depth_buffer), tous les
     * pipelines dessinent avec lui. depth_test : un fragment n'est gardé que s'il est PLUS PROCHE
     * (comparaison « inférieur ») que ce qui est déjà dessiné. depth_write : il met alors la
     * profondeur à jour. depth_write sans depth_test, ou l'un des deux sans depth buffer, est
     * refusé. Par défaut (faux, faux) : le pipeline ignore la profondeur (superpositions,
     * interface). */
    bool depth_test;
    bool depth_write;
    /* Pipeline d'une passe SANS cible de profondeur (résolution HDR, post-traitement) : il ne
     * déclare pas de profondeur même si le renderer a un depth buffer. Incompatible avec
     * depth_test et depth_write. */
    bool no_depth_target;
    /* Pipeline de la PASSE D'OMBRE (M1-17, eng_renderer_run_shadow_pass) : aucune cible couleur, la
     * profondeur est celle de la carte d'ombre (D32F). Exige depth_test et depth_write ;
     * incompatible avec no_depth_target ; n'a pas besoin du depth buffer de l'image. Son fragment
     * shader ne produit rien (shadow.frag). */
    bool depth_only;
} EngPipelineDesc;

/* ── File de commandes ── */

typedef enum EngIndexType {
    ENG_INDEX_U16 = 0, /* valeur par défaut */
    ENG_INDEX_U32,
} EngIndexType;

/* Nombre maximal de textures liées à un dessin (emplacements 0 à 4 du fragment shader). */
#define ENG_MAX_DRAW_TEXTURES 8U

typedef struct EngDrawCmd {
    EngPipelineHandle pipeline;
    EngBufferHandle vertex_buffer;
    EngBufferHandle index_buffer; /* ENG_BUFFER_NULL : dessin non indexé */
    EngIndexType index_type;
    uint32_t count;          /* sommets, ou index si indexé */
    uint32_t first;          /* premier sommet (ou index) */
    int32_t vertex_offset;   /* ajouté à chaque index (dessin indexé) */
    uint32_t instance_count; /* 0 : une instance */
    /* Uniforms du vertex shader (emplacement 0) : rempli par eng_render_draw_uniforms, ne pas
     * l'écrire à la main. uniform_size == 0 : aucun uniform. */
    uint32_t uniform_offset;
    uint32_t uniform_size;
    /* Uniforms du fragment shader (emplacement 0, `register(b0, space3)`) : rempli par
     * eng_render_draw_uniforms2, ne pas l'écrire à la main. fragment_uniform_size == 0 : aucun. */
    uint32_t fragment_uniform_offset;
    uint32_t fragment_uniform_size;
    /* Texture et sampler du fragment shader (emplacement 0). Tous les deux nuls : aucune
     * texture liée. L'un sans l'autre, ou un handle périmé : dessin rejeté. */
    EngTextureHandle texture;
    EngSamplerHandle sampler;
    /* Textures SUPPLÉMENTAIRES (M1-15), emplacements 1 à 4 du fragment shader, lues avec le MÊME
     * sampler. Elles se suivent sans trou : extra_textures[i] n'est permise que si l'emplacement
     * précédent (texture, ou extra_textures[i - 1]) est occupé. Un handle périmé, ou un trou :
     * dessin rejeté. Tout nul : une seule texture, comme avant. */
    EngTextureHandle extra_textures[ENG_MAX_DRAW_TEXTURES - 1];
    /* Carte d'ombre (M1-17), emplacement ENG_SHADOW_SLOT du fragment shader (`register(t8, space2)`
     * et `register(s8, space2)`). Les deux ou aucun : l'un sans l'autre, ou un handle périmé :
     * dessin rejeté. Exige les neuf emplacements 0 à 7 occupés (le shader les déclare tous). Tous
     * les deux nuls : emplacement 8 non lié. */
    EngTextureHandle shadow_map;
    EngSamplerHandle shadow_sampler;
} EngDrawCmd;

#define ENG_SHADOW_SLOT 8U

/* Taille maximale des uniforms d'un dessin (une matrice 4x4 = 64 octets) et alignement dans la
 * file. */
#define ENG_MAX_UNIFORM_BYTES 4096U
#define ENG_UNIFORM_ALIGNMENT 16U

typedef struct EngRenderQueue {
    EngDrawCmd *commands;
    uint32_t count;
    uint32_t capacity;
    uint32_t
        dropped; /* dessins refusés (file ou uniforms pleins, uniforms invalides), cette image */
    uint8_t *uniforms; /* copie des uniforms de l'image, référencés par EngDrawCmd.uniform_* */
    uint32_t uniform_used;
    uint32_t uniform_capacity;
} EngRenderQueue;

/* Ajoute un dessin. false (et `dropped` augmente) si la file est pleine. */
static inline bool eng_render_draw(EngRenderQueue *queue, const EngDrawCmd *cmd)
{
    if (queue->count >= queue->capacity) {
        queue->dropped++;
        return false;
    }
    queue->commands[queue->count++] = *cmd;
    return true;
}

/* Ajoute un dessin avec ses uniforms de VERTEX et/ou de FRAGMENT (M1-13). Chaque bloc est COPIÉ
 * dans la file (au plus ENG_MAX_UNIFORM_BYTES chacun), la source peut donc disparaître tout de
 * suite. Le vertex shader les lit dans `register(b0, space1)`, le fragment shader dans
 * `register(b0, space3)` ; une matrice cglm (mat4) se copie telle quelle, voir docs/conventions.md.
 * Un bloc absent s'écrit (NULL, 0). false (et `dropped` augmente) si la file ou la zone des
 * uniforms est pleine, si les deux blocs sont absents, ou si un bloc est incohérent (données NULL
 * avec une taille, ou taille nulle ou trop grande avec des données). Rien n'est ajouté en cas
 * d'échec. */
static inline bool eng_render_draw_uniforms2(EngRenderQueue *queue, const EngDrawCmd *cmd,
                                             const void *vertex_uniforms, uint32_t vertex_size,
                                             const void *fragment_uniforms, uint32_t fragment_size)
{
    const bool has_vertex = vertex_size != 0 || vertex_uniforms != NULL;
    const bool has_fragment = fragment_size != 0 || fragment_uniforms != NULL;
    const uint32_t vertex_offset =
        (queue->uniform_used + (ENG_UNIFORM_ALIGNMENT - 1U)) & ~(ENG_UNIFORM_ALIGNMENT - 1U);
    const uint32_t vertex_end = has_vertex ? vertex_offset + vertex_size : queue->uniform_used;
    const uint32_t fragment_offset =
        (vertex_end + (ENG_UNIFORM_ALIGNMENT - 1U)) & ~(ENG_UNIFORM_ALIGNMENT - 1U);
    const bool vertex_ok = !has_vertex || (vertex_uniforms != NULL && vertex_size != 0 &&
                                           vertex_size <= ENG_MAX_UNIFORM_BYTES);
    const bool fragment_ok = !has_fragment || (fragment_uniforms != NULL && fragment_size != 0 &&
                                               fragment_size <= ENG_MAX_UNIFORM_BYTES);
    if ((!has_vertex && !has_fragment) || !vertex_ok || !fragment_ok ||
        queue->count >= queue->capacity ||
        (has_vertex && (vertex_offset > queue->uniform_capacity ||
                        vertex_size > queue->uniform_capacity - vertex_offset)) ||
        (has_fragment && (fragment_offset > queue->uniform_capacity ||
                          fragment_size > queue->uniform_capacity - fragment_offset))) {
        queue->dropped++;
        return false;
    }
    EngDrawCmd *slot = &queue->commands[queue->count];
    *slot = *cmd;
    slot->uniform_offset = 0;
    slot->uniform_size = 0;
    slot->fragment_uniform_offset = 0;
    slot->fragment_uniform_size = 0;
    uint32_t used = queue->uniform_used;
    if (has_vertex) {
        memcpy(queue->uniforms + vertex_offset, vertex_uniforms, vertex_size);
        slot->uniform_offset = vertex_offset;
        slot->uniform_size = vertex_size;
        used = vertex_offset + vertex_size;
    }
    if (has_fragment) {
        memcpy(queue->uniforms + fragment_offset, fragment_uniforms, fragment_size);
        slot->fragment_uniform_offset = fragment_offset;
        slot->fragment_uniform_size = fragment_size;
        used = fragment_offset + fragment_size;
    }
    queue->count++;
    queue->uniform_used = used;
    return true;
}

/* Ajoute un dessin avec les uniforms de son vertex shader seulement (cas courant : une matrice
 * MVP). Mêmes règles que eng_render_draw_uniforms2 ; false (et `dropped` augmente) aussi si
 * `uniforms` est NULL ou `size` nul. */
static inline bool eng_render_draw_uniforms(EngRenderQueue *queue, const EngDrawCmd *cmd,
                                            const void *uniforms, uint32_t size)
{
    if (uniforms == NULL || size == 0) {
        queue->dropped++;
        return false;
    }
    return eng_render_draw_uniforms2(queue, cmd, uniforms, size, NULL, 0);
}

/* ── Renderer ── */

typedef struct EngRendererDesc {
    const char *source_dir; /* dossier des .hlsl (rechargement à chaud, Debug) ; NULL : aucun */
    const char *offline_dir; /* dossier des shaders compilés ; NULL : « shaders » de l'exécutable */
    uint32_t max_buffers;       /* 0 : 1024 */
    uint32_t max_pipelines;     /* 0 : 64 */
    uint32_t max_textures;      /* 0 : 256 */
    uint32_t max_samplers;      /* 0 : 32 */
    uint32_t max_commands;      /* dessins par image, 0 : 4096 */
    uint32_t max_uniform_bytes; /* uniforms de toute l'image, 0 : 256 Kio */
    /* Passe d'ombre (M1-17) : dessins de la file d'ombre par image (0 : pas de file d'ombre) et
     * octets d'uniforms correspondants (0 : 64 Kio). */
    uint32_t max_shadow_commands;
    uint32_t max_shadow_uniform_bytes;
    bool depth_buffer; /* profondeur pour la passe de l'image (eng_gpu_enable_depth) */
    /* HDR (M1-14) : la scène est dessinée en RGBA16F puis résolue vers l'écran (exposition,
     * tonemapping, sRGB ; engine/tonemap.h). Les pipelines sans color_format dessinent alors en
     * RGBA16F. Si le HDR ne peut pas être mis en place (shaders tonemap.* absents, GPU), un
     * avertissement est journalisé et le renderer continue SANS HDR (eng_renderer_hdr_enabled). */
    bool hdr;
    /* Format (SDL_GPUTextureFormat) de la cible de la résolution HDR ; 0 : celui de la swapchain.
     * Pour les tests, qui résolvent dans une texture de leur choix. */
    uint32_t resolve_format;
} EngRendererDesc;

typedef struct EngRendererStats {
    uint64_t frames;
    uint64_t draws;             /* dessins réellement émis */
    uint64_t uniform_pushes;    /* envois d'uniforms au GPU (vertex et fragment) */
    uint64_t sampler_binds;     /* changements de texture ou de sampler liés */
    uint64_t pipeline_binds;    /* changements de pipeline */
    uint64_t rejected_commands; /* dessins ignorés : handle invalide ou périmé */
    uint64_t dropped_commands;  /* dessins perdus : file pleine */
    uint64_t pipeline_rebuilds; /* pipelines recréés après un rechargement de shader */
    uint64_t overlay_passes;    /* images où l'overlay (interface de debug) a été dessiné */
    uint64_t resolve_draws;     /* résolutions HDR -> écran dessinées (M1-14) */
    uint64_t shadow_passes;     /* passes d'ombre ouvertes (M1-17) */
} EngRendererStats;

/* Overlay (M1-12) : dessin par-dessus la scène, dans une seconde passe sans profondeur (voir
 * eng_gpu_begin_overlay_pass). `prepare` est appelé AVANT toute passe (c'est là qu'on envoie
 * sommets et textures au GPU) et retourne false s'il n'y a RIEN à dessiner cette image : la passe
 * overlay n'est alors pas ouverte (interface masquée : coût nul). `draw` est appelé dans la passe.
 * `prepare` peut être nul : l'overlay est alors dessiné à chaque image. Le renderer ne connaît pas
 * l'interface : il appelle seulement ces deux fonctions (ui.h en fournit). */
typedef bool (*EngOverlayPrepareFn)(void *user, struct SDL_GPUCommandBuffer *commands);
typedef void (*EngOverlayDrawFn)(void *user, struct SDL_GPUCommandBuffer *commands,
                                 struct SDL_GPURenderPass *pass);

typedef struct EngRenderer {
    EngGpu *gpu;
    EngOverlayPrepareFn overlay_prepare;
    EngOverlayDrawFn overlay_draw;
    void *overlay_user;
    EngArena *scratch;
    EngPool buffers;
    EngPool pipelines;
    EngPool textures;
    EngPool samplers;
    EngRenderQueue queue;
    EngRenderQueue shadow_queue; /* dessins de la passe d'ombre ; capacity 0 : désactivée */
    EngTextureHandle shadow_map; /* carte rendue par end_frame avant la passe de l'image */
    char source_dir[512];
    char offline_dir[512];
    bool has_source_dir;
    bool has_offline_dir;
    bool ready;
    uint32_t streams_pending; /* buffers de streaming écrits, pas encore envoyés au GPU */
    bool warned_rejected;     /* le premier dessin rejeté a été signalé dans le journal */
    uint32_t depth_format;    /* format de profondeur de la passe de l'image ; 0 : aucun */
    uint32_t scene_format;    /* format de cible par défaut des pipelines (HDR ou swapchain) */
    bool hdr;                 /* HDR en service (désactivé si la mise en place a échoué) */
    EngTonemap tonemap;       /* réglages de la résolution HDR */
    EngPipelineHandle tonemap_pipeline;
    struct SDL_GPUSampler *hdr_sampler;
    uint32_t resolve_format; /* format de la cible de la résolution */
    EngRendererStats stats;
} EngRenderer;

/* `permanent` : mémoire des pools et de la file (durée de vie du renderer). `scratch` : arène de
 * travail pour lire et compiler les shaders ; le renderer la restitue dans son état d'origine à
 * chaque usage, mais elle doit survivre au renderer. false (journalisé) en cas d'échec. */
bool eng_renderer_init(EngRenderer *r, EngGpu *gpu, EngArena *permanent, EngArena *scratch,
                       const EngRendererDesc *desc);

/* Libère toutes les ressources encore vivantes, et retourne leur nombre (0 : tout avait été
 * détruit, ce qui est le cas normal ; sinon un avertissement est journalisé). Sans effet sur un
 * renderer déjà arrêté (retourne 0). À appeler avant eng_gpu_shutdown. */
uint32_t eng_renderer_shutdown(EngRenderer *r);

/* Crée un buffer et y copie `desc->data`. Handle nul (journalisé) en cas d'échec. */
EngBufferHandle eng_renderer_create_buffer(EngRenderer *r, const EngBufferDesc *desc);
/* Buffer de STREAMING (M1-11) : son contenu change à chaque image (lignes de debug, particules).
 * `capacity` octets, contenu non défini jusqu'à la première écriture. Pas d'allocation par image :
 * le tampon de transfert est créé ici une fois pour toutes.
 *
 *     chaque image, avant d'enregistrer les dessins qui l'utilisent :
 *         eng_renderer_stream_write(&r, vb, 0, vertices, size);
 *     eng_renderer_end_frame envoie les écritures au GPU AVANT la passe de rendu. Avec une passe
 *     ouverte à la main (tests) : eng_renderer_flush_streams(&r, commands) avant de l'ouvrir.
 *
 * Handle nul (journalisé) si `capacity` est nulle, l'usage invalide ou le GPU refuse. */
EngBufferHandle eng_renderer_create_stream_buffer(EngRenderer *r, EngBufferUsage usage,
                                                  uint32_t capacity);
/* Copie `size` octets à `offset` dans le buffer de streaming. Plusieurs écritures par image sont
 * permises (zones distinctes) ; la zone envoyée au GPU est leur enveloppe. false si le handle est
 * invalide ou n'est pas un buffer de streaming, `data` nul, ou si l'écriture dépasse la capacité.
 * `size` nul : sans effet, true. Les octets non écrits depuis l'image précédente sont indéfinis :
 * ne dessiner que ce qui a été écrit cette image. */
bool eng_renderer_stream_write(EngRenderer *r, EngBufferHandle handle, uint32_t offset,
                               const void *data, uint32_t size);
/* Envoie au GPU les écritures en attente, dans une passe de copie du tampon `commands`, qui doit
 * précéder la passe de rendu. Retourne le nombre de buffers envoyés. */
uint32_t eng_renderer_flush_streams(EngRenderer *r, struct SDL_GPUCommandBuffer *commands);

/* false si le handle est nul, périmé ou déjà détruit. */
bool eng_renderer_destroy_buffer(EngRenderer *r, EngBufferHandle handle);
bool eng_renderer_buffer_is_valid(const EngRenderer *r, EngBufferHandle handle);

/* Crée une texture, y envoie les pixels et ses mipmaps (calculés ici, voir engine/image.h).
 * Handle nul (journalisé) en cas d'échec : description invalide (taille nulle ou au-delà de
 * ENG_IMAGE_MAX_SIZE, pixels absents, format ou niveaux invalides) ou GPU qui refuse. */
EngTextureHandle eng_renderer_create_texture(EngRenderer *r, const EngTextureDesc *desc);
/* Variante où l'appelant fournit tous les niveaux (HDR, cubemaps) : voir EngTextureDataDesc.
 * Handle nul (journalisé) si la description est invalide ou si le GPU refuse. */
EngTextureHandle eng_renderer_create_texture_data(EngRenderer *r, const EngTextureDataDesc *desc);
/* Nombre de couches : 1 (2D) ou 6 (cube) ; 0 si le handle est invalide. */
uint32_t eng_renderer_texture_layers(const EngRenderer *r, EngTextureHandle handle);
bool eng_renderer_destroy_texture(EngRenderer *r, EngTextureHandle handle);

/* ── Ombre (M1-17, docs/shadows.md) ── */

/* Carte d'ombre carrée de `size` texels (8 à 8192) : texture de profondeur D32F, à la fois cible
 * d'une passe d'ombre et texture lue par les shaders. ENG_TEXTURE_NULL (journalisé) si `size` est
 * hors limites ou si le GPU ne gère pas ce format dans ces deux usages. */
EngTextureHandle eng_renderer_create_shadow_map(EngRenderer *r, uint32_t size);

/* Rend `queue` dans la carte d'ombre `map` (profondeur effacée à 1, aucune cible couleur) : ouvre
 * et ferme la passe sur `commands`, à appeler AVANT la passe de l'image. Les pipelines de la file
 * doivent être `depth_only`. Retourne le nombre de dessins émis ; 0 si `map` n'est pas une carte
 * d'ombre valide (aucune passe ouverte), ou si la file est vide (la carte est alors seulement
 * effacée). */
uint32_t eng_renderer_run_shadow_pass(EngRenderer *r, struct SDL_GPUCommandBuffer *commands,
                                      EngTextureHandle map, const EngRenderQueue *queue);

/* File d'ombre du renderer, vidée à chaque appel ; NULL si EngRendererDesc.max_shadow_commands
 * est nul. */
EngRenderQueue *eng_renderer_begin_shadow_queue(EngRenderer *r);

/* Carte rendue automatiquement avec la file d'ombre par eng_renderer_end_frame, avant la passe de
 * l'image. ENG_TEXTURE_NULL : plus de passe d'ombre. */
void eng_renderer_set_shadow_map(EngRenderer *r, EngTextureHandle map);
bool eng_renderer_texture_is_valid(const EngRenderer *r, EngTextureHandle handle);
/* Niveaux de mipmaps de la texture ; 0 si le handle est invalide. */
uint32_t eng_renderer_texture_mip_levels(const EngRenderer *r, EngTextureHandle handle);

EngSamplerHandle eng_renderer_create_sampler(EngRenderer *r, const EngSamplerDesc *desc);
bool eng_renderer_destroy_sampler(EngRenderer *r, EngSamplerHandle handle);
bool eng_renderer_sampler_is_valid(const EngRenderer *r, EngSamplerHandle handle);
/* Anisotropie effectivement appliquée (après plafonnement) ; 0 si le handle est invalide ou si
 * l'anisotropie est désactivée. */
float eng_renderer_sampler_anisotropy(const EngRenderer *r, EngSamplerHandle handle);

/* Crée un pipeline : charge (ou compile) ses deux shaders, puis le pipeline graphique. Handle nul
 * (journalisé) en cas d'échec : rien n'est alors à libérer. */
EngPipelineHandle eng_renderer_create_pipeline(EngRenderer *r, const EngPipelineDesc *desc);
bool eng_renderer_destroy_pipeline(EngRenderer *r, EngPipelineHandle handle);
bool eng_renderer_pipeline_is_valid(const EngRenderer *r, EngPipelineHandle handle);
/* Format de la cible (SDL_GPUTextureFormat) pour lequel le pipeline a été créé ; 0 si le handle
 * est invalide. */
uint32_t eng_renderer_pipeline_color_format(const EngRenderer *r, EngPipelineHandle handle);
/* Format de profondeur (SDL_GPUTextureFormat) que le pipeline attend dans sa passe ; 0 s'il n'en
 * attend aucun (renderer sans depth buffer, ou pipeline créé avec no_depth_target) ou si le handle
 * est invalide. */
uint32_t eng_renderer_pipeline_depth_format(const EngRenderer *r, EngPipelineHandle handle);

/* Build avec rechargement (Debug) : surveille les shaders des pipelines et recrée ceux dont un
 * shader a changé, SANS changer leur handle ; si la recréation échoue, l'ancien pipeline reste
 * en service. Autres builds : sans effet. Retourne le nombre de pipelines recréés. À appeler hors
 * du compteur d'allocations (c'est un outil de développement). */
uint32_t eng_renderer_update(EngRenderer *r, double now_seconds);

/* Format (SDL_GPUTextureFormat) de la profondeur que les pipelines attendent dans la passe où ils
 * dessinent ; 0 si le renderer n'a pas de depth buffer. Utile pour fabriquer une passe sur une
 * texture (tests, captures) compatible avec les pipelines. */
uint32_t eng_renderer_depth_format(const EngRenderer *r);

/* HDR (M1-14) ──────────────────────────────────────────────────────────── */

/* true si la scène est dessinée en HDR puis résolue (EngRendererDesc.hdr, mise en place réussie).
 */
bool eng_renderer_hdr_enabled(const EngRenderer *r);
/* Format (SDL_GPUTextureFormat) de la cible des pipelines créés sans color_format : RGBA16F avec
 * le HDR, sinon le format de la swapchain. */
uint32_t eng_renderer_scene_format(const EngRenderer *r);
/* Réglages de la résolution HDR : copie, remise dans le domaine valide (eng_tonemap_sanitize).
 * Prise en compte à la prochaine résolution. Sans effet si `t` est nul ou le renderer arrêté. */
void eng_renderer_set_tonemap(EngRenderer *r, const EngTonemap *t);
EngTonemap eng_renderer_tonemap(const EngRenderer *r);
/* Dessine la texture HDR `hdr` plein écran dans `pass` avec les réglages courants (c'est ce
 * qu'appelle eng_renderer_end_frame via EngGpu ; les tests l'appellent à la main sur une texture).
 * `pass` doit avoir une cible du format de résolution et AUCUNE profondeur. false si le HDR n'est
 * pas en service ou si un paramètre est nul. */
bool eng_renderer_resolve_hdr(EngRenderer *r, struct SDL_GPUCommandBuffer *commands,
                              struct SDL_GPURenderPass *pass, struct SDL_GPUTexture *hdr);

/* Couleur de fond de l'image (composantes dans [0, 1]). */
void eng_renderer_set_clear_color(EngRenderer *r, float red, float green, float blue, float alpha);

/* Installe (ou, avec `draw` nul, retire) l'overlay. Appelés à chaque image qui a une cible. */
void eng_renderer_set_overlay(EngRenderer *r, EngOverlayPrepareFn prepare, EngOverlayDrawFn draw,
                              void *user);

/* Vide la file de l'image et la rend pour enregistrement. NULL si le renderer est arrêté. */
EngRenderQueue *eng_renderer_begin_frame(EngRenderer *r);

/* Prend l'image de la swapchain, y exécute la file, soumet et présente. Si aucune image n'est
 * disponible (fenêtre réduite), la file est ignorée. */
void eng_renderer_end_frame(EngRenderer *r);

/* Exécute la file dans une passe de rendu déjà ouverte (celle de end_frame, ou une passe sur une
 * texture : tests, captures). `commands` est le tampon de commandes de la passe : il sert à
 * envoyer les uniforms. Les dessins dont un handle est invalide, ou dont les uniforms sont
 * incohérents, sont ignorés, comptés dans les statistiques et signalés une fois dans le journal.
 * Retourne le nombre de dessins émis. */
uint32_t eng_renderer_execute(EngRenderer *r, const EngRenderQueue *queue,
                              struct SDL_GPUCommandBuffer *commands,
                              struct SDL_GPURenderPass *pass);
