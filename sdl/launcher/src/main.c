#include <SDL3/SDL_events.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <math.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/equipment.h"
#include "engine/assert.h"
#include "engine/clock.h"
#include "engine/camera.h"
#include "engine/config.h"
#include "engine/engine.h"
#include "engine/files.h"
#include "engine/game_host.h"
#include "engine/gpu.h"
#include "engine/lsan.h"
#include "engine/log.h"
#include "engine/loop.h"
#include "engine/memory.h"
#include "engine/platform.h"
#include "engine/profile.h"
#include "engine/renderer.h"
#include "engine/frame_stats.h"
#include "engine/lighting.h"
#include "engine/shadow.h"
#include "engine/tweak.h"
#include "engine/ui.h"

#include "demo_scene.h"
#include "editor_mode.h"
#if defined(ENG_HAVE_CRASH)
#include "engine/crash.h"
#endif
#include "game_flow.h"
#include "engine/daynight.h"
#include "post_bench.h"
#include "play_mode.h"
#include "world_bench.h"
#include "world_view.h"
#include "skeleton_view.h"
#include "splash.h"
#include "title.h"
#include "avatar_gallery.h"

#define LAUNCHER_SSAO 0.6F /* force du SSAO quand il est activé (finition M8-06) */

ENG_LSAN_DEFAULT_SUPPRESSIONS();

static void usage(const char *prog)
{
    fprintf(stderr, "usage : %s [--frames N] [--log-level trace|debug|info|warn|error]\n", prog);
    fprintf(stderr, "          [--game chemin_de_la_bibliotheque] [--config chemin]\n");
    fprintf(
        stderr,
        "          [--software] [--assert-test] [--no-mipmaps] [--anisotropy N] [--ui]\n"
        "          [--env image.hdr] [--env-samples N] [--frames-in-flight N] [--stress-resize]\n"
        "          [--watch-assets] [--sun-elevation DEG] [--sun-azimuth DEG]\n          "
        "[--day-night HEURE] [--day-length S]\n          "
        "[--fog-density 1/M] [--exposure EV] [--ambient X] [--rocks N] [--placement]\n          "
        "[--placement-seed N] [--grass]\n          "
        "[--water] [--water-level M]\n");
    fprintf(stderr, "  --frames N     quitte après N images (tests automatiques)\n");
    fprintf(stderr,
            "  --log-level L  niveau du journal (sinon variable ENG_LOG_LEVEL, sinon info)\n");
    fprintf(stderr,
            "  --game PATH    bibliothèque du jeu (défaut : game.so à côté de l'exécutable)\n");
    fprintf(
        stderr,
        "  --config PATH  fichier de configuration (défaut : config.ini à côté de l'exécutable)\n");
    fprintf(stderr,
            "  --watch-assets démo : recuit et recharge le modèle quand ses sources changent\n");
    fprintf(
        stderr,
        "  --terrain PATH démo : dessine le champ de hauteurs .hmap (assetc heightmap) à la place "
        "de la démonstration ; point de vue au-dessus du centre\n");
    fprintf(stderr,
            "  --editor       éditeur : charge la scène, clic = sélection, flèches = déplacement, "
            "fenêtre Éditeur (Enregistrer)\n");
    fprintf(stderr, "  --scene PATH   scène de l'éditeur (défaut " EDITOR_DEFAULT_SCENE
                    ", relatif à l'exécutable ; chemin absolu accepté)\n");
    fprintf(stderr,
            "  --editor-selftest (développement) clique, déplace, enregistre et relit, puis "
            "journalise AUTOTEST ÉDITEUR\n");
    fprintf(
        stderr,
        "  --play         mode jouable : personnage, caméra 3e personne, saut, sprint, arène en "
        "boîtes grises (ZQSD, souris, Espace, Maj)\n");
    fprintf(stderr, "  --skeleton F   affiche le squelette F (.skel, voir `assetc skel`) en os "
                    "superposés au modèle central\n");
    fprintf(stderr, "  --play-selftest (développement) autotest du mode jouable sans fenêtre, "
                    "journalise AUTOTEST JEU\n");
    fprintf(stderr, "  --combat       mode jouable + combat : mannequin, camp de 3 ennemis (clic "
                    "gauche/droit, Ctrl, E, Q) ; --no-audio : sans son\n");
    fprintf(stderr,
            "  --crash-test   (développement) plantage volontaire : écrit logs/crash-*.txt\n");
    fprintf(stderr, "  --new-game     avec --combat : saute le menu principal (nouvelle partie)\n");
    fprintf(stderr,
            "  --game-selftest (développement) partie, sauvegarde, relance, reprise sans fenêtre ; "
            "journalise AUTOTEST PARTIE\n");
    fprintf(stderr, "  --combat-autoplay  comme --combat, joué par un pilote automatique "
                    "(démonstration)\n");
    fprintf(stderr,
            "  --at-boss      avec --combat --terrain : commence devant l'entrée de l'arène "
            "du boss (M6-09)\n");
    fprintf(stderr,
            "  --gear N       avec --combat : commence avec le palier d'équipement N (0 à 4 : "
            "butin des boss de la grande carte)\n");
    fprintf(stderr, "  --at-pos X Z   avec --world : départ au point (x, z) de la carte du monde "
                    "(mètres ; développement)\n");
    fprintf(stderr, "  --no-splash    pas d'écran de chargement animé (chargement direct, comme "
                    "avec --frames et les autotests)\n");
    fprintf(stderr, "  --splash       (développement) écran de chargement même avec --frames\n");
    fprintf(stderr, "  --windowed     fenêtre au lieu du plein écran (le jeu s'ouvre en plein "
                    "écran par défaut)\n");
    fprintf(stderr, "  --creative     avec --new-game : la partie est en mode créatif (invincible, "
                    "vol) ; sinon le mode se choisit au menu, à la création de la partie\n");
    fprintf(stderr, "  --creative-speed V  vitesse de vol du mode créatif au départ, de 0.1 à 1000 "
                    "m/s (25 par défaut ; en jeu : la molette)\n");
    fprintf(stderr,
            "  --look-at X Z  (développement) la caméra regarde le point (x, z) de la carte "
            "au départ ; --look-pitch D : inclinée de D degrés (positif : vers le haut)\n");
    fprintf(stderr, "  --look-at-boss (développement) la caméra suit le boss spécial de la zone "
                    "(le dragon en vol)\n");
    fprintf(stderr, "  --warp N       (développement) joue N ticks de jeu d'un coup à la troisième "
                    "image (60 par seconde de jeu) : captures et tests du vol du dragon\n");
    fprintf(stderr, "  --menu-script S  (développement) joue des touches dans le menu : "
                    "« image:touche,... » avec up, down, left, right, ok, back, esc (événement "
                    "Échap), bs (effacer), text=MOT, mouse=X/Y, click\n");
    fprintf(stderr, "  --shots DOSSIER N1,N2,...  (développement) écrit les images n° N1, N2... "
                    "en PPM dans DOSSIER\n");
    fprintf(stderr, "  --save-dir DOSSIER  parties et options dans ce dossier (au lieu du dossier "
                    "de préférences)\n");
    fprintf(stderr, "  --save-selftest  (développement) avec --world --combat --new-game : "
                    "enregistre, s'éloigne, reprend ; journalise AUTOTEST SAUVEGARDE MONDE\n");
    fprintf(stderr, "  --splash-shots DOSSIER  (développement) écrit des images de l'écran de "
                    "chargement en PPM, puis quitte\n");
    fprintf(stderr, "  --at-zone N    avec --world --combat : commence 20 m avant la zone N de la "
                    "grande carte (0 à 11 : farm facile, farm dur, boss de chaque biome)\n");
    fprintf(stderr,
            "  --at-pack N    avec --world --combat : commence 20 m avant la meute de petits "
            "monstres N de la carte dessinée (0 : la première ligne « meute »)\n");
    fprintf(stderr,
            "  --post-bench N mesure le coût de chaque effet (FXAA, bloom, SSAO, échelle) : N "
            "images par configuration, sans vsync, puis quitte\n");
    fprintf(stderr, "  --screenshot F N (développement) écrit l'image n° N en PPM dans F\n");
    fprintf(stderr, "  --avatar-gallery (développement) rangée de chevaliers, un état d'animation "
                    "chacun (repos, marche, attaques au tick d'impact, roulade...)\n");
    fprintf(stderr,
            "  --world-bench [KM] benchmark de traversée du monde ouvert (défaut 10 km à 30 "
            "m/s), sans fenêtre ; --world-bench-report F : rapport écrit dans F\n");
    fprintf(stderr, "  --combat-rig-selftest (développement) le même, avec les personnages "
                    "animés : hurtboxes sur les os, lame en main\n");
    fprintf(stderr, "  --boss-selftest (développement) autotest des boss spéciaux de la grande "
                    "carte (cavalier, dragon), sans fenêtre\n");
    fprintf(stderr, "  --combat-selftest (développement) autotest du combat sans fenêtre, "
                    "journalise AUTOTEST COMBAT\n");
    fprintf(stderr, "  --no-mipmaps   démo : texture du cube sans mipmaps (montre l'aliasing)\n");
    fprintf(stderr, "  --anisotropy N démo : filtrage anisotrope de 0 (aucun) à 16 (défaut 16)\n");
    fprintf(stderr, "  --env FICHIER  démo : ciel = image équirectangulaire Radiance .hdr (sinon "
                    "ciel procédural)\n");
    fprintf(stderr, "  --env-samples N démo : échantillons du préfiltrage du ciel (défaut 512 ; 16 "
                    ": rapide)\n");
    fprintf(stderr,
            "  --frames-in-flight N images préparées par le CPU pendant que le GPU dessine (1 à 3, "
            "défaut 2)\n");
    fprintf(stderr,
            "  --stress-resize (développement) change la taille de la fenêtre à chaque image, "
            "puis vérifie\n                 qu'aucune cible ni barrière n'a fui (code de sortie 1 "
            "sinon)\n");
    fprintf(stderr,
            "  --ui           démarre avec l'interface de debug affichée (sinon F1 l'affiche)\n");
    fprintf(stderr, "  --software     affichage logiciel (sans GPU) : montre le carré du jeu\n");
    fprintf(stderr, "  --assert-test  (développement) déclenche volontairement deux assertions\n");
}

/* Dessins de debug du lanceur (mode jouable, squelette), appelés par la scène
 * (demo_scene_set_debug_hook). */
typedef struct PlayHook {
    const PlayMode *play;   /* NULL : pas de mode jouable */
    SkeletonView *skeleton; /* NULL : pas de squelette */
    double alpha;
} PlayHook;

/* Personnages animés (M4) : dessinés par la scène dans ses deux passes. */
static void launcher_model_hook(void *user, EngRenderQueue *queue, mat4 view_proj,
                                const EngLighting *light, const EngModelShadow *shadow,
                                bool shadow_pass)
{
    const PlayHook *hook = user;
    if (hook->play != NULL) {
        play_mode_draw_models(hook->play, queue, view_proj, light, shadow, shadow_pass);
    }
}

/* --avatar-gallery : la rangée d'états d'animation. */
typedef struct GalleryHook {
    AvatarGallery *gallery;
    AvatarLibrary *lib;
} GalleryHook;

static void gallery_model_hook(void *user, EngRenderQueue *queue, mat4 view_proj,
                               const EngLighting *light, const EngModelShadow *shadow,
                               bool shadow_pass)
{
    const GalleryHook *hook = user;
    avatar_gallery_draw(hook->gallery, hook->lib, queue, view_proj, light, shadow, shadow_pass);
}

/* --world (M7) : les cellules du monde streamé, puis les personnages. */
typedef struct WorldHook {
    WorldView *world;
    PlayHook *play_hook;
    const DemoScene *scene;
} WorldHook;

static void world_model_hook(void *user, EngRenderQueue *queue, mat4 view_proj,
                             const EngLighting *light, const EngModelShadow *shadow,
                             bool shadow_pass)
{
    WorldHook *hook = user;
    if (shadow_pass) {
        world_view_draw_depth(hook->world, queue, view_proj);
    } else {
        if (hook->scene->has_atmosphere) {
            world_view_set_fog(hook->world, &hook->scene->fog, &hook->scene->atmosphere,
                               hook->scene->last_sky_intensity, hook->scene->last_true_sun);
        }
        world_view_draw(hook->world, queue, view_proj, light, shadow);
    }
    launcher_model_hook(hook->play_hook, queue, view_proj, light, shadow, shadow_pass);
}

static bool world_ground(void *user, float x, float z, float *out)
{
    return world_view_height(user, x, z, out);
}

/* ── Sauvegarde dans le monde ouvert (game_flow.h : GameFlowWorld) ── */

typedef struct FlowWorld {
    WorldView *world;
    PlayMode *play;
    EngCamera *camera;
    EngCamera *camera_prev;
} FlowWorld;

static void flow_to_world(void *user, const float local[3], double world[3])
{
    const FlowWorld *h = user;
    eng_origin_to_world(&h->world->origin, local, world);
}

/* Charge le relief autour de `at` (monde) en bloquant, répercute le recentrage éventuel de
 * l'origine sur le jeu et les caméras, puis pose le joueur (au sol s'il était dessous). */
static bool flow_place(void *user, const double at[3], float yaw)
{
    const FlowWorld *h = user;
    if (!isfinite(at[0]) || !isfinite(at[1]) || !isfinite(at[2])) {
        return false;
    }
    const EngWorldOrigin before = h->world->origin;
    if (!world_view_prime(h->world, at, 1, 60.0)) {
        return false;
    }
    const float shift[3] = {(float)(before.x - h->world->origin.x),
                            (float)(before.y - h->world->origin.y),
                            (float)(before.z - h->world->origin.z)};
    if (shift[0] != 0.0F || shift[1] != 0.0F || shift[2] != 0.0F) {
        play_mode_origin_shift(h->play, shift);
        for (int a = 0; a < 3; a++) {
            h->camera->position[a] += shift[a];
            h->camera_prev->position[a] += shift[a];
        }
    }
    float feet[3];
    eng_origin_to_local(&h->world->origin, at, feet);
    float ground = 0.0F;
    if (world_view_height(h->world, feet[0], feet[2], &ground) && feet[1] < ground + 0.05F) {
        feet[1] = ground + 0.05F;
    }
    play_mode_teleport(h->play, feet, yaw);
    ENG_LOG_INFO("monde : joueur replacé en (%.1f, %.1f) du monde, à %.1f m d'altitude%s", at[0],
                 at[2], (double)feet[1],
                 shift[0] != 0.0F || shift[2] != 0.0F ? " (origine recentrée)" : "");
    return true;
}

/* Nom du lieu d'un point de la carte, pour la carte de la partie dans le menu : le château près
 * du point de départ, sinon le biome dominant. */
static void flow_place_name(void *user, const double at[3], char *out, size_t capacity)
{
    static const char *const names[ENG_BIOME_COUNT] = {"Plaine", "Montagnes", "Désert", "Volcan"};
    const FlowWorld *h = user;
    out[0] = '\0';
    const EngWorldMap *map = h->world->map;
    if (map == NULL) {
        return;
    }
    if (hypot(at[0] - (double)map->spawn[0], at[2] - (double)map->spawn[1]) < 70.0) {
        (void)snprintf(out, capacity, "Château");
        return;
    }
    float weights[ENG_BIOME_COUNT];
    eng_worldmap_weights(map, at[0], at[2], weights, NULL);
    uint32_t best = 0;
    for (uint32_t b = 1; b < ENG_BIOME_COUNT; b++) {
        best = weights[b] > weights[best] ? b : best;
    }
    (void)snprintf(out, capacity, "%s", names[best]);
}

/* --menu-script « image:touche,... » (développement, tests) : des touches jouées dans le menu à
 * des images données. Touches : up, down, left, right, ok, back ; bs efface un caractère ;
 * text=MOT tape MOT ; mouse=X/Y place la souris (fractions de l'écran, de 0 à 1) ; click clique ;
 * esc envoie un vrai événement Échap à la fenêtre, et type=MOT un vrai événement de texte : ils
 * passent par la plateforme, comme au clavier. */
#define MENU_SCRIPT_MAX 96U

