#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Contexte GPU (SDL_GPU) : périphérique, rattachement à la fenêtre, swapchain, présentation.
 * Cet en-tête n'expose aucun type SDL_GPU (seulement des pointeurs opaques) : game/ n'en dépend
 * pas. Le vrai module de rendu (handles, file de commandes) arrive en M1-05 et s'appuiera sur
 * celui-ci.
 *
 * Utilisation, une fois par image :
 *     if (eng_gpu_begin_frame(&gpu)) {
 *         struct SDL_GPURenderPass *pass = eng_gpu_begin_render_pass(&gpu);
 *         if (pass != NULL) { ...dessin... }
 *     }
 *     eng_gpu_end_frame(&gpu);
 * La passe efface l'image à la couleur choisie. `end_frame` la termine (ou, si personne ne l'a
 * ouverte, efface simplement l'image), soumet et présente. */

typedef enum EngPresentMode {
    ENG_PRESENT_VSYNC,   /* synchronisation verticale : toujours disponible */
    ENG_PRESENT_MAILBOX, /* sans déchirure, sans attente : l'image la plus récente est affichée */
    ENG_PRESENT_IMMEDIATE, /* aucune synchronisation : déchirure possible, latence minimale */
} EngPresentMode;

/* Images en vol (M1-18) : nombre d'images que le CPU peut préparer pendant que le GPU en dessine
 * d'autres. 1 : le CPU attend la fin de chaque image (latence minimale, aucun recouvrement) ; 2 :
 * valeur par défaut (une image se prépare pendant que l'autre se dessine) ; 3 : plus de débit, plus
 * de latence. Chaque image soumise reçoit une barrière (fence) ; `begin_frame` attend la plus
 * ancienne quand le maximum est atteint, quel que soit le mode de présentation. */
#define ENG_GPU_MAX_FRAMES_IN_FLIGHT 3U
#define ENG_GPU_DEFAULT_FRAMES_IN_FLIGHT 2U

typedef struct EngGpuDesc {
    bool debug;                  /* validation du pilote (couches de validation si installées) */
    EngPresentMode present_mode; /* mode demandé ; repli sur VSYNC si indisponible */
    uint32_t frames_in_flight; /* 1 à 3 ; 0 : valeur par défaut (2) ; hors de 1..3 : par défaut */
} EngGpuDesc;

/* Appelée quand la taille des cibles du moteur (swapchain, profondeur, HDR) change, après leur
 * recréation, et à la toute première image avec la taille initiale. `width` et `height` sont en
 * pixels. Pour les cibles qui dépendent de la taille de l'écran (SSAO, bloom...) : les recréer ici.
 */
typedef void (*EngGpuResizeFn)(void *user, uint32_t width, uint32_t height);