typedef struct MenuScript {
    struct {
        long frame;
        uint32_t keys;
        char text[40];
        float mouse[2]; /* fractions de l'écran ; négatif : pas de déplacement */
        bool click;
        bool escape; /* événement clavier Échap, traité par la plateforme à l'image suivante */
        bool typed;  /* `text` part en événement de texte (plateforme) au lieu d'aller au menu */
        float wheel; /* crans de molette (0 : aucun) */
    } step[MENU_SCRIPT_MAX];
    uint32_t count;
    uint32_t next;
    uint32_t pending; /* touches en attente du prochain tick */
} MenuScript;

static bool menu_script_parse(MenuScript *s, const char *spec)
{
    memset(s, 0, sizeof *s);
    const char *p = spec;
    while (p != NULL && *p != '\0' && s->count < MENU_SCRIPT_MAX) {
        char *end = NULL;
        const long frame = strtol(p, &end, 10);
        if (end == p || *end != ':') {
            return false;
        }
        const char *key = end + 1;
        const char *stop = strchr(key, ',');
        const size_t len = stop != NULL ? (size_t)(stop - key) : strlen(key);
        s->step[s->count].frame = frame;
        s->step[s->count].mouse[0] = -1.0F;
        if (len > 6U && strncmp(key, "mouse=", 6) == 0) {
            if (sscanf(key + 6, "%f/%f", &s->step[s->count].mouse[0],
                       &s->step[s->count].mouse[1]) != 2) {
                return false;
            }
        } else if (len == 5U && strncmp(key, "click", 5) == 0) {
            s->step[s->count].click = true;
        } else if (len == 3U && strncmp(key, "esc", 3) == 0) {
            s->step[s->count].escape = true;
        } else if (len > 6U && strncmp(key, "wheel=", 6) == 0) {
            s->step[s->count].wheel = strtof(key + 6, NULL);
        } else if (len > 5U && strncmp(key, "type=", 5) == 0) {
            (void)snprintf(s->step[s->count].text, sizeof s->step[s->count].text, "%.*s",
                           (int)(len - 5U), key + 5);
            s->step[s->count].typed = true;
        } else if (len > 5U && strncmp(key, "text=", 5) == 0) {
            (void)snprintf(s->step[s->count].text, sizeof s->step[s->count].text, "%.*s",
                           (int)(len - 5U), key + 5);
        } else if (len == 2U && strncmp(key, "bs", 2) == 0) {
            (void)snprintf(s->step[s->count].text, sizeof s->step[s->count].text, "\b");
        } else {
            static const struct {
                const char *name;
                uint32_t key;
            } keys[] = {{"up", ENG_MENU_KEY_UP},     {"down", ENG_MENU_KEY_DOWN},
                        {"left", ENG_MENU_KEY_LEFT}, {"right", ENG_MENU_KEY_RIGHT},
                        {"ok", ENG_MENU_KEY_OK},     {"back", ENG_MENU_KEY_BACK}};
            for (size_t k = 0; k < sizeof keys / sizeof keys[0]; k++) {
                if (strlen(keys[k].name) == len && strncmp(key, keys[k].name, len) == 0) {
                    s->step[s->count].keys = keys[k].key;
                }
            }
            if (s->step[s->count].keys == 0U) {
                return false;
            }
        }
        s->count++;
        p = stop != NULL ? stop + 1 : NULL;
    }
    return true;
}

/* les pas de l'image `frame` : le texte va au menu, les touches attendent le prochain tick, la
 * souris passe par l'interface (position) et par les entrées (clic gauche) */
static void menu_script_frame(MenuScript *s, GameFlow *flow, uint64_t frame, EngUi *ui,
                              EngPlatform *platform)
{
    while (s->next < s->count && s->step[s->next].frame <= (long)frame) {
        if (s->step[s->next].typed) { /* le texte reste en place : la file le lit plus tard */
            SDL_Event ev;
            SDL_zero(ev);
            ev.type = SDL_EVENT_TEXT_INPUT;
            ev.text.text = s->step[s->next].text;
            (void)SDL_PushEvent(&ev);
        } else if (s->step[s->next].text[0] != '\0') {
            game_flow_text(flow, s->step[s->next].text);
        }
        if (s->step[s->next].mouse[0] >= 0.0F) {
            eng_ui_add_mouse_pos(ui, s->step[s->next].mouse[0] * (float)platform->width,
                                 s->step[s->next].mouse[1] * (float)platform->height);
        }
        if (s->step[s->next].click) {
            eng_input_mouse_button(&platform->input, SDL_BUTTON_LEFT, true);
            eng_input_mouse_button(&platform->input, SDL_BUTTON_LEFT, false);
        }
        if (s->step[s->next].wheel != 0.0F) {
            eng_input_mouse_wheel(&platform->input, s->step[s->next].wheel);
        }
        if (s->step[s->next].escape) {
            for (int down = 1; down >= 0; down--) {
                SDL_Event ev;
                SDL_zero(ev);
                ev.type = down != 0 ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
                ev.key.scancode = SDL_SCANCODE_ESCAPE;
                ev.key.key = SDLK_ESCAPE;
                ev.key.down = down != 0;
                (void)SDL_PushEvent(&ev);
            }
        }
        s->pending |= s->step[s->next].keys;
        s->next++;
    }
}

/* --save-selftest (monde ouvert) : la partie est enregistrée où se trouve le joueur, le joueur est
 * envoyé au château de départ de la carte, puis la partie est reprise : il revient à sa position
 * MONDE, au sol, alors que l'origine a été recentrée entre-temps. */
static void world_save_selftest(GameFlow *flow, PlayMode *play, const WorldView *world)
{
    float feet[3] = {0.0F, 0.0F, 0.0F};
    double saved[3];
    double away[3];
    double back[3];
    (void)eng_character_position(play->character, feet);
    eng_origin_to_world(&world->origin, feet, saved);
    const uint32_t rebases = world->origin.rebases;
    const int32_t slot = flow->slot;
    /* le point de départ du jeu (repère local) doit rester au même endroit du MONDE */
    double start_before[3];
    double start_after[3];
    eng_origin_to_world(&world->origin, play->spawn, start_before);
    const bool wrote = game_flow_save_now(flow, play);
    const double castle[3] = {world->map != NULL ? (double)world->map->spawn[0] : 0.0, 0.0,
                              world->map != NULL ? (double)world->map->spawn[1] : 0.0};
    /* la carte de la partie (menu « Parties ») : le lieu de la sauvegarde, les boss de la carte */
    char near_castle[32] = "";
    if (flow->world.place_name != NULL) {
        flow->world.place_name(flow->world.user, castle, near_castle, sizeof near_castle);
    }
    const EngMenuSlot *card = &flow->menu.slots[slot];
    ENG_LOG_INFO("AUTOTEST SAUVEGARDE MONDE : carte de la partie : lieu « %s », %u boss ; au "
                 "château : « %s »",
                 card->place, (unsigned)card->boss_count, near_castle);
    const bool sent =
        flow->world.place != NULL && flow->world.place(flow->world.user, castle, 0.0F);
    /* mesuré ici, au milieu du trajet : un aller-retour ramène la même origine et masquerait un
     * recentrage mal répercuté */
    double start_mid[3];
    eng_origin_to_world(&world->origin, play->spawn, start_mid);
    (void)eng_character_position(play->character, feet);
    eng_origin_to_world(&world->origin, feet, away);
    const bool loaded = game_flow_load_slot(flow, play, slot);
    (void)eng_character_position(play->character, feet);
    eng_origin_to_world(&world->origin, feet, back);
    float ground = 0.0F;
    const bool on_ground =
        world_view_height(world, feet[0], feet[2], &ground) && fabsf(feet[1] - ground) < 0.5F;
    eng_origin_to_world(&world->origin, play->spawn, start_after);
    const double start_gap =
        fmax(hypot(start_after[0] - start_before[0], start_after[2] - start_before[2]),
             hypot(start_mid[0] - start_before[0], start_mid[2] - start_before[2]));
    const double moved = hypot(away[0] - saved[0], away[2] - saved[2]);
    const double gap = hypot(back[0] - saved[0], back[2] - saved[2]);
    if (wrote && sent && loaded && moved > 600.0 && gap < 0.5 && start_gap < 0.5 && on_ground &&
        world->origin.rebases > rebases) {
        ENG_LOG_INFO("AUTOTEST SAUVEGARDE MONDE : OK (parti à %.0f m, revenu à %.2f m de la "
                     "position enregistrée, au sol, %u recentrage(s) de l'origine)",
                     moved, gap, world->origin.rebases - rebases);
    } else {
        ENG_LOG_ERROR("AUTOTEST SAUVEGARDE MONDE : ÉCHEC (écrit %d, envoyé au château %d, relu %d, "
                      "parti à %.0f m, écart %.2f m, départ déplacé de %.2f m, au sol %d, "
                      "recentrages %u)",
                      (int)wrote, (int)sent, (int)loaded, moved, gap, start_gap, (int)on_ground,
                      world->origin.rebases - rebases);
    }
}

/* Point de départ du monde (coordonnées MONDE, m) : au milieu de la cellule (0, 0). */
/* carte du monde : plan lointain (m) et brouillard (1/m) : toute la carte visible depuis le château
 */
#define LAUNCHER_MAP_FAR 6000.0F
#define LAUNCHER_MAP_FOG 0.00035F
#define WORLD_SPAWN_X 128.0
#define WORLD_SPAWN_Z 128.0

/* Zones de combat de la grande carte : les ENG_MAPZONE_COUNT zones fixes, puis les meutes de petits
 * monstres de la carte dessinée (lignes « meute x z »). Retourne leur nombre (0 : carte invalide).
 */
#define WORLD_ZONES_MAX (ENG_MAPZONE_COUNT + ENG_WORLDMAP_MAX_PACKS)
static uint32_t world_zones(const WorldView *world, EngMapZone out[WORLD_ZONES_MAX])
{
    if (world->map == NULL) {
        return eng_mapzones_build(&world->gen.biomes, out) ? ENG_MAPZONE_COUNT : 0U;
    }
    if (!eng_worldmap_zones(world->map, out)) {
        return 0U;
    }
    const uint32_t fixed = ENG_MAPZONE_COUNT; /* les meutes suivent les zones fixes */
    return ENG_MAPZONE_COUNT + eng_worldmap_packs(world->map, out + fixed, ENG_WORLDMAP_MAX_PACKS);
}

/* --world : monde physique vide, streaming, chargement initial bloquant (les 9 cellules autour du
 * joueur), puis le personnage posé sur le relief. */
static bool start_world(WorldView *world, PlayMode *play, EngTweaks *tweaks, float sensitivity,
                        bool invert_y, EngRenderer *renderer, const DemoScene *scene, int32_t seed,
                        const double start[2], long at_zone, bool start_forced)
{
    const float spawn[3] = {(float)start[0], 0.0F, (float)start[1]};
    if (!play_mode_init_world(play, tweaks, sensitivity, invert_y, spawn, world_ground, world)) {
        return false;
    }
    const uint64_t t0 = eng_clock_counter();
    if (!world_view_init(world, seed, renderer, play->world,
                         scene->has_environment ? &scene->environment : NULL)) {
        play_mode_shutdown(play);
        return false;
    }
    double spawn_world[3] = {start[0], 0.0, start[1]};
    static EngMapZone zones[WORLD_ZONES_MAX];
    if (world->map != NULL && !start_forced) { /* carte : départ au château, ou devant la zone */
        spawn_world[0] = (double)world->map->spawn[0];
        spawn_world[2] = (double)world->map->spawn[1];
        if (at_zone >= 0 && (uint32_t)at_zone < world_zones(world, zones)) {
            /* sur le chemin depuis le château : 20 m avant la zone, 34 m avant un boss */
            const EngMapZone *z = &zones[at_zone];
            const double dx = spawn_world[0] - z->x;
            const double dz = spawn_world[2] - z->z;
            const double len = hypot(dx, dz) > 1.0 ? hypot(dx, dz) : 1.0;
            /* le dragon est colossal : 150 m, pour le voir en entier sans être sous sa gueule */
            const double back = z->boss_kind == ENG_BOSSKIND_DRAGON
                                    ? 150.0
                                    : (z->kind == ENG_MAPZONE_BOSS ? 34.0 : 20.0);
            spawn_world[0] = z->x + (back * dx / len);
            spawn_world[2] = z->z + (back * dz / len);
        }
    }
    if (!world_view_prime(world, spawn_world, 1, 60.0)) {
        world_view_shutdown(world);
        play_mode_shutdown(play);
        return false;
    }
    /* le chargement initial a pu recentrer l'origine flottante (départ loin de (0, 0), --at-zone) :
     * le joueur et son point de départ sont placés dans le repère LOCAL */
    float feet[3];
    eng_origin_to_local(&world->origin, spawn_world, feet);
    play->spawn[0] = feet[0];
    play->spawn[2] = feet[2];
    feet[1] = play_mode_ground(play, feet[0], feet[2]) + 0.05F;
    play_mode_teleport(play, feet, -1.5707963F);
    ENG_LOG_INFO("monde : chargement initial en %.0f ms (%u cellules envoyées), joueur à %.1f m "
                 "d'altitude",
                 (double)(eng_clock_counter() - t0) * 1000.0 /
                     (double)SDL_GetPerformanceFrequency(),
                 world->uploaded_cells, (double)feet[1]);
    return true;
}

/* M7 : le camp de combat garde sa place dans le MONDE à travers les recentrages de l'origine
 * flottante (sa position locale est décalée avec elle). Journalise l'ancrage, le premier
 * recentrage, et une ERREUR si le camp a bougé. */
static void check_camp_anchor(const WorldView *world, const PlayMode *play, bool shifted,
                              bool rebase)
{
    static bool known;
    static bool reported;
    static double anchor[3];
    static uint32_t shifts;
    float c[2];
    if (rebase) {
        known = false; /* la zone a changé : le camp est ailleurs */
    }
    if (play->combat == NULL || !play_combat_camp_center(play->combat, c)) {
        return;
    }
    const float local[3] = {c[0], 0.0F, c[1]};
    double w[3];
    eng_origin_to_world(&world->origin, local, w);
    if (!known) {
        memcpy(anchor, w, sizeof anchor);
        known = true;
        ENG_LOG_INFO("monde : camp de combat ancré en (%.1f, %.1f) du monde", w[0], w[2]);
        return;
    }
    if (!shifted) {
        return;
    }
    shifts++;
    const double gap = hypot(w[0] - anchor[0], w[2] - anchor[2]);
    if (gap > 0.05) {
        ENG_LOG_ERROR("monde : le camp a bougé de %.2f m au recentrage n° %u", gap, shifts);
    } else if (!reported) {
        reported = true;
        ENG_LOG_INFO("monde : origine recentrée, le camp reste en place (écart %.3f m)", gap);
    }
}

/* Carte du monde : l'eau (le joueur patauge sous son niveau) et la lave (elle brûle le joueur qui a
 * les pieds dedans, à moins de 0,6 m du sol). */
static void update_hazards(const WorldView *world, PlayMode *play, const float feet[3])
{
    if (world->map == NULL) {
        return;
    }
    const double level_world[3] = {0.0, (double)world->map->water, 0.0};
    float level[3];
    eng_origin_to_local(&world->origin, level_world, level);
    play_mode_set_water_level(play, world->water.ready ? level[1] : -INFINITY);
    if (play->combat == NULL) {
        return;
    }
    double pw[3];
    eng_origin_to_world(&world->origin, feet, pw);
    float ground = 0.0F;
    const bool on_ground =
        world_view_height(world, feet[0], feet[2], &ground) && feet[1] - ground < 0.6F;
    play_combat_set_lava(play->combat,
                         on_ground && eng_worldmap_lava(world->map, pw[0], pw[2]) > 0.5F);
}

/* Carte du monde : les troncs et les rochers à moins de 48 m du camp d'ennemis (le carré de 30 m
 * où ils se déplacent, coins compris) leur sont donnés comme obstacles à éviter. Refait toutes les
 * 30 images : les cellules voisines et leurs arbres se chargent peu à peu. */