typedef struct EngGpu {
    struct SDL_GPUDevice *device;
    struct SDL_Window *window;
    struct SDL_GPUCommandBuffer *command_buffer; /* image en cours, NULL hors image */
    struct SDL_GPUTexture *swapchain_texture;    /* cible de l'image en cours */
    struct SDL_GPURenderPass *render_pass; /* passe ouverte (principale ou overlay), ou NULL */
    bool overlay_open;                     /* la passe ouverte est l'overlay */
    bool main_pass_ended;                  /* la passe principale a été terminée (overlay) */
    uint32_t swapchain_width;              /* taille de la cible, en pixels */
    uint32_t swapchain_height;
    struct SDL_GPUTexture *depth_texture; /* profondeur de la passe swapchain (si activée) */
    uint32_t depth_width;                 /* taille de depth_texture */
    uint32_t depth_height;
    uint32_t depth_format; /* SDL_GPUTextureFormat retenu ; 0 : profondeur désactivée */
    bool hdr_enabled;      /* la scène est dessinée en HDR puis résolue (M1-14) */
    struct SDL_GPUTexture *hdr_texture; /* cible couleur HDR de la passe principale (si activée) */
    uint32_t hdr_width;                 /* taille de hdr_texture */
    uint32_t hdr_height;
    void (*resolve)(void *user, struct SDL_GPUCommandBuffer *commands,
                    struct SDL_GPURenderPass *pass, struct SDL_GPUTexture *hdr);
    void *resolve_user;
    uint64_t resolve_passes;     /* passes de résolution HDR -> swapchain exécutées (M1-14) */
    EngPresentMode present_mode; /* mode réellement actif */
    float clear_color[4];
    uint64_t frames_submitted; /* images soumises et présentées */
    uint64_t frames_skipped;   /* images sans cible (fenêtre réduite) ou en erreur */
    uint64_t overlay_passes;   /* passes overlay ouvertes (M1-12) */
    /* Images en vol et barrières (M1-18) */
    uint32_t frames_in_flight; /* maximum d'images en cours (1 à 3) */
    struct SDL_GPUFence
        *fences[ENG_GPU_MAX_FRAMES_IN_FLIGHT]; /* images soumises, la plus ancienne d'abord */
    uint32_t fence_count;     /* images soumises dont la fin n'a pas encore été constatée */
    uint32_t max_pending;     /* plus grand nombre d'images en vol observé */
    uint64_t fence_waits;     /* fois où begin_frame a dû attendre le GPU */
    uint64_t fences_acquired; /* barrières obtenues à la soumission */
    uint64_t fences_released; /* barrières rendues : égal à fences_acquired à l'arrêt */
    /* Cibles qui dépendent de la taille de l'écran (M1-18) */
    uint64_t targets_created;  /* textures de profondeur et HDR créées */
    uint64_t targets_released; /* textures de profondeur et HDR libérées */
    uint32_t target_width;     /* dernière taille annoncée au rappel de redimensionnement */
    uint32_t target_height;
    uint64_t resizes; /* changements de taille constatés (première image comprise) */
    EngGpuResizeFn on_resize;
    void *on_resize_user;
    bool (*fence_query)(void *user, struct SDL_GPUFence *fence); /* NULL : SDL_QueryGPUFence */
    bool (*fence_wait)(void *user, struct SDL_GPUFence *fence);  /* NULL : SDL_WaitForGPUFences */
    void *fence_user;
} EngGpu;

/* Crée le périphérique (Vulkan, D3D12 ou Metal selon la plateforme) et le rattache à
 * `window`. false si aucun pilote GPU utilisable (message dans le journal) : l'appelant peut
 * alors se rabattre sur l'affichage logiciel. */
bool eng_gpu_init(EngGpu *gpu, struct SDL_Window *window, const EngGpuDesc *desc);

/* Attend la fin du travail GPU puis libère tout. Sans effet sur une structure déjà arrêtée (true).
 * Retourne true si toutes les barrières et toutes les cibles créées ont été rendues (M1-18) ;
 * false, et une erreur au journal, si l'une d'elles manque. */
bool eng_gpu_shutdown(EngGpu *gpu);

/* Nom du pilote retenu (« vulkan », « direct3d12 », « metal »), « aucun » si arrêté. */
const char *eng_gpu_driver_name(const EngGpu *gpu);

/* Change le mode de présentation. false (et mode inchangé) s'il n'est pas disponible. */
bool eng_gpu_set_present_mode(EngGpu *gpu, EngPresentMode mode);

/* Couleur d'effacement, composantes dans [0, 1], prise en compte à la prochaine image. */
void eng_gpu_set_clear_color(EngGpu *gpu, float r, float g, float b, float a);

/* Prend la prochaine image de la swapchain ; peut attendre (c'est ce qui cadence la
 * synchronisation verticale). Retourne false s'il n'y a rien à dessiner (fenêtre réduite) ou en
 * cas d'erreur : l'image est alors ignorée, `end_frame` reste sans danger. */
bool eng_gpu_begin_frame(EngGpu *gpu);

/* Efface l'image, la soumet et la présente. Sans effet si `begin_frame` a retourné false. */
void eng_gpu_end_frame(EngGpu *gpu);

/* Ouvre la passe de rendu de l'image sur la swapchain, effacée à la couleur choisie. NULL s'il
 * n'y a pas d'image en cours (begin_frame a échoué) ou si la passe est déjà ouverte. Elle est
 * terminée par `end_frame` : ne pas appeler SDL_EndGPURenderPass dessus. */
struct SDL_GPURenderPass *eng_gpu_begin_render_pass(EngGpu *gpu);

/* Active la profondeur : la passe de l'image reçoit alors une texture de profondeur de la taille
 * de la swapchain (recréée quand la fenêtre est redimensionnée), effacée à 1,0 (plan lointain) au
 * début de la passe. À appeler AVANT de créer des pipelines qui dessinent dans cette passe : ils
 * doivent déclarer le même format (eng_gpu_depth_format). Choisit D32_FLOAT, sinon D24, sinon D16.
 * false si aucun format de profondeur n'est utilisable. Sans effet (true) si déjà activée. */
bool eng_gpu_enable_depth(EngGpu *gpu);

/* Format (SDL_GPUTextureFormat) de la profondeur de la passe swapchain ; 0 si désactivée. */
uint32_t eng_gpu_depth_format(const EngGpu *gpu);

/* Réglages de la cible de profondeur d'une passe : effacée à 1,0 (le plus loin) au début, non
 * conservée à la fin. Partagés par la passe swapchain et par les tests hors écran, pour que les
 * tests de pixels vérifient les vrais réglages. */
// NOLINTNEXTLINE(readability-identifier-naming): type SDL
struct SDL_GPUDepthStencilTargetInfo;
void eng_gpu_fill_depth_target(struct SDL_GPUTexture *texture,
                               struct SDL_GPUDepthStencilTargetInfo *out);

/* Passe « overlay » (M1-12 : interface de debug) : une seconde passe sur la MÊME image, qui
 * CONSERVE ce que la première a dessiné (chargement de la couleur au lieu d'un effacement) et n'a
 * PAS de profondeur : l'interface se dessine par-dessus la scène, sans être masquée ni masquer. Les
 * pipelines qui y dessinent ne doivent donc déclarer aucun format de profondeur.
 * Termine la passe principale (ou, si personne ne l'avait ouverte, l'ouvre et l'efface d'abord),
 * puis ouvre l'overlay, que `eng_gpu_end_frame` termine. NULL s'il n'y a pas d'image en cours ou si
 * l'overlay est déjà ouvert. Tout envoi de données au GPU (copie) doit avoir eu lieu AVANT la
 * première passe de l'image. */
struct SDL_GPURenderPass *eng_gpu_begin_overlay_pass(EngGpu *gpu);

/* Réglages de la cible couleur de l'overlay : conserve le contenu existant (LOAD) et le stocke.
 * Partagés avec les tests hors écran, pour qu'ils vérifient les vrais réglages. */
// NOLINTNEXTLINE(readability-identifier-naming): type SDL
struct SDL_GPUColorTargetInfo;
void eng_gpu_fill_overlay_target(struct SDL_GPUTexture *texture,
                                 struct SDL_GPUColorTargetInfo *out);

/* Format des pixels de la swapchain (SDL_GPUTextureFormat), nécessaire pour créer un pipeline qui
 * dessine dessus ; 0 (invalide) si le GPU est arrêté. */
uint32_t eng_gpu_swapchain_format(const EngGpu *gpu);

/* ── HDR (M1-14) ──
 *
 * Avec le HDR, la passe principale ne dessine plus dans la swapchain mais dans une texture
 * RGBA16F de la taille de la swapchain (recréée au redimensionnement) : la lumière linéaire peut
 * dépasser 1,0. Quand la passe principale se termine, le moteur ouvre une passe de RÉSOLUTION sur
 * la swapchain (effacée en noir) et appelle la fonction installée par eng_gpu_set_resolve, qui y
 * dessine la texture HDR (tonemapping, gamma : le renderer fournit cette fonction). L'overlay
 * (interface) vient ensuite, sur la swapchain, donc sans tonemapping. Sans fonction de résolution,
 * l'image reste noire. */

/* Fonction de résolution : dessine `hdr` (lisible comme texture, taille de la swapchain) dans
 * `pass`, la passe ouverte sur la swapchain ; ne doit pas la terminer. */