static void update_tree_obstacles(const WorldView *world, PlayMode *play)
{
    static uint32_t wait;
    static uint32_t known = UINT32_MAX;
    static float circles[PLAY_AVOID_MAX][3];
    if (world->trees == NULL || play->combat == NULL || wait++ % 30U != 0U) {
        return;
    }
    float camp[2];
    const uint32_t n =
        play_combat_camp_center(play->combat, camp)
            ? world_trees_obstacles(world->trees, camp, 48.0F, circles, PLAY_AVOID_MAX)
            : 0U;
    play_combat_set_trees(play->combat, circles, n);
    if (n != known) {
        known = n;
        ENG_LOG_INFO("monde : %u tronc(s) et rocher(s) à éviter pour les ennemis du camp", n);
    }
}

/* Grande carte : le décor (château, piliers, campements) de la zone la plus proche à moins de
 * 300 m, et celui du camp de réveil à moins de 300 m du point de départ. Reposé quand la zone la
 * plus proche change ; réessayé tant que le relief n'est pas chargé sous toutes les boîtes. Avant
 * update_zone : les murs sont des obstacles pour les ennemis de la zone. */
static void update_decor(WorldView *world, PlayMode *play, const float feet[3])
{
    static EngMapZone zones[ENG_MAPZONE_COUNT];
    static EngDecorBox boxes[ENG_DECOR_MAX];
    static bool built;
    static bool complete;
    static bool camp_shown;
    static int32_t shown = -2; /* zone dont le décor est posé ; -1 : aucune ; -2 : rien encore */
    if (!built) {
        built = world->map != NULL ? eng_worldmap_zones(world->map, zones)
                                   : eng_mapzones_build(&world->gen.biomes, zones);
        if (!built) {
            return;
        }
    }
    double pw[3];
    eng_origin_to_world(&world->origin, feet, pw);
    int32_t near = eng_mapzones_nearest(zones, pw[0], pw[2], 300.0);
    for (int32_t k = 0; near < 0 && k < (int32_t)ENG_MAPZONE_COUNT; k++) {
        /* une zone qui s'active de plus loin (l'antre du dragon : 420 m) a son décor avant elle */
        if (hypot(zones[k].x - pw[0], zones[k].z - pw[2]) <= zones[k].reach + 60.0) {
            near = k;
        }
    }
    /* le château de départ est au départ de la CARTE (pas celui de --at-zone) */
    float home[3] = {play->spawn[0], 0.0F, play->spawn[2]};
    if (world->map != NULL) {
        const double spawn_world[3] = {world->map->spawn[0], 0.0, world->map->spawn[1]};
        eng_origin_to_local(&world->origin, spawn_world, home);
    }
    const bool camp = hypotf(feet[0] - home[0], feet[2] - home[2]) < 300.0F;
    if (complete && near == shown && camp == camp_shown) {
        return;
    }
    play_mode_clear_decor(play);
    uint32_t wanted = 0;
    uint32_t placed = 0;
    if (camp) {
        const uint32_t n = eng_camp_decor(boxes, ENG_DECOR_MAX);
        wanted += n;
        placed += play_mode_add_decor(play, boxes, n, home[0], home[2]);
    }
    if (near >= 0) {
        const double centre[3] = {zones[near].x, 0.0, zones[near].z};
        float local[3];
        eng_origin_to_local(&world->origin, centre, local);
        const uint32_t n = eng_zone_decor(&zones[near], boxes, ENG_DECOR_MAX);
        wanted += n;
        placed += play_mode_add_decor(play, boxes, n, local[0], local[2]);
    }
    /* pas d'herbe sous les murs et les piliers */
    static float footprints[PLAY_DECOR_MAX][5];
    for (uint32_t i = 0; i < play->decor_count; i++) {
        footprints[i][0] = play->decor_center[i][0];
        footprints[i][1] = play->decor_center[i][2];
        footprints[i][2] = play->decor_half[i][0];
        footprints[i][3] = play->decor_half[i][2];
        footprints[i][4] = play->decor_yaw[i];
    }
    world_grass_set_blockers(&world->grass, footprints, play->decor_count);
    shown = near;
    camp_shown = camp;
    complete = placed == wanted;
    if (complete && wanted > 0U) {
        ENG_LOG_INFO("monde : décor posé : %u boîte(s) (zone %d%s)", placed, (int)near,
                     camp ? ", château de départ" : "");
    }
}

/* Grande carte : la zone d'ennemis à portée du joueur (zone fixe : 110 m ; meute de petits
 * monstres : 85 m ; antre du dragon : 420 m ; la plus proche l'emporte) remplace la zone chargée,
 * sauf en plein combat (à moins que le joueur soit déjà 40 m au-delà de la portée de l'ancienne).
 * Au départ : le camp de réveil, sans ennemis. Un boss vaincu ne revient pas. Renvoie true si la
 * zone a changé. */
static bool update_zone(const WorldView *world, PlayMode *play, const float feet[3])
{
    static EngMapZone zones[WORLD_ZONES_MAX];
    static uint32_t count;
    static int32_t active = -2; /* -2 : rien chargé ; -1 : camp de réveil */
    if (play->combat == NULL) {
        return false;
    }
    if (count == 0U) {
        count = world_zones(world, zones);
        if (count == 0U) {
            return false;
        }
    }
    bool changed = false;
    if (active == -2) {
        changed = play_combat_load_zone(play->combat, NULL, -1, play->spawn[0], play->spawn[2]);
        active = -1;
        ENG_LOG_INFO("monde : camp de réveil (aucun ennemi) ; %u zones sur la carte et %u "
                     "meute(s) de petits monstres",
                     (unsigned)ENG_MAPZONE_COUNT, (unsigned)(count - ENG_MAPZONE_COUNT));
    }
    double pw[3];
    eng_origin_to_world(&world->origin, feet, pw);
    const int32_t near = eng_mapzones_active(zones, count, pw[0], pw[2]);
    if (near < 0 || near == active) {
        return changed;
    }
    const bool far_from_active =
        active < 0 ||
        hypot(zones[active].x - pw[0], zones[active].z - pw[2]) > zones[active].reach + 40.0;
    if (play_combat_busy(play->combat) && !far_from_active) {
        return changed;
    }
    const EngMapZone zone = zones[near];
    const double centre[3] = {zone.x, 0.0, zone.z};
    float local[3];
    eng_origin_to_local(&world->origin, centre, local);
    if (play_combat_load_zone(play->combat, &zone, near, local[0], local[2])) {
        active = near;
        changed = true;
        ENG_LOG_INFO(
            "monde : zone %d « %s » chargée (%s, %s, %u ennemi(s), %.0f PV, dégâts x "
            "%.1f)%s",
            (int)near, zone.name, eng_biome_name(zone.biome), eng_mapzone_kind_name(zone.kind),
            zone.enemy_count, (double)zone.hp, (double)zone.damage_scale,
            play_combat_zone_beaten(play->combat, near) ? " : boss déjà vaincu, vide" : "");
    } else {
        ENG_LOG_ERROR("monde : zone %d « %s » impossible à charger", (int)near, zone.name);
    }
    return changed;
}

/* Une image du monde : pilote automatique éventuel, streaming, recentrage de l'origine. Renvoie
 * les mètres parcourus par le pilote automatique. */
static double world_frame(WorldView *world, PlayMode *play, EngCamera *camera,
                          EngCamera *camera_prev, DemoScene *scene, double autopilot, double dt)
{
    double travelled = 0.0;
    float feet[3];
    if (!eng_character_position(play->character, feet)) {
        return 0.0;
    }
    if (autopilot > 0.0) { /* traversée mesurée : le personnage glisse vers +X sur le relief */
        const float step = (float)(autopilot * (dt < 0.1 ? dt : 0.1));
        float ground = 0.0F;
        if (world_view_height(world, feet[0] + step, feet[2], &ground)) {
            const float d[3] = {step, ground + 0.05F - feet[1], 0.0F};
            play_mode_shift(play, d);
            for (int a = 0; a < 3; a++) {
                feet[a] += d[a];
                camera->position[a] += d[a];
                camera_prev->position[a] += d[a];
            }
            travelled = (double)step;
        }
    }
    double player_world[3];
    eng_origin_to_world(&world->origin, feet, player_world);
    float shift[3];
    const bool shifted = world_view_update(world, player_world, shift);
    if (shifted) {
        play_mode_origin_shift(play, shift); /* M7 : personnage, départ et combat ensemble */
        for (int a = 0; a < 3; a++) {
            feet[a] += shift[a];
            camera->position[a] += shift[a];
            camera_prev->position[a] += shift[a];
        }
    }
    update_hazards(world, play, feet);
    update_decor(world, play, feet);
    const bool zone_changed = update_zone(world, play, feet);
    update_tree_obstacles(world, play);
    check_camp_anchor(world, play, shifted, zone_changed);
    for (int a = 0; a < 3; a++) {
        scene->shadow_focus[a] = feet[a];
    }
    return travelled;
}

/* ── Étapes longues du chargement : exécutées dans un fil à part quand l'écran de chargement est
 * affiché (splash_run), directement sinon. Elles ne font que créer et remplir des ressources. ── */

typedef struct RendererJob {
    EngRenderer *renderer;
    EngGpu *gpu;
    EngMemory *memory;
    const EngRendererDesc *desc;
    DemoScene *scene;
    const DemoOptions *options;
} RendererJob;

static bool renderer_job(void *user)
{
    const RendererJob *j = user;
    return eng_renderer_init(j->renderer, j->gpu, &j->memory->permanent, &j->memory->frame,
                             j->desc) &&
           demo_scene_init(j->scene, j->renderer, &j->memory->permanent, &j->memory->level,
                           &j->memory->frame, j->options);
}

typedef struct WorldJob {
    WorldView *world;
    PlayMode *play;
    EngTweaks *tweaks;
    float sensitivity;
    bool invert_y;
    EngRenderer *renderer;
    const DemoScene *scene;
    int32_t seed;
    const double *start;
    long at_zone;
    bool start_forced;
} WorldJob;

static bool world_job(void *user)
{
    const WorldJob *j = user;
    return start_world(j->world, j->play, j->tweaks, j->sensitivity, j->invert_y, j->renderer,
                       j->scene, j->seed, j->start, j->at_zone, j->start_forced);
}

typedef struct CombatJob {
    PlayMode *play;
    bool audio;
} CombatJob;

static bool combat_job(void *user)
{
    const CombatJob *j = user;
    return play_mode_enable_combat(j->play, j->audio);
}

typedef struct AvatarJob {
    AvatarLibrary *avatars;
    EngRenderer *renderer;
    const EngEnvironment *env;
} AvatarJob;

static bool avatar_job(void *user)
{
    const AvatarJob *j = user;
    return avatar_library_load_cpu(j->avatars) &&
           avatar_library_load_gpu(j->avatars, j->renderer, j->env);
}

/* --splash-shots : des images de l'écran de chargement à des instants choisis, écrites en PPM et
 * mesurées : le fondu d'entrée, le fil qui se déroule, le titre qui apparaît, la corde qui se
 * remplit, la sortie. Journalise AUTOTEST CHARGEMENT. */
static bool splash_shots(Splash *splash, const char *dir, int width, int height)
{
    static const float moments[8][3] = {
        /* temps (s), progression, sortie */
        {0.25F, 0.03F, 0.0F}, {1.4F, 0.12F, 0.0F}, {1.7F, 0.25F, 0.0F},  {2.9F, 0.45F, 0.0F},
        {4.5F, 0.7F, 0.0F},   {8.0F, 0.92F, 0.0F}, {10.0F, 1.0F, 0.35F}, {10.3F, 1.0F, 0.9F}};
    SplashStats m[8];
    uint32_t written = 0;
    for (uint32_t i = 0; i < 8U; i++) {
        char path[1024];
        (void)snprintf(path, sizeof path, "%s/splash-%02u.ppm", dir, (unsigned)i);
        memset(&m[i], 0, sizeof m[i]);
        written += splash_capture(splash, (double)moments[i][0], moments[i][1], moments[i][2],
                                  (uint32_t)width, (uint32_t)height, path, &m[i])
                       ? 1U
                       : 0U;
        ENG_LOG_INFO("  image %u (t = %.2f s) : luminosité %.3f, titre %.4f (gauche %.4f, droite "
                     "%.4f), fil %.4f, corde %.4f",
                     (unsigned)i, (double)moments[i][0], (double)m[i].luma, (double)m[i].title,
                     (double)m[i].title_left, (double)m[i].title_right, (double)m[i].thread,
                     (double)m[i].bar);
    }
    ENG_LOG_INFO("écran de chargement : %u image(s) écrite(s) dans %s", written, dir);
    const bool fade_in = m[0].luma < 0.75F * m[4].luma; /* il sort du noir */
    const bool no_title_yet = m[1].title < 0.002F;      /* pas de titre avant 1,1 s */
    const bool title = m[4].title > 0.03F;              /* puis les lettres, en clair */
    /* à 1,7 s le titre n'a que sa moitié gauche ; à 4,5 s il est entier */
    const bool half_title = m[2].title_left > 0.03F && m[2].title_right < 0.2F * m[2].title_left &&
                            m[4].title_right > 0.5F * m[4].title_left;
    const bool no_thread_yet = m[0].thread < 0.0005F; /* le fil n'est pas encore parti */
    const bool thread = m[4].thread > 0.004F;         /* puis il traverse le ciel */
    const bool bar = m[5].bar > 3.0F * m[1].bar && m[1].bar > 0.0F; /* la corde se remplit */
    const bool outro = m[7].luma < 0.25F * m[5].luma;               /* et tout s'éteint */
    const bool ok = written == 8U && fade_in && no_title_yet && title && half_title &&
                    no_thread_yet && thread && bar && outro;
    if (ok) {
        ENG_LOG_INFO("AUTOTEST CHARGEMENT : OK (8 images : fondu, fil, titre, corde, sortie)");
    } else {
        ENG_LOG_ERROR("AUTOTEST CHARGEMENT : ÉCHEC (images %u/8 ; fondu %d, sans titre %d, titre "
                      "%d, mi-titre %d, sans fil %d, fil %d, corde %d, sortie %d)",
                      written, (int)fade_in, (int)no_title_yet, (int)title, (int)half_title,
                      (int)no_thread_yet, (int)thread, (int)bar, (int)outro);
    }
    return ok;
}

static void launcher_debug_hook(void *user, EngDebugLines *lines, mat4 model, bool has_model)
{
    const PlayHook *hook = user;
    if (hook->play != NULL) {
        play_mode_debug_draw(hook->play, lines, hook->alpha);
    }
    if (hook->skeleton != NULL && has_model) {
        (void)skeleton_view_draw(hook->skeleton, lines, model);
    }
}