typedef void (*EngGpuResolveFn)(void *user, struct SDL_GPUCommandBuffer *commands,
                                struct SDL_GPURenderPass *pass, struct SDL_GPUTexture *hdr);

/* Active le HDR. À appeler AVANT de créer des pipelines qui dessinent dans la passe principale :
 * ils doivent déclarer eng_gpu_scene_format. false (journalisé) si le GPU ne sait pas dessiner
 * dans une texture RGBA16F lue ensuite comme texture. Sans effet (true) si déjà activé. */
bool eng_gpu_enable_hdr(EngGpu *gpu);

/* Désactive le HDR et libère sa texture : la passe principale redessine dans la swapchain. Appelé
 * par le renderer qui l'avait activé, à son arrêt. Retire aussi la fonction de résolution. */
void eng_gpu_disable_hdr(EngGpu *gpu);

/* Format (SDL_GPUTextureFormat) des textures HDR : RGBA 16 bits flottants. */
uint32_t eng_gpu_hdr_format(void);

/* Format de la cible couleur de la passe principale : HDR si activé, sinon celui de la swapchain
 * (0 si le GPU est arrêté). */
uint32_t eng_gpu_scene_format(const EngGpu *gpu);

/* Installe (ou, avec `fn` nul, retire) la fonction de résolution HDR. */
void eng_gpu_set_resolve(EngGpu *gpu, EngGpuResolveFn fn, void *user);

/* Réglages de la cible couleur de la passe de résolution : effacée en noir, conservée. Partagés
 * avec les tests hors écran, pour qu'ils vérifient les vrais réglages. */
void eng_gpu_fill_resolve_target(struct SDL_GPUTexture *texture,
                                 struct SDL_GPUColorTargetInfo *out);

/* ── Images en vol et barrières (M1-18) ── */

/* Maximum d'images en vol actuellement autorisé (1 à 3), 0 si le GPU est arrêté. */
uint32_t eng_gpu_frames_in_flight(const EngGpu *gpu);

/* Change le maximum d'images en vol. false (rien changé) hors de 1..3, si le GPU est arrêté ou si
 * le pilote refuse. Une réduction prend effet à la prochaine image : begin_frame attend alors que
 * le GPU rattrape son retard. */
bool eng_gpu_set_frames_in_flight(EngGpu *gpu, uint32_t count);

/* Images soumises dont le GPU n'a pas encore fini (interroge les barrières, sans attendre). */
uint32_t eng_gpu_frames_pending(EngGpu *gpu);

/* Attend la fin de TOUTES les images soumises et rend leurs barrières. À appeler avant de détruire
 * une ressource que ces images utilisent, ou pour une mesure. Sans effet hors image en cours. */
void eng_gpu_wait_idle(EngGpu *gpu);

/* Vérifie qu'aucune ressource de cible ni barrière n'a fui : barrières obtenues = rendues + en vol,
 * textures de cible créées = libérées + vivantes. Pour les tests et le contrôle à l'arrêt. */
bool eng_gpu_resources_balanced(const EngGpu *gpu);

/* Remplace l'interrogation et l'attente des barrières (avec `query` et `wait` nuls : SDL). Réservé
 * aux tests : sur une machine rapide ou un GPU logiciel, le GPU a presque toujours fini avant que
 * le CPU revienne, et la limite d'images en vol n'est jamais atteinte. Un GPU simulé permet de
 * tester la règle d'attente sans dépendre de la vitesse de la machine. `query` dit si l'image est
 * finie ; `wait` retourne true une fois qu'elle l'est (les barrières réelles sont toujours rendues
 * à SDL). */
void eng_gpu_set_fence_hooks(EngGpu *gpu, bool (*query)(void *user, struct SDL_GPUFence *fence),
                             bool (*wait)(void *user, struct SDL_GPUFence *fence), void *user);

/* Installe (ou, avec `fn` nul, retire) le rappel de redimensionnement des cibles. */
void eng_gpu_set_resize_callback(EngGpu *gpu, EngGpuResizeFn fn, void *user);