int main(int argc, char **argv)
{
    eng_mem_track_install(); /* avant toute allocation de SDL */

    long max_frames = -1; /* -1 : jusqu'à la fermeture de la fenêtre */
    bool assert_test = false;
    bool force_software = false;
    bool start_with_ui = false;
    bool stress_resize = false;
    bool editor_enabled = false;
    bool editor_selftest = false;
    bool play_enabled = false;
    bool avatar_gallery_mode = false;
    bool play_selftest = false;
    bool combat_enabled = false;
    bool combat_audio = true;
    bool combat_selftest = false;
    bool boss_selftest = false; /* --boss-selftest : cavalier et dragon de la grande carte */
    bool combat_selftest_rig = false;
    const char *combat_terrain_selftest_path = NULL;
    bool combat_autoplay = false;
    double world_bench_km = 0.0;
    const char *terrain_selftest_path = NULL;
    bool game_selftest = false;
    bool start_new_game = false;
    bool crash_test = false;
    const char *world_bench_report = NULL;
    float exposure_arg = NAN; /* --exposure EV (B2) ; NAN : défaut */
    float ambient_arg = NAN;  /* --ambient X : intensité de l'environnement ; NAN : défaut */
    bool world_mode = false;
    bool weapon_greatsword = false; /* --weapon greatsword (M8-08) */
    bool at_boss = false;           /* --at-boss (M6-09) */
    long post_bench_frames = 0;     /* --post-bench N (finition M8-06) */
    long gear_tier = 0; /* --gear N : commence avec le palier d'équipement N (0 à 4) */
    long at_zone = -1;  /* --at-zone N : départ devant la zone N de la grande carte */
    double world_start[2] = {WORLD_SPAWN_X, WORLD_SPAWN_Z};
    int32_t world_seed = 1;
    bool world_start_forced = false;     /* --at-pos X Z : départ à ce point de la carte */
    bool fog_from_command_line = false;  /* --fog-density : la carte du monde ne le remplace pas */
    bool splash_forced = false;          /* --splash : écran de chargement même avec --frames */
    bool splash_disabled = false;        /* --no-splash */
    const char *splash_shots_dir = NULL; /* --splash-shots DOSSIER */
    const char *menu_script_spec = NULL; /* --menu-script : touches jouées dans le menu */
    const char *shots_dir = NULL;        /* --shots DOSSIER LISTE : images écrites en PPM */
    const char *shots_list = NULL;
    bool windowed = false;       /* --windowed : pas de plein écran par défaut */
    bool creative_game = false;  /* --creative : avec --new-game, partie en mode créatif */
    double creative_speed = 0.0; /* --creative-speed V : vitesse de vol au départ (m/s) */
    bool look_at_set = false;    /* --look-at X Z : la caméra regarde ce point de la carte */
    double look_at[2] = {0.0, 0.0};
    double look_pitch = 0.0; /* --look-pitch D : degrés (positif : vers le haut) */
    long warp_ticks = 0;     /* --warp N : N ticks de jeu joués d'un coup à la troisième image */
    bool look_at_boss = false;   /* --look-at-boss : la caméra suit le boss spécial de la zone */
    const char *save_dir = NULL; /* --save-dir DOSSIER : parties et options ailleurs */
    bool save_selftest = false;  /* --save-selftest : sauvegarde dans le monde ouvert */
    double world_autopilot = 0.0;
    const char *screenshot_path = NULL;
    long screenshot_frame = 0;
    const char *skeleton_path = NULL;
    const char *anim_path = NULL;
    const char *sockets_path = NULL;
    const char *scene_arg = NULL;
    uint32_t frames_in_flight = 0; /* 0 : valeur par défaut du moteur */
    DemoOptions demo_options = {.mipmaps = true,
                                .placement_seed = 1,
                                .anisotropy = 16.0F,
                                .sun_elevation = 45.0F,
                                .sun_azimuth = 30.0F,
                                .fog_density = 0.0008F}; /* B2 : 0,003 délavait tout */
    bool use_day_night = false; /* --day-night / --day-length (M8-07) ; la partie l'active aussi */
    double day_night_hour = -1.0;
    double day_length = 0.0;
    const char *game_path = NULL;
    const char *config_path = NULL;
    EngLogLevel log_level = eng_log_env_level(ENG_LEVEL_INFO);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            char *end = NULL;
            max_frames = strtol(argv[++i], &end, 10);
            if (*end != '\0' || max_frames < 0) {
                usage(argv[0]);
                return 2;
            }
        } else if (strcmp(argv[i], "--log-level") == 0 && i + 1 < argc) {
            if (!eng_log_parse_level(argv[++i], &log_level)) {
                usage(argv[0]);
                return 2;
            }
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "--game") == 0 && i + 1 < argc) {
            game_path = argv[++i];
        } else if (strcmp(argv[i], "--editor") == 0) {
            editor_enabled = true;
        } else if (strcmp(argv[i], "--editor-selftest") == 0) {
            editor_enabled = true;
            editor_selftest = true;
        } else if (strcmp(argv[i], "--play") == 0) {
            play_enabled = true;
        } else if (strcmp(argv[i], "--play-selftest") == 0) {
            play_selftest = true;
        } else if (strcmp(argv[i], "--combat") == 0) {
            play_enabled = true;
            combat_enabled = true;
        } else if (strcmp(argv[i], "--crash-test") == 0) {
            crash_test = true;
        } else if (strcmp(argv[i], "--new-game") == 0) {
            start_new_game = true;
        } else if (strcmp(argv[i], "--splash") == 0) {
            splash_forced = true;
        } else if (strcmp(argv[i], "--no-splash") == 0) {
            splash_disabled = true;
        } else if (strcmp(argv[i], "--splash-shots") == 0 && i + 1 < argc) {
            splash_shots_dir = argv[++i];
        } else if (strcmp(argv[i], "--menu-script") == 0 && i + 1 < argc) {
            menu_script_spec = argv[++i];
        } else if (strcmp(argv[i], "--shots") == 0 && i + 2 < argc) {
            shots_dir = argv[++i];
            shots_list = argv[++i];
        } else if (strcmp(argv[i], "--windowed") == 0) {
            windowed = true;
        } else if (strcmp(argv[i], "--creative") == 0) {
            creative_game = true;
        } else if (strcmp(argv[i], "--creative-speed") == 0 && i + 1 < argc) {
            creative_speed = strtod(argv[++i], NULL);
        } else if (strcmp(argv[i], "--look-at") == 0 && i + 2 < argc) {
            look_at[0] = strtod(argv[++i], NULL);
            look_at[1] = strtod(argv[++i], NULL);
            look_at_set = isfinite(look_at[0]) && isfinite(look_at[1]);
        } else if (strcmp(argv[i], "--look-at-boss") == 0) {
            look_at_boss = true;
        } else if (strcmp(argv[i], "--look-pitch") == 0 && i + 1 < argc) {
            look_pitch = strtod(argv[++i], NULL);
        } else if (strcmp(argv[i], "--warp") == 0 && i + 1 < argc) {
            warp_ticks = strtol(argv[++i], NULL, 10);
            if (warp_ticks < 0 || warp_ticks > 200000) {
                fprintf(stderr, "--warp : nombre de ticks de 0 à 200000 attendu\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--save-dir") == 0 && i + 1 < argc) {
            save_dir = argv[++i];
        } else if (strcmp(argv[i], "--save-selftest") == 0) {
            save_selftest = true;
        } else if (strcmp(argv[i], "--game-selftest") == 0) {
            game_selftest = true;
        } else if (strcmp(argv[i], "--play-terrain-selftest") == 0 && i + 1 < argc) {
            terrain_selftest_path = argv[++i];
        } else if (strcmp(argv[i], "--world-bench") == 0) {
            world_bench_km = 10.0;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                world_bench_km = strtod(argv[++i], NULL);
            }
        } else if (strcmp(argv[i], "--at-boss") == 0) {
            at_boss = true;
        } else if (strcmp(argv[i], "--gear") == 0 && i + 1 < argc) {
            char *end = NULL;
            gear_tier = strtol(argv[++i], &end, 10);
            if (*end != '\0' || gear_tier < 0 || gear_tier > 4) {
                fprintf(stderr, "--gear : palier d'équipement de 0 à 4 attendu\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--at-pos") == 0 && i + 2 < argc) {
            char *end_x = NULL;
            char *end_z = NULL;
            world_start[0] = strtod(argv[++i], &end_x);
            world_start[1] = strtod(argv[++i], &end_z);
            if (*end_x != '\0' || *end_z != '\0' || !isfinite(world_start[0]) ||
                !isfinite(world_start[1])) {
                fprintf(stderr, "--at-pos : deux nombres attendus (x z, en mètres de la carte)\n");
                return 2;
            }
            world_start_forced = true;
        } else if (strcmp(argv[i], "--at-zone") == 0 && i + 1 < argc) {
            char *end = NULL;
            at_zone = strtol(argv[++i], &end, 10);
            EngMapZone zones[ENG_MAPZONE_COUNT];
            if (*end != '\0' || at_zone < 0 || at_zone >= (long)ENG_MAPZONE_COUNT ||
                !eng_mapzones_build(NULL, zones)) {
                fprintf(stderr, "--at-zone : numéro de zone de 0 à %u attendu\n",
                        (unsigned)ENG_MAPZONE_COUNT - 1U);
                return 2;
            }
            /* avant la zone, sur le chemin : 20 m, ou 34 m devant un boss (hors de son décor) */
            world_start[0] =
                zones[at_zone].x - (zones[at_zone].boss_kind == ENG_BOSSKIND_DRAGON
                                        ? 150.0
                                        : (zones[at_zone].kind == ENG_MAPZONE_BOSS ? 34.0 : 20.0));
            world_start[1] = zones[at_zone].z;
        } else if (strcmp(argv[i], "--at-pack") == 0 && i + 1 < argc) {
            char *end = NULL; /* devant la meute N de la carte dessinée (0 : la première ligne) */
            const long pack = strtol(argv[++i], &end, 10);
            if (*end != '\0' || pack < 0 || pack >= (long)ENG_WORLDMAP_MAX_PACKS) {
                fprintf(stderr, "--at-pack : numéro de meute de 0 à %u attendu\n",
                        (unsigned)ENG_WORLDMAP_MAX_PACKS - 1U);
                return 2;
            }
            at_zone = (long)ENG_MAPZONE_COUNT + pack;
        } else if (strcmp(argv[i], "--post-bench") == 0 && i + 1 < argc) {
            char *end = NULL;
            post_bench_frames = strtol(argv[++i], &end, 10);
            if (*end != '\0' || post_bench_frames < 1 || post_bench_frames > 100000) {
                fprintf(stderr, "--post-bench : nombre d'images de 1 à 100000 attendu\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--weapon") == 0 && i + 1 < argc) {
            weapon_greatsword = strcmp(argv[++i], "greatsword") == 0;
        } else if (strcmp(argv[i], "--world") == 0) {
            world_mode = true;
            play_enabled = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                world_seed = (int32_t)strtol(argv[++i], NULL, 10);
            }
        } else if (strcmp(argv[i], "--world-autopilot") == 0 && i + 1 < argc) {
            world_autopilot = strtod(argv[++i], NULL);
        } else if (strcmp(argv[i], "--world-bench-report") == 0 && i + 1 < argc) {
            world_bench_report = argv[++i];
        } else if (strcmp(argv[i], "--avatar-gallery") == 0) {
            avatar_gallery_mode = true;
        } else if (strcmp(argv[i], "--combat-autoplay") == 0) {
            play_enabled = true;
            combat_enabled = true;
            combat_autoplay = true;
        } else if (strcmp(argv[i], "--screenshot") == 0 && i + 2 < argc) {
            screenshot_path = argv[++i];
            screenshot_frame = strtol(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--no-audio") == 0) {
            combat_audio = false;
        } else if (strcmp(argv[i], "--boss-selftest") == 0) {
            boss_selftest = true;
        } else if (strcmp(argv[i], "--combat-selftest") == 0) {
            combat_selftest = true;
        } else if (strcmp(argv[i], "--combat-terrain-selftest") == 0 && i + 1 < argc) {
            combat_terrain_selftest_path = argv[++i];
        } else if (strcmp(argv[i], "--combat-rig-selftest") == 0) {
            combat_selftest = true;
            combat_selftest_rig = true;
        } else if (strcmp(argv[i], "--skeleton") == 0 && i + 1 < argc) {
            skeleton_path = argv[++i];
        } else if (strcmp(argv[i], "--anim") == 0 && i + 1 < argc) {
            anim_path = argv[++i];
        } else if (strcmp(argv[i], "--sockets") == 0 && i + 1 < argc) {
            sockets_path = argv[++i];
        } else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            scene_arg = argv[++i];
        } else if ((strcmp(argv[i], "--sun-elevation") == 0 ||
                    strcmp(argv[i], "--sun-azimuth") == 0) &&
                   i + 1 < argc) {
            char *end = NULL;
            const bool is_elevation = strcmp(argv[i], "--sun-elevation") == 0;
            const float value = strtof(argv[++i], &end);
            if (*end != '\0' || !isfinite(value) || value < -180.0F || value > 180.0F ||
                (is_elevation && (value < -90.0F || value > 90.0F))) {
                usage(argv[0]);
                return 2;
            }
            if (is_elevation) {
                demo_options.sun_elevation = value;
            } else {
                demo_options.sun_azimuth = value;
            }
        } else if ((strcmp(argv[i], "--day-night") == 0 || strcmp(argv[i], "--day-length") == 0) &&
                   i + 1 < argc) {
            const bool is_hour = strcmp(argv[i], "--day-night") == 0;
            char *end = NULL;
            const double value = strtod(argv[++i], &end);
            if (*end != '\0' || !isfinite(value) || value < 0.0 ||
                (is_hour ? value >= 24.0 : (value < 1.0 || value > 1e6))) {
                usage(argv[0]);
                return 2;
            }
            use_day_night = true;
            if (is_hour) {
                day_night_hour = value;
            } else {
                day_length = value;
            }
        } else if ((strcmp(argv[i], "--exposure") == 0 || strcmp(argv[i], "--ambient") == 0) &&
                   i + 1 < argc) {
            const bool is_exposure = strcmp(argv[i], "--exposure") == 0;
            char *end = NULL;
            const float value = strtof(argv[++i], &end);
            if (*end != '\0' || !isfinite(value) ||
                (is_exposure ? (value < -8.0F || value > 8.0F) : (value < 0.0F || value > 4.0F))) {
                usage(argv[0]);
                return 2;
            }
            *(is_exposure ? &exposure_arg : &ambient_arg) = value;
        } else if (strcmp(argv[i], "--fog-density") == 0 && i + 1 < argc) {
            char *end = NULL;
            const float value = strtof(argv[++i], &end);
            if (*end != '\0' || !isfinite(value) || value < 0.0F || value > 1.0F) {
                usage(argv[0]);
                return 2;
            }
            demo_options.fog_density = value;
            fog_from_command_line = true;
        } else if (strcmp(argv[i], "--rocks") == 0 && i + 1 < argc) {
            char *end = NULL;
            const unsigned long value = strtoul(argv[++i], &end, 10);
            if (*end != '\0' || end == argv[i] || value > 1000000UL) {
                usage(argv[0]);
                return 2;
            }
            demo_options.rock_count = (uint32_t)value;
        } else if (strcmp(argv[i], "--grass") == 0) {
            demo_options.grass = true;
        } else if (strcmp(argv[i], "--water") == 0) {
            demo_options.water = true;
        } else if (strcmp(argv[i], "--water-level") == 0 && i + 1 < argc) {
            char *end = NULL;
            const float value = strtof(argv[++i], &end);
            if (*end != '\0' || end == argv[i] || !isfinite(value) || fabsf(value) > 100000.0F) {
                usage(argv[0]);
                return 2;
            }
            demo_options.water = true;
            demo_options.water_level_set = true;
            demo_options.water_level = value;
        } else if (strcmp(argv[i], "--placement") == 0) {
            demo_options.placement = true;
        } else if (strcmp(argv[i], "--placement-seed") == 0 && i + 1 < argc) {
            char *end = NULL;
            const unsigned long value = strtoul(argv[++i], &end, 10);
            if (*end != '\0' || end == argv[i] || value > 0xFFFFFFF0UL) {
                usage(argv[0]);
                return 2;
            }
            demo_options.placement_seed = (uint32_t)value;
        } else if (strcmp(argv[i], "--terrain") == 0 && i + 1 < argc) {
            demo_options.terrain_path = argv[++i];
        } else if (strcmp(argv[i], "--watch-assets") == 0) {
            demo_options.watch_assets = true;
        } else if (strcmp(argv[i], "--no-mipmaps") == 0) {
            demo_options.mipmaps = false;
        } else if (strcmp(argv[i], "--anisotropy") == 0 && i + 1 < argc) {
            char *end = NULL;
            demo_options.anisotropy = strtof(argv[++i], &end);
            if (*end != '\0' || !(demo_options.anisotropy >= 0.0F)) {
                usage(argv[0]);
                return 2;
            }
        } else if (strcmp(argv[i], "--env") == 0 && i + 1 < argc) {
            demo_options.environment_path = argv[++i];
        } else if (strcmp(argv[i], "--env-samples") == 0 && i + 1 < argc) {
            char *end = NULL;
            const long samples = strtol(argv[++i], &end, 10);
            if (*end != '\0' || samples < 1 || samples > 65536) {
                usage(argv[0]);
                return 2;
            }
            demo_options.environment_samples = (uint32_t)samples;
        } else if (strcmp(argv[i], "--frames-in-flight") == 0 && i + 1 < argc) {
            char *end = NULL;
            const long count = strtol(argv[++i], &end, 10);
            if (*end != '\0' || count < 1 || count > (long)ENG_GPU_MAX_FRAMES_IN_FLIGHT) {
                usage(argv[0]);
                return 2;
            }
            frames_in_flight = (uint32_t)count;
        } else if (strcmp(argv[i], "--stress-resize") == 0) {
            stress_resize = true;
        } else if (strcmp(argv[i], "--ui") == 0) {
            start_with_ui = true;
        } else if (strcmp(argv[i], "--software") == 0) {
            force_software = true;
        } else if (strcmp(argv[i], "--assert-test") == 0) {
            assert_test = true;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    /* Si le fichier ne peut pas être créé, l'erreur est déjà journalisée : on continue
     * avec la console seule. */
    char log_dir[512];
    if (!eng_path_resolve(log_dir, sizeof log_dir, "logs")) {
        snprintf(log_dir, sizeof log_dir, "logs"); /* repli : relatif au dossier courant */
    }
    (void)eng_log_init(log_dir, log_level);

    ENG_LOG_INFO("engine %s", eng_version());
    /* M9-03 : rapport de crash à côté du journal */
#if defined(ENG_HAVE_CRASH)
    if (eng_crash_install(log_dir, eng_version())) {
        eng_crash_note("journal", log_dir);
    }
#else
    ENG_LOG_WARN("rapport de crash désactivé (engine/src/crash.c absent de cette build)");
#endif
    if (crash_test) { /* (développement) plantage volontaire : le rapport doit être écrit */
#if defined(__SANITIZE_ADDRESS__)
        /* sous ASan/UBSan, une écriture nulle est interceptée avant le signal : on l'envoie
         * directement (même gestionnaire, même rapport) */
        ENG_LOG_WARN("test de crash : signal SIGSEGV (build instrumenté)");
        (void)raise(SIGSEGV);
#else
        ENG_LOG_WARN("test de crash : écriture à l'adresse nulle");
        volatile int *volatile null_ptr = NULL;
        *null_ptr = 42; /* NOLINT(clang-analyzer-core.NullDereference) : plantage voulu */
#endif
    }
    ENG_LOG_INFO("journal dans %s", log_dir);
    eng_print_deps();

    if (game_selftest) { /* sans fenêtre : nouvelle partie, sauvegarde, relance, reprise */
        const bool game_ok = game_flow_selftest();
        eng_log_shutdown();
        return game_ok ? 0 : 1;
    }
    if (terrain_selftest_path != NULL) { /* sans fenêtre : personnage sur un terrain .hmap */
        const bool terrain_ok = play_mode_terrain_selftest(terrain_selftest_path);
        eng_log_shutdown();
        return terrain_ok ? 0 : 1;
    }
    if (world_bench_km > 0.0) { /* sans fenêtre : traversée du monde ouvert mesurée */
        const bool bench_ok = world_bench_run(world_bench_km, 30.0, world_bench_report);
        eng_log_shutdown();
        return bench_ok ? 0 : 1;
    }
    if (combat_terrain_selftest_path != NULL) { /* M6-03 : le camp sur un relief */
        const bool camp_ok = play_combat_terrain_selftest(combat_terrain_selftest_path);
        eng_log_shutdown();
        return camp_ok ? 0 : 1;
    }
    if (boss_selftest) { /* grande carte : cavalier et dragon, sans fenêtre */
        const bool boss_ok = play_combat_bosses_selftest();
        eng_log_shutdown();
        return boss_ok ? 0 : 1;
    }
    if (combat_selftest) { /* sans fenêtre ni GPU : personnage, combat et IA scriptés */
        const bool combat_ok = play_combat_selftest(combat_selftest_rig);
        eng_log_shutdown();
        return combat_ok ? 0 : 1;
    }
    if (play_selftest) { /* sans fenêtre ni GPU : physique et entrées scriptées seulement */
        const bool play_ok = play_mode_selftest();
        eng_log_shutdown();
        return play_ok ? 0 : 1;
    }

    if (assert_test) {
        int answer = 41;
        ENG_LOG_INFO("test d'assertion (assertions %s)",
                     ENG_ASSERTS_ENABLED ? "actives" : "désactivées");
        ENG_ASSERT(argc > 0); /* vraie : ne doit rien afficher */
        ENG_ASSERT_MSG(answer == 42, "la réponse vaut %d au lieu de 42", answer);
        ENG_LOG_INFO("l'exécution a repris après la première assertion");
        ENG_ASSERT(answer == 42);
        ENG_LOG_INFO("l'exécution a repris après la seconde assertion");
    }

    /* Configuration : valeurs par défaut si le fichier est absent ; les lignes invalides sont
     * signalées puis ignorées. */
    char default_config_path[512];
    if (config_path == NULL) {
        if (eng_path_resolve(default_config_path, sizeof default_config_path, "config.ini")) {
            config_path = default_config_path;
        } else {
            config_path = "config.ini";
        }
    }
    EngConfig config;
    EngArena config_scratch;
    if (!eng_arena_init(&config_scratch, "config", ENG_CONFIG_MAX_FILE_SIZE + 4096)) {
        eng_log_shutdown();
        return 1;
    }
    if (eng_config_load(&config, &config_scratch, config_path)) {
        ENG_LOG_INFO("configuration lue : %s (%d ligne(s) ignorée(s))", config_path,
                     config.issue_count);
    } else {
        ENG_LOG_INFO("pas de configuration (%s) : valeurs par défaut", config_path);
    }
    eng_arena_shutdown(&config_scratch);

    /* Écran de chargement animé (splash.h) : pour une partie lancée à la main. Les lancements
     * automatiques (--frames, autotests, mesures) chargent directement, comme avant ; --splash le
     * force, --no-splash le retire. */
    const bool interactive =
        max_frames < 0 && !editor_selftest && !play_selftest && !combat_selftest &&
        !boss_selftest && !game_selftest && terrain_selftest_path == NULL &&
        combat_terrain_selftest_path == NULL && world_bench_km <= 0.0 && post_bench_frames == 0 &&
        !crash_test && !stress_resize && !assert_test;
    const bool splash_wanted = !splash_disabled && !force_software &&
                               (interactive || splash_forced || splash_shots_dir != NULL);
    EngWindowDesc desc = {
        .title = "Open World",
        .width = config.window_width,
        .height = config.window_height,
        .resizable = true,
        /* le plein écran des options du jeu dès l'ouverture (pas au milieu du chargement) ; le
         * jeu lancé à la main s'ouvre en plein écran tant que les options ne disent pas le
         * contraire (--windowed : en fenêtre) */
        .fullscreen = config.fullscreen || (interactive && !windowed && play_enabled &&
                                            combat_enabled && game_flow_saved_fullscreen(true)),
        .vsync = config.vsync,
        .no_software_surface = !force_software, /* la fenêtre ira au GPU, sauf repli */
        .hidden_until_shown = splash_wanted && splash_shots_dir == NULL,
    };
    EngPlatform platform;
    if (!eng_platform_init(&platform, &desc)) {
        eng_log_shutdown();
        return 1;
    }
    /* le jeu a un menu : Échap l'ouvre ou y revient en arrière, et ne ferme plus la fenêtre */
    platform.escape_quits = !(play_enabled && combat_enabled);
    static MenuScript menu_script;
    if (menu_script_spec != NULL && !menu_script_parse(&menu_script, menu_script_spec)) {
        ENG_LOG_ERROR("--menu-script : « %s » illisible (image:touche,...)", menu_script_spec);
        eng_platform_shutdown(&platform);
        eng_log_shutdown();
        return 1;
    }

    /* GPU : si aucun pilote n'est utilisable (pas de Vulkan, pilote vidéo factice des tests),
     * on se rabat sur l'affichage logiciel provisoire. */
    EngGpu gpu = {0};
    bool use_gpu = false;
    if (!force_software) {
        const EngGpuDesc gpu_desc = {
#ifndef NDEBUG
            .debug = true,
#endif
            .present_mode = config.vsync ? ENG_PRESENT_VSYNC : ENG_PRESENT_IMMEDIATE,
            .frames_in_flight = frames_in_flight,
        };
        use_gpu = eng_gpu_init(&gpu, platform.window, &gpu_desc);
        if (use_gpu && screenshot_path != NULL) {
            gpu.capture_path = screenshot_path;
            gpu.capture_frame = screenshot_frame > 0 ? (uint64_t)screenshot_frame : 1U;
        }
        if (use_gpu) {
            platform.vsync = gpu.present_mode == ENG_PRESENT_VSYNC;
        } else {
            ENG_LOG_WARN("GPU indisponible : affichage logiciel (le rendu GPU arrive en M1)");
            eng_platform_use_software_surface(&platform, config.vsync);
        }
    }

    /* Écran de chargement animé (splash.h) : dès que le GPU est prêt. */
    static Splash splash;
    if (use_gpu && splash_wanted && splash_init(&splash, &gpu, &platform)) {
        if (splash_shots_dir != NULL) {
            const bool shots_ok =
                splash_shots(&splash, splash_shots_dir, platform.width, platform.height);
            splash_shutdown(&splash);
            eng_gpu_shutdown(&gpu);
            eng_platform_shutdown(&platform);
            eng_log_shutdown();
            return shots_ok ? 0 : 1;
        }
    }
    /* la fenêtre créée cachée apparaît maintenant, et sa première image suit aussitôt */
    eng_platform_show_window(&platform);
    splash_frame(&splash, &platform);
    splash_frame(&splash, &platform);

    EngMemory memory;
    const EngMemoryConfig memory_config = {
        .permanent_bytes = (size_t)64 * 1024 * 1024,
        /* un terrain de 1025 x 1025 échantillons demande 75 Mio de maillage CPU (engine/terrain.h)
         */
        .level_bytes = (size_t)(demo_options.terrain_path != NULL ? 256 : 64) * 1024 * 1024,
        .frame_bytes = (size_t)8 * 1024 * 1024,
    };
    if (!eng_memory_init(&memory, &memory_config)) {
        eng_gpu_shutdown(&gpu);
        eng_platform_shutdown(&platform);
        eng_log_shutdown();
        return 1;
    }

    eng_config_apply_input(&config, &platform.input);

    /* Caméra libre de debug (M1-09). L'axe LOOK est en « unités de regard » : on convertit la
     * sensibilité de la souris (radians par pixel, config.ini) vers cette unité. Sans capture de
     * la souris (F2), la souris n'agit plus sur le regard : seul le stick droit reste actif. */
    const float mouse_look_sensitivity = config.mouse_sensitivity / ENG_CAMERA_LOOK_UNIT;
    EngCamera camera;
    eng_camera_init(&camera, (float[3]){0.0F, 3.0F, 14.0F}, 0.0F, -0.2F);
    EngCamera camera_prev = camera;
    if (eng_platform_set_mouse_capture(&platform, true)) {
        ENG_LOG_INFO("caméra libre : souris capturée (F2 pour la libérer), ZQSD/WASD, Espace/Ctrl "
                     "(haut/bas), Maj (sprint), molette (vitesse)");
    }

    /* Le jeu : bibliothèque dynamique rechargeable à chaud, état dans la mémoire permanente. */
    EngServices services;
    eng_game_services_init(&services, &memory);
    char default_game_path[512];
    if (game_path == NULL) {
        if (!eng_game_default_path(default_game_path, sizeof default_game_path)) {
            ENG_LOG_ERROR("chemin du jeu introuvable : utilise --game");
            eng_memory_shutdown(&memory);
            eng_gpu_shutdown(&gpu);
            eng_platform_shutdown(&platform);
            eng_log_shutdown();
            return 1;
        }
        game_path = default_game_path;
    }
    EngGameHost game;
    if (!eng_game_host_init(&game, game_path, &services, &memory.permanent, 0.25)) {
        eng_memory_shutdown(&memory);
        eng_gpu_shutdown(&gpu);
        eng_platform_shutdown(&platform);
        eng_log_shutdown();
        return 1;
    }

    /* Renderer et scène de démonstration (un cube qui tourne). Sans eux (shaders absents), l'image
     * est seulement effacée. En Debug, les shaders sont recompilés dès qu'on modifie le .hlsl. */
    EngRenderer renderer;
    DemoScene scene;
    bool use_renderer = false;
    if (use_gpu) {
        EngRendererDesc renderer_desc = {
            .depth_buffer = true,
            .hdr = true,
            .max_shadow_commands = 1024,
            /* deux samplers par modèle GPU : 25 cellules du monde, personnages, armes, et les
             * boîtes des créatures (6) et du décor (8) de la grande carte : 128 ne suffit plus */
            .max_samplers = 256,
            /* carte du monde : jusqu'à 256 morceaux de vue lointaine, les arbres par cellule */
            .max_commands = 8192,
            .max_uniform_bytes = 1024U * 1024U,
            /* personnages skinnés : 41 matrices d'os (~2,8 Kio) par pièce, aussi dans l'ombre ;
             * B8 : sur un terrain, 4 cascades x 5 personnages x 6 pièces ~ 340 Kio */
            .max_shadow_uniform_bytes = 1024U * 1024U};
#ifdef OW_SHADER_SOURCE_DIR
        renderer_desc.source_dir = OW_SHADER_SOURCE_DIR;
#endif
        if (combat_enabled) {
            /* B8 : le joueur apparaît en (4, 4), le camp est vers (-15, -11) : pas d'arbre à
             * moins de 45 m du centre (ils n'ont pas de collision) */
            demo_options.placement_clearing_m = 45.0F;
        }
        RendererJob job = {&renderer, &gpu, &memory, &renderer_desc, &scene, &demo_options};
        use_renderer = splash_run(&splash, &platform, renderer_job, &job, 0.5F);
        if (!use_renderer) {
            (void)eng_renderer_shutdown(&renderer);
            ENG_LOG_WARN("renderer indisponible : l'image sera seulement effacée");
        }
    }

    /* Interface de debug (M1-12) : Dear ImGui dessinée par-dessus la scène, dans une passe à part.
     * F1 l'affiche ou la masque ; masquée, elle ne coûte rien (aucune image d'interface). */
    EngUi ui = {0};
    static Title title;
    bool use_ui = false;
    if (use_renderer && eng_ui_available()) {
        use_ui = eng_ui_init(&ui, platform.window, gpu.device, eng_gpu_swapchain_format(&gpu));
        if (use_ui) {
            char font_path[1024];
            if (eng_path_resolve(font_path, sizeof font_path,
                                 "assets/fonts/NotoSans-Regular.ttf")) { /* M8-03 (OFL) */
                (void)eng_ui_set_font(&ui, font_path, 17.0F);
            }
            if (eng_path_resolve(font_path, sizeof font_path,
                                 "assets/fonts/NotoSans-Bold.ttf")) { /* titres des menus */
                (void)eng_ui_set_bold_font(&ui, font_path, 17.0F);
            }
            platform.event_hook = eng_ui_event_hook;
            platform.event_hook_user = &ui;
            /* la passe de l'interface : la scène de l'écran-titre (title.h), puis l'interface */
            title_init(&title, NULL, &ui);
            eng_renderer_set_overlay(&renderer, title_overlay_prepare, title_overlay_draw, &title);
            platform.debug_ui = start_with_ui;
            ENG_LOG_INFO("interface de debug : F1 l'affiche (la souris est alors libérée), F3 le "
                         "debug draw");
        } else {
            ENG_LOG_WARN("interface de debug indisponible");
        }
    }
    splash_frame(&splash, &platform); /* entre deux étapes : l'animation ne s'arrête pas */
    /* Terrain (--terrain) : point de vue au-dessus du centre, plan lointain et vitesse adaptés à un
     * terrain de l'ordre du kilomètre (la diagonale de 1025 m en fait 1450). */
    float terrain_position[3];
    if (use_renderer && demo_scene_terrain_view(&scene, terrain_position)) {
        eng_camera_init(&camera, terrain_position, 0.0F, -0.35F);
        camera.far_z = 3000.0F;
        eng_camera_set_speed(&camera, 40.0F);
        camera_prev = camera;
    }
    bool ui_was_shown = false;
    bool mouse_was_captured_before_ui = false;

    /* Éditeur (M2-10b) : un monde chargé depuis la scène, dessiné à la place du modèle central. */
    EditorMode editor_mode = {0};
    bool use_editor = false;
    bool selftest_done = false;
    bool selftest_failed = false;
    if (editor_enabled) {
        if (!use_renderer) {
            ENG_LOG_WARN("éditeur indisponible sans rendu GPU%s",
                         editor_selftest ? " : AUTOTEST ÉDITEUR : IGNORÉ (pas de rendu GPU)" : "");
            selftest_done = true;
        } else if (editor_mode_init(&editor_mode, scene_arg)) {
            use_editor = true;
            demo_scene_set_world(&scene, &editor_mode.world);
            if (use_ui) {
                platform.debug_ui = true; /* la fenêtre d'éditeur est affichée dès le départ */
            }
            ENG_LOG_INFO("éditeur : clic gauche = sélection, flèches de couleur = déplacement sur "
                         "l'axe, F2 = capturer/libérer la souris pour voler, F1 = interface");
        }
    }

    /* Variables réglables en direct par l'interface (adresses de nos propres variables). */
    EngTweaks tweaks;
    eng_tweaks_init(&tweaks);
    float demo_time_scale = 1.0F;
    float fov_degrees = camera.fov_y * (180.0F / 3.14159265F);
    float clear_color[3] = {gpu.clear_color[0], gpu.clear_color[1], gpu.clear_color[2]};
    (void)eng_tweaks_add_float(&tweaks, "vitesse de la démo", &demo_time_scale, 0.0F, 4.0F);
    (void)eng_tweaks_add_float(&tweaks, "champ de vision (deg)", &fov_degrees, 30.0F, 110.0F);
    (void)eng_tweaks_add_float(&tweaks, "vitesse caméra (m/s)", &camera.speed, ENG_CAMERA_SPEED_MIN,
                               50.0F);
    /* Tonemapping (M1-14) : la scène est en HDR, l'exposition et la courbe se règlent en direct. */
    EngTonemap tonemap;
    eng_tonemap_default(&tonemap);
    tonemap.exposure_ev = -0.5F; /* B2 : un demi-cran plus sombre (ciel moins blanc) */
    if (isfinite(exposure_arg)) {
        tonemap.exposure_ev = exposure_arg;
    }
    if (use_renderer) {
        tonemap = eng_renderer_tonemap(&renderer);
    }
    tonemap.fxaa = true;          /* post-process (M8-06) ; le menu Options le règle ensuite */
    tonemap.ssao = LAUNCHER_SSAO; /* finition M8-06 : occlusion ambiante */
    tonemap.bloom = 0.5F;
    int32_t tonemap_operator = (int32_t)tonemap.op;
    (void)eng_tweaks_add_int(&tweaks, "tonemap : 0 aucun, 1 ACES, 2 AgX", &tonemap_operator, 0,
                             (int32_t)ENG_TONEMAP_COUNT - 1);
    (void)eng_tweaks_add_float(&tweaks, "exposition (EV)", &tonemap.exposure_ev,
                               ENG_EXPOSURE_EV_MIN, ENG_EXPOSURE_EV_MAX);
    (void)eng_tweaks_add_bool(&tweaks, "anticrénelage FXAA", &tonemap.fxaa);
    (void)eng_tweaks_add_float(&tweaks, "SSAO : force", &tonemap.ssao, 0.0F, 1.0F);
    (void)eng_tweaks_add_float(&tweaks, "SSAO : rayon (m)", &tonemap.ssao_radius, 0.1F, 4.0F);
    (void)eng_tweaks_add_float(&tweaks, "bloom", &tonemap.bloom, 0.0F, 2.0F);
    (void)eng_tweaks_add_float(&tweaks, "fond rouge", &clear_color[0], 0.0F, 1.0F);
    (void)eng_tweaks_add_float(&tweaks, "fond vert", &clear_color[1], 0.0F, 1.0F);
    (void)eng_tweaks_add_float(&tweaks, "fond bleu", &clear_color[2], 0.0F, 1.0F);
    (void)eng_tweaks_add_bool(&tweaks, "debug draw (F3)", &platform.debug_draw);

    /* Mode jouable (M4) : monde physique, personnage, caméra 3e personne. */
    PlayMode play = {0};
    static AvatarLibrary avatars;  /* personnages animés (grand : hors pile) */
    static char combat_text[1024]; /* M5-16 : panneau « Combat » de F1 */
    static char ai_text[512];      /* M6-10 : ennemi sélectionné au clic */
    uint32_t pick_serial = 0; /* M6-10 : dernier appui vu (un clic bref tient dans une image) */
    bool use_play = false;
    static GameFlow flow;
    bool use_flow = false;
    bool flow_menu_was_open = false;
    SkeletonView skeleton_view = {0};
    PlayHook play_hook = {0};
    bool use_debug_hook = false;
    static WorldView world; /* M7 : ~1,3 Mo, hors pile */
    static FlowWorld flow_world;
    WorldHook world_hook = {&world, &play_hook, &scene};
    double world_travelled = 0.0;
    if (play_enabled) {
        if (!use_renderer || use_editor) {
            ENG_LOG_WARN("mode jouable indisponible %s",
                         use_editor ? "avec l'éditeur" : "sans rendu GPU");
        } else if (world_mode
                       ? splash_run(&splash, &platform, world_job,
                                    &(WorldJob){&world, &play, &tweaks, config.camera_sensitivity,
                                                config.camera_invert_y, &renderer, &scene,
                                                world_seed, world_start, at_zone,
                                                world_start_forced},
                                    0.85F)
                   : demo_options.terrain_path != NULL
                       ? play_mode_init_terrain(&play, &tweaks, config.camera_sensitivity,
                                                config.camera_invert_y, demo_options.terrain_path)
                       : play_mode_init(&play, &tweaks, config.camera_sensitivity,
                                        config.camera_invert_y)) {
            use_play = true;
            /* B8 : les troncs et rochers du placement arrêtent le joueur et la caméra */
            const DemoObstacle *obstacles = NULL;
            const uint32_t obstacle_total =
                world_mode ? 0U : demo_scene_obstacles(&scene, &obstacles);
            uint32_t obstacle_added = 0;
            while (obstacle_added < obstacle_total && /* monde plein : un seul avertissement */
                   play_mode_add_obstacle(&play, obstacles[obstacle_added].base,
                                          obstacles[obstacle_added].radius,
                                          obstacles[obstacle_added].height)) {
                obstacle_added++;
            }
            if (obstacle_total > 0) {
                play_mode_obstacles_done(&play);
                ENG_LOG_INFO("mode jouable : %u/%u obstacle(s) du décor (troncs, rochers)",
                             obstacle_added, obstacle_total);
            }
            if (combat_enabled && !splash_run(&splash, &platform, combat_job,
                                              &(CombatJob){&play, combat_audio}, 0.92F)) {
                ENG_LOG_WARN("combat indisponible : mode jouable sans combat");
            }
            play.autoplay = combat_autoplay;
            splash_frame(&splash, &platform);
            /* M8 : menus, sauvegarde, options ; la démonstration automatique saute le menu */
            if (save_dir != NULL) {
                (void)SDL_CreateDirectory(save_dir);
            }
            use_flow = play.combat != NULL && game_flow_init(&flow, &play, &platform.input,
                                                             save_dir, interactive && !windowed);
            if (use_flow && world_mode) { /* sauvegarde : position monde, relief rechargé */
                flow_world = (FlowWorld){&world, &play, &camera, &camera_prev};
                game_flow_set_world(
                    &flow, &(GameFlowWorld){&flow_world, flow_to_world, flow_place, flow_place_name,
                                            world.map != NULL ? ENG_BIOME_COUNT : 0U});
            }
            if (use_flow && windowed) { /* --windowed : ce lancement reste en fenêtre */
                flow.options.fullscreen = platform.fullscreen;
            }
            if (use_flow && flow.options.fullscreen != platform.fullscreen) {
                /* B19 : le plein écran enregistré, appliqué au lancement (ensuite les options
                 * suivent l'état de la fenêtre, image par image) */
                eng_platform_toggle_fullscreen(&platform);
            }
            if (use_flow && (start_new_game || combat_autoplay)) {
                game_flow_start_new(&flow, &play, creative_game);
            }
            if (creative_speed > 0.0) { /* vitesse de vol du mode créatif (sinon : la molette) */
                play_mode_set_creative_speed(&play, (float)creative_speed);
            }
            if (look_at_set) { /* développement : la caméra regarde un point de la carte */
                float at[3] = {0.0F, 0.0F, 0.0F};
                (void)eng_character_position(play.character, at);
                double from[3] = {(double)at[0], (double)at[1], (double)at[2]};
                if (world_mode) {
                    eng_origin_to_world(&world.origin, at, from);
                }
                const float yaw =
                    atan2f(-(float)(look_at[0] - from[0]), -(float)(look_at[1] - from[2]));
                const float pitch = (float)(look_pitch * 0.017453292519943295);
                play.orbit.yaw = yaw;
                play.orbit.want_yaw = yaw;
                play.orbit.pitch = pitch;
                play.orbit.want_pitch = pitch;
            }
            float gate[3];
            if (at_boss && play.combat != NULL && play_combat_boss_gate(play.combat, gate)) {
                gate[1] = play_mode_ground(&play, gate[0], gate[2]) + 0.05F; /* M6-09 */
                play_mode_teleport(&play, gate, 0.0F); /* regarde -Z : l'entrée, le boss */
            } else if (at_boss) {
                ENG_LOG_WARN("--at-boss : pas d'arène de boss (il faut --combat --terrain)");
            }
            if (gear_tier > 0 && play.combat != NULL) { /* butin des boss : palier d'essai */
                (void)play_combat_give_gear(play.combat, (uint32_t)gear_tier);
            }
            if (weapon_greatsword &&
                play.combat != NULL) { /* M8-08 : essai de l'espadon (après la nouvelle partie) */
                (void)play_combat_equip(play.combat, ENG_WEAPON_GREATSWORD, true);
            }
            if (use_flow && day_night_hour >= 0.0) {
                (void)eng_daynight_set_hour(&flow.daynight, day_night_hour);
            }
            if (use_flow && day_length > 0.0) {
                flow.daynight.day_seconds = (float)day_length;
            }
            play_hook.play = &play;
            use_debug_hook = true;
            play_mode_view(&play, &camera);
            camera_prev = camera;
            splash_frame(&splash, &platform);
            /* M4 : chevalier et squelettes animés à la place des capsules */
            if (splash_run(&splash, &platform, avatar_job,
                           &(AvatarJob){&avatars, &renderer,
                                        scene.has_environment ? &scene.environment : NULL},
                           0.98F) &&
                play_mode_attach_avatars(&play, &avatars)) {
                demo_scene_set_model_hook(&scene, launcher_model_hook, &play_hook);
                platform.debug_draw = false; /* capsules et grille : F3 */
                (void)play_mode_load_arena_visuals(&play, &renderer, &memory.permanent,
                                                   scene.has_environment ? &scene.environment
                                                                         : NULL);
            }
        }
    }
    splash_frame(&splash, &platform);
    if (world.ready) { /* le monde dessine le sol ; personnages en plus s'ils sont chargés */
        demo_scene_set_model_hook(&scene, world_model_hook, &world_hook);
        scene.world_mode = true;
        if (world.map != NULL) { /* carte du monde : on voit loin depuis la colline du château */
            if (!fog_from_command_line) {
                scene.fog.density = LAUNCHER_MAP_FOG;
            }
            camera.far_z = LAUNCHER_MAP_FAR;
        }
        (void)demo_scene_use_cascades(&scene, &renderer); /* M7-09 */
        platform.debug_draw = false;
    } else if (use_renderer && scene.has_terrain) {
        /* B8 : sur un terrain aussi, les personnages, arbres et rochers projettent leur ombre */
        (void)demo_scene_use_cascades(&scene, &renderer);
    }
    static AvatarGallery gallery;
    GalleryHook gallery_hook = {&gallery, &avatars};
    if (avatar_gallery_mode && use_renderer && !use_play) {
        char attacks_path[1024];
        (void)snprintf(attacks_path, sizeof attacks_path, "%s/combat/attacks.txt",
                       OW_ASSET_SOURCE_DIR);
        char *attacks_text = (char *)SDL_LoadFile(attacks_path, NULL);
        if (avatar_library_load_cpu(&avatars) &&
            avatar_library_load_gpu(&avatars, &renderer,
                                    scene.has_environment ? &scene.environment : NULL) &&
            avatar_gallery_init(&gallery, &avatars, attacks_text)) {
            demo_scene_set_model_hook(&scene, gallery_model_hook, &gallery_hook);
            scene.hide_props = true;
            avatar_gallery_camera(&gallery, &camera);
            camera_prev = camera;
            platform.debug_draw = false;
        }
        SDL_free(attacks_text);
    }
    /* Squelette de debug (M4-12) : os superposés au modèle central. */
    if (skeleton_path != NULL || anim_path != NULL) {
        if (!use_renderer) {
            ENG_LOG_WARN("--skeleton / --anim indisponibles sans rendu GPU");
        } else if (skeleton_path == NULL || skeleton_view_load(&skeleton_view, skeleton_path)) {
            /* M4-16 : --anim anime le squelette ; sans --skeleton, celui que désigne le clip */
            if (anim_path != NULL &&
                !skeleton_view_load_anim(&skeleton_view, skeleton_path, anim_path)) {
                skeleton_view_shutdown(&skeleton_view);
            }
            if (skeleton_view.ready && sockets_path != NULL &&
                !skeleton_view_load_sockets(&skeleton_view, sockets_path)) {
                skeleton_view_shutdown(&skeleton_view);
            }
            if (skeleton_view.ready) {
                play_hook.skeleton = &skeleton_view;
                use_debug_hook = true;
                if (skeleton_view.animated) {
                    EngAnimDebug *a = &skeleton_view.anim;
                    (void)eng_tweaks_add_bool(&tweaks, "animation : pause (P)", &a->paused);
                    (void)eng_tweaks_add_float(&tweaks, "animation : instant (s)", &a->time, 0.0F,
                                               skeleton_view.clip.duration);
                    (void)eng_tweaks_add_float(&tweaks, "animation : vitesse", &a->speed,
                                               -ENG_ANIM_DEBUG_MAX_SPEED, ENG_ANIM_DEBUG_MAX_SPEED);
                    (void)eng_tweaks_add_bool(&tweaks, "animation : boucle", &a->loop);
                    (void)eng_tweaks_add_bool(&tweaks, "animation : pose de repos", &a->show_rest);
                    (void)eng_tweaks_add_bool(&tweaks, "animation : trajectoire (T)",
                                              &a->show_trail);
                    (void)eng_tweaks_add_int(&tweaks, "animation : articulation suivie (J K)",
                                             &a->selected, -1,
                                             (int32_t)skeleton_view.skeleton.joint_count - 1);
                }
            }
        }
    }
    if (use_debug_hook) {
        demo_scene_set_debug_hook(&scene, launcher_debug_hook, &play_hook);
    }
    /* Éclairage du modèle (PBR, M1-15) : le soleil se règle par azimut et hauteur. */
    EngLighting lighting;
    eng_lighting_default(&lighting);
    /* intensité de l'environnement (M1-16) : 1 = le ciel tel quel ; 0,5 depuis B2 (à 1, l'ambiante
     * du ciel écrasait le contraste du soleil : image délavée) */
    lighting.ambient_intensity = 0.5F;
    if (isfinite(ambient_arg)) {
        lighting.ambient_intensity = ambient_arg;
    }
    float sun_azimuth = demo_options.sun_azimuth;
    float sun_elevation = demo_options.sun_elevation;
    /* Cycle jour / nuit (M8-07) : celui de la partie, sinon une horloge locale (--day-night). */
    static EngDayNight local_daynight;
    eng_daynight_init(&local_daynight, day_night_hour >= 0.0 ? day_night_hour : 9.0);
    if (day_length > 0.0) {
        local_daynight.day_seconds = (float)day_length;
    }
    EngDayNight *daynight = use_flow ? &flow.daynight : (use_day_night ? &local_daynight : NULL);
    float daynight_hour_ui = daynight != NULL ? (float)daynight->hour : 0.0F;
    float daynight_hour_shown = daynight_hour_ui;
    if (daynight != NULL) {
        (void)eng_tweaks_add_float(&tweaks, "jour/nuit : heure", &daynight_hour_ui, 0.0F, 23.99F);
        (void)eng_tweaks_add_float(&tweaks, "jour/nuit : vitesse", &daynight->speed, 0.0F, 60.0F);
    }
    (void)eng_tweaks_add_float(&tweaks, "soleil : azimut (deg)", &sun_azimuth, -180.0F, 180.0F);
    (void)eng_tweaks_add_float(&tweaks, "soleil : hauteur (deg)", &sun_elevation, -90.0F, 90.0F);
    if (use_renderer) {
        (void)eng_tweaks_add_float(&tweaks, "brouillard : densité (1/m)", &scene.fog.density, 0.0F,
                                   0.02F);
        (void)eng_tweaks_add_float(&tweaks, "brouillard : décroissance (1/m)",
                                   &scene.fog.height_falloff, 0.0F, 0.02F);
        (void)eng_tweaks_add_float(&tweaks, "brouillard : opacité max", &scene.fog.max_opacity,
                                   0.0F, 1.0F);
    }
    (void)eng_tweaks_add_float(&tweaks, "soleil : intensité", &lighting.sun_intensity, 0.0F, 4.0F);
    (void)eng_tweaks_add_float(&tweaks, "environnement : intensité", &lighting.ambient_intensity,
                               0.0F, 4.0F);
    /* Ombre du soleil (M1-17) : biais en texels de la carte, rayon du filtre, force. */
    bool shadow_enabled = true;
    EngShadowParams shadow_params;
    eng_shadow_params_default(&shadow_params);
    int32_t shadow_radius = (int32_t)shadow_params.pcf_radius;
    (void)eng_tweaks_add_bool(&tweaks, "ombre : active", &shadow_enabled);
    (void)eng_tweaks_add_float(&tweaks, "ombre : biais constant (texels)",
                               &shadow_params.constant_bias, 0.0F, 8.0F);
    (void)eng_tweaks_add_float(&tweaks, "ombre : biais de pente", &shadow_params.slope_bias, 0.0F,
                               4.0F);
    (void)eng_tweaks_add_float(&tweaks, "ombre : biais normal (texels)", &shadow_params.normal_bias,
                               0.0F, 8.0F);
    (void)eng_tweaks_add_int(&tweaks, "ombre : rayon du filtre (texels)", &shadow_radius, 0,
                             (int32_t)ENG_SHADOW_MAX_PCF_RADIUS);
    (void)eng_tweaks_add_float(&tweaks, "ombre : force", &shadow_params.strength, 0.0F, 1.0F);
    EngFrameStats frame_stats;
    eng_frame_stats_init(&frame_stats);
    double demo_time = 0.0;
    double previous_cpu_ms = 0.0;
    uint64_t draws_last_frame = 0;
    uint64_t binds_last_frame = 0;
    uint64_t uniforms_last_frame = 0;

    /* Fin de l'écran de chargement : le titre finit d'apparaître, la scène s'éteint. Une touche
     * pressée pendant l'animation ne doit pas agir sur le menu qui suit. */
    if (splash.ready) {
        /* vers l'écran-titre : la scène continue, sans sortie ; vers le jeu : éclat puis noir */
        const bool to_title = use_flow && use_ui && game_flow_in_title(&flow);
        splash_finish(&splash, &platform, !to_title);
        eng_input_release_all(&platform.input);
        EngInputState discarded;
        eng_input_begin_tick(&platform.input, &discarded);
        if (use_flow && !to_title) {
            game_flow_fade_in(&flow); /* l'écran de chargement finit au noir : le jeu en sort */
        }
        if (use_ui) {
            title_init(&title, &splash, &ui);
        }
    }
    if (use_flow) { /* fondus entre le titre et le jeu : avec l'écran de chargement seulement */
        game_flow_set_transitions(&flow, splash.ready, title_ready(&title));
    }
    /* --shots : numéros des images à écrire */
    long shot_frames[64];
    uint32_t shot_count = 0;
    uint32_t shot_next = 0;
    static char shot_path[1100];
    for (const char *sp = shots_list; sp != NULL && *sp != '\0' && shot_count < 64U;) {
        char *end = NULL;
        const long n = strtol(sp, &end, 10);
        if (end == sp) {
            break;
        }
        shot_frames[shot_count++] = n;
        sp = *end == ',' ? end + 1 : end;
    }
    if (shots_dir != NULL) {
        (void)SDL_CreateDirectory(shots_dir);
    }

    /* Échauffement : SDL alloue paresseusement sa file d'événements. On la sollicite une fois
     * hors du compteur. */
    bool alive = eng_platform_pump(&platform);

    /* Le compteur couvre la mise à jour (événements et jeu). L'affichage logiciel provisoire en
     * est exclu : SDL_UpdateWindowSurface alloue 2 fois par image en interne (mesuré avec le
     * pilote dummy). Il disparaît avec Vulkan (M1) ; le compteur couvrira alors l'image
     * entière. Le contrôle de rechargement du jeu (accès disque, copie) est hors compteur lui
     * aussi : c'est un outil de développement. */
    EngClock clock;
    eng_clock_init(&clock);
    EngLoop loop;
    if (!eng_loop_init(&loop, clock.frequency, 60, 0.25)) {
        if (use_ui) {
            platform.event_hook = NULL;
            eng_ui_shutdown(&ui);
        }
        if (use_renderer) {
            demo_scene_shutdown(&scene, &renderer);
            (void)eng_renderer_shutdown(&renderer);
        }
        eng_game_host_shutdown(&game);
        eng_memory_shutdown(&memory);
        eng_gpu_shutdown(&gpu);
        eng_platform_shutdown(&platform);
        eng_log_shutdown();
        return 1;
    }
    uint64_t frame = 0;
    PostBench post_bench;
    post_bench_init(&post_bench, (uint32_t)post_bench_frames);
    ENG_PROF_THREAD_NAME("principal");
    while (alive && (max_frames < 0 || (long)frame < max_frames)) {
        ENG_PROF_ZONE(z_frame, "frame");
        eng_clock_tick(&clock);
        const uint64_t frame_start = eng_clock_counter();
        if (frame > 0) { /* durée TOTALE de l'image précédente et son temps CPU */
            (void)eng_frame_stats_push(&frame_stats, (float)(clock.dt * 1000.0),
                                       (float)previous_cpu_ms);
        }
        ENG_PROF_ZONE(z_poll, "game_reload_check");
        (void)eng_game_host_poll(&game, clock.time);
        if (use_renderer) {
            /* Rechargement des shaders : outil de développement, hors compteur d'allocations. */
            (void)eng_renderer_update(&renderer, clock.time);
            demo_scene_update(&scene, clock.time); /* recuisson et rechargement des assets */
        }
        ENG_PROF_ZONE_END(z_poll);
        eng_memory_begin_frame(&memory);
        ENG_PROF_ZONE(z_events, "events");
        alive = eng_platform_pump(&platform);
        if (use_flow) {
            flow.options.fullscreen = platform.fullscreen;
            /* nom d'une partie : le clavier écrit (les touches du jeu sont alors ignorées) */
            eng_platform_set_text_input(&platform, game_flow_wants_text(&flow));
            char typed[64];
            if (eng_platform_take_text(&platform, typed, sizeof typed) > 0U) {
                game_flow_text(&flow, typed);
            }
            menu_script_frame(&menu_script, &flow, frame, &ui, &platform);
        }
        if (save_selftest && use_flow && world.ready && frame == 8U) {
            world_save_selftest(&flow, &play, &world);
        }
        if (stress_resize && use_gpu) {
            /* taille différente à chaque image, sans attendre que la précédente soit prise en
             * compte */
            const int width = 160 + (int)((frame * 37U) % 700U);
            const int height = 120 + (int)((frame * 53U) % 500U);
            (void)eng_platform_set_window_size(&platform, width, height);
        }
        ENG_PROF_ZONE_END(z_events);
        uint32_t steps = eng_loop_advance(&loop, clock.dt_counter);
        if (menu_script.count > 0U) {
            steps = 1; /* --menu-script : un tick par image, le même déroulé sur toute machine */
        }
        if (warp_ticks > 0 && frame == 2U) { /* --warp : le temps du jeu avance d'un coup */
            steps += (uint32_t)warp_ticks;
        }
        ENG_PROF_PLOT("pas de simulation", steps);
        ENG_PROF_ZONE(z_sim, "simulation");
        uint32_t frozen_steps = 0; /* F6 : ticks avancés pendant le gel (animations) */
        for (uint32_t i = 0; i < steps; i++) {
            ENG_PROF_ZONE(z_tick, "tick");
            EngInputState input;
            platform.input.axes[ENG_AXIS_LOOK].mouse_sensitivity =
                platform.mouse_captured ? mouse_look_sensitivity : 0.0F;
            eng_input_begin_tick(&platform.input, &input); /* UNE fois par pas */
            if (menu_script.pending != 0U) { /* --menu-script : touches du menu jouées */
                input.menu_pressed |= menu_script.pending;
                input.menu_held |= menu_script.pending;
                menu_script.pending = 0;
            }
            camera_prev = camera;
            if (use_play) {
                /* F5 : jeu gelé ; F6 : un tick par pression (débogage du combat, M5-16) */
                bool run_tick = !platform.debug_freeze;
                if (platform.debug_freeze && platform.debug_steps > 0U) {
                    platform.debug_steps--;
                    run_tick = true;
                    frozen_steps++;
                }
                if (run_tick &&
                    (!use_flow || game_flow_tick(&flow, &play, &input, (float)loop.tick_seconds))) {
                    play_mode_tick(&play, &input, (float)loop.tick_seconds);
                }
                float boss_xz[2];
                float boss_alt = 0.0F;
                if (look_at_boss && play.combat != NULL &&
                    play_combat_boss_point(play.combat, boss_xz, &boss_alt)) {
                    /* développement : la caméra suit le boss (captures du dragon en vol) */
                    float at[3] = {0.0F, 0.0F, 0.0F};
                    (void)eng_character_position(play.character, at);
                    const float dx = boss_xz[0] - at[0];
                    const float dz = boss_xz[1] - at[2];
                    const float up =
                        boss_alt + 20.0F + play_mode_ground(&play, boss_xz[0], boss_xz[1]) - at[1];
                    /* un peu de côté : le personnage, devant la caméra, ne cache pas le boss */
                    play.orbit.want_yaw = atan2f(-dx, -dz) + 0.3F;
                    play.orbit.want_pitch = atan2f(up, hypotf(dx, dz) + 1.0F);
                    play.orbit.yaw = play.orbit.want_yaw;
                    play.orbit.pitch = play.orbit.want_pitch > ENG_ORBIT_PITCH_MAX
                                           ? ENG_ORBIT_PITCH_MAX
                                           : play.orbit.want_pitch;
                }
                play_mode_view(&play, &camera);
                if (use_flow && flow.quit) {
                    alive = false;
                }
                if (use_flow && flow.graphics_dirty) { /* M8-06 */
                    tonemap.fxaa = flow.options.fxaa;  /* appliqué au renderer plus bas */
                    tonemap.bloom = flow.options.bloom;
                    tonemap.ssao = flow.options.ssao ? LAUNCHER_SSAO : 0.0F;
                    if (use_gpu) {
                        eng_gpu_set_render_scale(&gpu, flow.options.render_scale);
                    }
                    /* finition M8 : vsync, plein écran, qualité des ombres, distance de vue */
                    const EngPresentMode mode =
                        flow.options.vsync ? ENG_PRESENT_VSYNC : ENG_PRESENT_IMMEDIATE;
                    if (use_gpu && gpu.present_mode != mode) {
                        (void)eng_gpu_set_present_mode(&gpu, mode);
                        platform.vsync = gpu.present_mode == ENG_PRESENT_VSYNC;
                    }
                    if (platform.fullscreen != flow.options.fullscreen) {
                        eng_platform_toggle_fullscreen(&platform);
                    }
                    shadow_radius = (int32_t)flow.options.shadow_quality; /* filtre 0, 1, 2 */
                    /* plan lointain : x 0,5 / 1 / 1,5 du défaut du mode (1 km ; 3 km terrain) */
                    const float base_far = world.map != NULL                   ? LAUNCHER_MAP_FAR
                                           : demo_options.terrain_path != NULL ? 3000.0F
                                                                               : 1000.0F;
                    camera.far_z = base_far * (0.5F + (0.5F * (float)flow.options.view_distance));
                    static const char *const quality[3] = {"bas", "moyen", "haut"};
                    ENG_LOG_INFO(
                        "options graphiques : vsync %s (présentation %s), plein écran %s, "
                        "ombres %s, plan lointain %.0f m, FXAA %s, bloom %.0f %%, "
                        "SSAO %s, échelle %.0f %%",
                        flow.options.vsync ? "oui" : "non",
                        gpu.present_mode == ENG_PRESENT_VSYNC ? "vsync" : "immédiate",
                        platform.fullscreen ? "oui" : "non",
                        quality[flow.options.shadow_quality % 3U], (double)camera.far_z,
                        flow.options.fxaa ? "oui" : "non", (double)(flow.options.bloom * 100.0F),
                        flow.options.ssao ? "oui" : "non", (double)(gpu.render_scale * 100.0F));
                    flow.graphics_dirty = false;
                }
            } else {
                eng_camera_update(&camera, &input, (float)loop.tick_seconds);
            }
            ENG_PROF_ZONE(z_update, "game.update");
            game.api->update(game.state, &input, loop.tick_seconds);
            ENG_PROF_ZONE_END(z_update);
            ENG_PROF_ZONE_END(z_tick);
        }
        ENG_PROF_ZONE_END(z_sim);
        /* F5 : jeu gelé : on montre le dernier tick (pas d'interpolation qui tremble) */
        const double frame_alpha = platform.debug_freeze ? 1.0 : loop.alpha;
        eng_memory_end_frame(&memory);
        EngClockStats stats;
        if (eng_clock_take_stats(&clock, &stats)) {
            char title[96];
            snprintf(title, sizeof title, "Open World - %.1f fps, dt %.2f ms (max %.2f ms)%s",
                     stats.fps, stats.avg_dt_ms, stats.max_dt_ms, platform.vsync ? " [vsync]" : "");
            eng_platform_set_title(&platform, title);
            ENG_LOG_DEBUG("%.1f fps, dt moyen %.2f ms, max %.2f ms, vsync %s", stats.fps,
                          stats.avg_dt_ms, stats.max_dt_ms, platform.vsync ? "oui" : "non");
        }
        if (alive) {
            EngDebugSquare square;
            ENG_PROF_ZONE(z_render, "game.render");
            game.api->render(game.state, loop.alpha, &square);
            ENG_PROF_ZONE_END(z_render);
            ENG_PROF_ZONE(z_present, "present");
            if (use_gpu) {
                /* Le renderer exécute la file de commandes ; le carré du jeu n'est visible
                 * qu'avec --software tant que le jeu ne dessine pas par le renderer. */
                if (use_renderer) {
                    /* Temps de la démo : accumulé ici pour que son réglage (curseur) ne fasse pas
                     * sauter la rotation. dt est plafonné comme celui de la simulation. */
                    demo_time += (clock.dt < 0.25 ? clock.dt : 0.25) * (double)demo_time_scale;
                    const double sim_time = demo_time;
                    camera.fov_y = fov_degrees * (3.14159265F / 180.0F);
                    camera_prev.fov_y = camera.fov_y;
                    eng_renderer_set_clear_color(&renderer, clear_color[0], clear_color[1],
                                                 clear_color[2], 1.0F);
                    tonemap.op = (EngTonemapOperator)tonemap_operator;
                    EngTonemap frame_tonemap = tonemap;
                    scene.night = 0.0F;
                    if (daynight != NULL) {
                        if (daynight_hour_ui != daynight_hour_shown) { /* curseur F1 déplacé */
                            (void)eng_daynight_set_hour(daynight, (double)daynight_hour_ui);
                        }
                        if (daynight == &local_daynight) {
                            eng_daynight_tick(daynight, clock.dt < 0.25 ? clock.dt : 0.25);
                        }
                        eng_daynight_sun(daynight, &sun_azimuth, &sun_elevation);
                        scene.night = eng_daynight_night_weight(sun_elevation);
                        if (use_play && play.combat != NULL) {
                            play_combat_set_night(play.combat, scene.night); /* M8-01 */
                        }
                        frame_tonemap.exposure_ev +=
                            eng_daynight_exposure_bias(daynight, sun_elevation);
                        daynight_hour_ui = (float)daynight->hour;
                        daynight_hour_shown = daynight_hour_ui;
                    }
                    if (post_bench.active && /* finition M8-06 : coût de chaque effet */
                        !post_bench_frame(&post_bench, clock.dt * 1000.0, &frame_tonemap, &gpu)) {
                        alive = false;
                    }
                    eng_renderer_set_tonemap(&renderer, &frame_tonemap);
                    const float aspect =
                        platform.height > 0 ? (float)platform.width / (float)platform.height : 1.0F;
                    EngCamera view_camera;
                    eng_camera_lerp(&camera_prev, &camera, (float)loop.alpha, &view_camera);
                    skeleton_view_update(&skeleton_view, (float)clock.dt,
                                         !use_ui || !eng_ui_wants_keyboard(&ui));
                    if (gallery.ready) {
                        avatar_gallery_update(&gallery, &avatars, (float)clock.dt);
                    }
                    combat_text[0] = '\0';
                    ai_text[0] = '\0';
                    if (use_play && play.combat != NULL) {
                        /* M5-16 / M6-10 : panneau F1 ; clic dans la vue = sélection d'un ennemi */
                        (void)play_combat_debug_text(play.combat, combat_text, sizeof combat_text);
                        float mx = 0.0F;
                        float my = 0.0F;
                        (void)SDL_GetMouseState(&mx, &my);
                        /* l'appui est un ÉVÉNEMENT : lire l'état du bouton raterait un clic dont
                         * l'appui et la relâche arrivent dans la même image */
                        const bool click = platform.input.last_serial != pick_serial &&
                                           platform.input.last_kind == ENG_BIND_MOUSE_BUTTON &&
                                           platform.input.last_code == SDL_BUTTON_LEFT;
                        pick_serial = platform.input.last_serial;
                        if (platform.debug_ui && !platform.mouse_captured && click &&
                            (!use_ui || !eng_ui_wants_mouse(&ui))) {
                            int ww = 0;
                            int wh = 0;
                            (void)SDL_GetWindowSize(platform.window, &ww, &wh);
                            EngCamera pick_camera;
                            eng_camera_lerp(&camera_prev, &camera, (float)loop.alpha, &pick_camera);
                            EngRay ray;
                            const float aspect_pick = wh > 0 ? (float)ww / (float)wh : 1.0F;
                            if (eng_ray_from_screen(&pick_camera, aspect_pick, mx, my, (float)ww,
                                                    (float)wh, &ray)) {
                                float feet[3];
                                (void)eng_character_position(play.character, feet);
                                (void)play_combat_pick(play.combat, ray.origin, ray.dir,
                                                       play_mode_ground(&play, feet[0], feet[2]));
                            }
                        }
                        (void)play_combat_ai_text(play.combat, ai_text, sizeof ai_text);
                    }
                    if (world.ready && use_play) { /* M7 : streaming, origine flottante */
                        world_travelled += world_frame(&world, &play, &camera, &camera_prev, &scene,
                                                       world_autopilot, clock.dt);
                        if (frame > 30U) { /* hors chargement initial et premières images */
                            world_view_note_frame(&world, (float)(clock.dt * 1000.0),
                                                  (float)previous_cpu_ms);
                        }
                        eng_camera_lerp(&camera_prev, &camera, (float)loop.alpha, &view_camera);
                    }
                    if (use_play) { /* animation des personnages : une fois par image */
                        play.show_debug = platform.debug_draw;
                        const bool paused = use_flow && eng_menu_is_open(&flow.menu);
                        /* F5 : les animations sont gelées avec le combat ; F6 les avance d'un
                         * tick, comme la simulation */
                        const float visual_dt =
                            platform.debug_freeze
                                ? (float)((double)frozen_steps * loop.tick_seconds)
                                : (float)clock.dt;
                        play_mode_update_visuals(&play, frame_alpha, paused ? 0.0F : visual_dt);
                    }
                    EngRenderQueue *queue = eng_renderer_begin_frame(&renderer);
                    (void)eng_lighting_set_sun_angles(&lighting, sun_azimuth, sun_elevation);
                    shadow_params.pcf_radius = (uint32_t)shadow_radius;
                    /* écran-titre : sa scène couvre tout l'écran, le jeu n'est pas dessiné */
                    const bool title_on =
                        use_flow && title_ready(&title) && game_flow_in_title(&flow);
                    /* animation du menu : au rythme des images ; pas fixe pour les lancements
                     * automatiques (l'image n° N montre toujours le même instant) */
                    const float menu_dt = interactive ? (float)clock.dt : 1.0F / 60.0F;
                    if (!title_on) {
                        demo_scene_draw(&scene, &renderer, queue, sim_time, &view_camera, aspect,
                                        &lighting, shadow_enabled ? &shadow_params : NULL);
                    }
                    if (use_editor) {
                        if (editor_selftest) {
                            /* le modèle est chargé par demo_scene_update : on attend quelques
                             * images, puis le test se déroule en une seule image */
                            if (!selftest_done && frame >= 8) {
                                char out[512];
                                if (eng_path_resolve(out, sizeof out,
                                                     "logs/editor_selftest.scene") &&
                                    editor_mode_selftest(&editor_mode, &scene, &view_camera, aspect,
                                                         (float)platform.width,
                                                         (float)platform.height, out)) {
                                    selftest_failed = false;
                                } else {
                                    selftest_failed = true;
                                }
                                selftest_done = true;
                            }
                        } else if (!platform.mouse_captured) {
                            float mouse_x = 0.0F;
                            float mouse_y = 0.0F;
                            const SDL_MouseButtonFlags buttons =
                                SDL_GetMouseState(&mouse_x, &mouse_y);
                            int window_w = 0;
                            int window_h = 0;
                            (void)SDL_GetWindowSize(platform.window, &window_w, &window_h);
                            EngRay ray;
                            if (eng_ray_from_screen(&view_camera, aspect, mouse_x, mouse_y,
                                                    (float)window_w, (float)window_h, &ray)) {
                                (void)eng_editor_pointer(&editor_mode.editor, &editor_mode.world,
                                                         demo_scene_entity_bounds, &scene, &ray,
                                                         view_camera.position,
                                                         (buttons & SDL_BUTTON_LMASK) != 0,
                                                         use_ui && eng_ui_wants_mouse(&ui));
                            }
                        } else {
                            eng_editor_drag_end(
                                &editor_mode.editor); /* souris capturée : pas de clic */
                        }
                    }
                    play_hook.alpha = frame_alpha;
                    if (!title_on && (platform.debug_draw || use_editor || use_play ||
                                      play_hook.skeleton != NULL)) {
                        /* F3 ; en dernier : les lignes recouvrent la scène */
                        demo_scene_draw_debug(&scene, &renderer, queue, sim_time, &view_camera,
                                              aspect, platform.debug_draw,
                                              use_editor ? &editor_mode.editor : NULL);
                    }
                    if (use_ui) {
                        /* F1 : à l'ouverture la souris est libérée (sinon on ne pourrait pas
                         * cliquer), et rendue à la fermeture si elle était capturée avant. */
                        if (platform.debug_ui != ui_was_shown) {
                            if (platform.debug_ui) {
                                mouse_was_captured_before_ui = platform.mouse_captured;
                                (void)eng_platform_set_mouse_capture(&platform, false);
                            } else if (mouse_was_captured_before_ui) {
                                (void)eng_platform_set_mouse_capture(&platform, true);
                            }
                            ui_was_shown = platform.debug_ui;
                        }
                        if (use_flow) { /* menu ouvert : souris libre ; fermé : regard à la souris
                                         */
                            const bool menu_open = eng_menu_is_open(&flow.menu);
                            if (menu_open != flow_menu_was_open && !platform.debug_ui) {
                                (void)eng_platform_set_mouse_capture(&platform, !menu_open);
                            }
                            flow_menu_was_open = menu_open;
                        }
                        eng_ui_set_visible(&ui, platform.debug_ui || use_flow);
                        if (eng_ui_begin_frame(&ui, platform.width, platform.height,
                                               (float)clock.dt)) {
                            EngUiPanelData panel = {
                                .frames = &frame_stats,
                                .draws = (uint32_t)draws_last_frame,
                                .pipeline_binds = (uint32_t)binds_last_frame,
                                .uniform_pushes = (uint32_t)uniforms_last_frame,
                                .rejected_commands = renderer.stats.rejected_commands,
                                .dropped_commands = renderer.stats.dropped_commands,
                                .terrain_chunks_drawn = scene.terrain_chunks_drawn,
                                .terrain_chunks_total = scene.terrain_chunk_total,
                                .arena_count = 3,
                                .frame_allocs = eng_memory_frame_allocs(&memory),
                                .tweaks = &tweaks,
                                .animation = skeleton_view.animated ? skeleton_view.status : NULL,
                                .combat = combat_text[0] != '\0' ? combat_text : NULL,
                                .ai = ai_text[0] != '\0' ? ai_text : NULL,
                            };
                            panel.arenas[0] =
                                (EngUiArena){"permanent", memory.permanent.used,
                                             memory.permanent.peak, memory.permanent.capacity};
                            panel.arenas[1] =
                                (EngUiArena){"level", memory.level.used, memory.level.peak,
                                             memory.level.capacity};
                            panel.arenas[2] =
                                (EngUiArena){"frame", memory.frame.used, memory.frame.peak,
                                             memory.frame.capacity};
                            if (platform.debug_ui) {
                                eng_ui_draw_panel(&ui, &panel);
                            }
                            if (use_flow) {
                                game_flow_draw(&flow, &ui, &play, &view_camera, aspect,
                                               platform.width, platform.height, menu_dt);
                            }
                            if (use_editor) {
                                EngUiEditorResult editor_result = {0};
                                eng_ui_draw_editor(&ui, &editor_mode.world, &editor_mode.editor,
                                                   &editor_result);
                                if (editor_result.save_requested) {
                                    (void)editor_mode_save(&editor_mode);
                                }
                            }
                            eng_ui_end_frame(&ui);
                        }
                    }
                    title_frame(&title, use_flow ? &flow : NULL, menu_dt, (uint32_t)platform.width,
                                (uint32_t)platform.height);
                    if (shot_next < shot_count && (gpu.capture_path == NULL || gpu.captured)) {
                        /* --shots : l'image suivante de la liste */
                        (void)snprintf(shot_path, sizeof shot_path, "%s/image-%04ld.ppm", shots_dir,
                                       shot_frames[shot_next]);
                        gpu.capture_path = shot_path;
                        gpu.capture_frame = (uint64_t)shot_frames[shot_next];
                        gpu.captured = false;
                        shot_next++;
                    }
                    /* Fin du travail CPU de l'image (la présentation, qui attend la synchro
                     * verticale, n'en fait pas partie). */
                    previous_cpu_ms =
                        eng_clock_ms_between(&clock, frame_start, eng_clock_counter());
                    const EngRendererStats before = renderer.stats;
                    eng_renderer_end_frame(&renderer);
                    draws_last_frame = renderer.stats.draws - before.draws;
                    binds_last_frame = renderer.stats.pipeline_binds - before.pipeline_binds;
                    uniforms_last_frame = renderer.stats.uniform_pushes - before.uniform_pushes;
                } else {
                    (void)eng_gpu_begin_frame(&gpu);
                    eng_gpu_end_frame(&gpu);
                }
            } else {
                eng_platform_present(&platform, &square);
            }
            ENG_PROF_ZONE_END(z_present);
            frame++;
        }
        ENG_PROF_ZONE_END(z_frame);
        ENG_PROF_FRAME();
    }

    ENG_LOG_INFO(
        "simulation : %llu pas à %u Hz, %llu image(s) plafonnée(s), %u rechargement(s) du jeu",
        (unsigned long long)loop.tick_count, (unsigned)loop.tick_hz,
        (unsigned long long)loop.clamped_frames, (unsigned)game.reload_count);
    eng_memory_log_stats(&memory);
    if (use_flow) { /* état du menu à l'arrêt (tests de --menu-script) */
        uint32_t used_slots = 0;
        for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
            used_slots += flow.menu.slots[i].used ? 1U : 0U;
        }
        ENG_LOG_INFO("MENU : écran %d, ligne %d ; %u emplacement(s) occupé(s) ; partie en cours "
                     "« %s »%s%s ; scène du titre : %llu image(s)",
                     (int)flow.menu.screen, (int)flow.menu.selected, used_slots, flow.name,
                     flow.creative ? " en mode créatif" : "",
                     flow.started && !game_flow_in_title(&flow) ? " (en jeu)" : "",
                     (unsigned long long)title.frames);
        if (flow.creative && play_enabled) {
            ENG_LOG_INFO("CRÉATIF : vitesse de vol %.2f m/s",
                         (double)play_mode_creative_speed(&play));
        }
    }
    if (use_ui) {
        platform.event_hook = NULL;
        eng_renderer_set_overlay(&renderer, NULL, NULL, NULL);
        eng_ui_shutdown(&ui); /* avant le GPU : le backend détient des ressources GPU */
    }
    eng_tweaks_clear(&tweaks);
    if (use_renderer) {
        ENG_LOG_INFO("renderer : %llu image(s), %llu passe(s) d'ombre",
                     (unsigned long long)renderer.stats.frames,
                     (unsigned long long)renderer.stats.shadow_passes);
        if (renderer.stats.dropped_commands != 0U) { /* file ou uniforms trop petits */
            ENG_LOG_WARN("renderer : %llu dessin(s) perdu(s) (file ou uniforms pleins)",
                         (unsigned long long)renderer.stats.dropped_commands);
        }
        if (use_editor) {
            demo_scene_set_world(&scene, NULL);
            editor_mode_shutdown(&editor_mode);
        }
        demo_scene_set_debug_hook(&scene, NULL, NULL);
        if (world.ready) {
            world_view_log_summary(&world, world_travelled);
            float feet[3];
            float ground = 0.0F;
            if (use_play && eng_character_position(play.character, feet) &&
                world_view_height(&world, feet[0], feet[2], &ground)) {
                const float gap = feet[1] - ground;
                if (fabsf(gap) < 0.3F) {
                    ENG_LOG_INFO("MONDE : joueur au sol (%.2f m)", (double)gap);
                } else {
                    ENG_LOG_ERROR("MONDE : TROU sous le joueur (%.2f m du sol)", (double)gap);
                }
            }
            if (use_play && world.water.ready) { /* carte du monde : lacs et rivières */
                ENG_LOG_INFO("MONDE : eau : %u cellules mouillées ; joueur %s",
                             world.water.wet_cells, play.wading ? "dans l'eau, à gué" : "au sec");
            }
            if (use_play && play.combat != NULL) { /* M7 : le camp dans le monde */
                PlayCombatStats cs;
                play_combat_stats(play.combat, &cs);
                ENG_LOG_INFO("monde : combat : %u ennemi(s) engagé(s) au plus, %u coup(s) reçu(s), "
                             "%u ennemi(s) vaincu(s)",
                             cs.enemies_engaged, cs.player_hits_taken, cs.enemy_kills);
            }
            demo_scene_set_model_hook(&scene, NULL, NULL);
            world_view_shutdown(&world); /* avant le monde physique : retire ses corps */
        }
        if (use_play) {
            play_mode_shutdown(&play);
        }
        skeleton_view_shutdown(&skeleton_view);
        demo_scene_set_model_hook(&scene, NULL, NULL);
        play_mode_unload_visuals(&play, &renderer);
        avatar_library_shutdown(&avatars, &renderer);
        demo_scene_shutdown(&scene, &renderer);
        const uint32_t leaked = eng_renderer_shutdown(&renderer);
        if (leaked != 0) {
            ENG_LOG_WARN("renderer : %u ressource(s) non détruite(s) libérée(s) à l'arrêt", leaked);
        }
    }
    eng_game_host_shutdown(&game);
    eng_memory_shutdown(&memory);
    int exit_code = selftest_failed ? 1 : 0;
    if (use_gpu) {
        eng_gpu_wait_idle(&gpu);
        splash_shutdown(&splash); /* la scène de l'écran de chargement et du titre */
        const bool balanced = eng_gpu_resources_balanced(&gpu);
        ENG_LOG_INFO(
            "GPU : %llu redimensionnement(s), %llu image(s) ignorée(s), %u image(s) en vol "
            "au plus : %s",
            (unsigned long long)gpu.resizes, (unsigned long long)gpu.frames_skipped,
            (unsigned)gpu.max_pending, balanced ? "sans fuite" : "FUITE de barrière ou de cible");
        if (!balanced || (stress_resize && gpu.frames_skipped != 0)) {
            exit_code = 1;
        }
    }
    if (!eng_gpu_shutdown(&gpu)) {
        exit_code = 1;
    }
    eng_platform_shutdown(&platform);
    ENG_LOG_INFO("arrêt propre après %llu images", (unsigned long long)frame);
    eng_log_shutdown();
    return exit_code;
}
