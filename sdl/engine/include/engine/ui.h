#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/editor.h"
#include "engine/frame_stats.h"
#include "engine/tweak.h"

/* Interface de debug (M1-12) : Dear ImGui (API C : cimgui) avec ses backends SDL3 (entrées) et
 * SDL_GPU (rendu). Le moteur ne dessine pas l'interface lui-même : le renderer appelle
 * eng_ui_prepare puis eng_ui_draw (voir eng_renderer_set_overlay) dans une passe à part, au-dessus
 * de la scène.
 *
 * Utilisation, une fois par image (après la scène, avant eng_renderer_end_frame) :
 *     if (eng_ui_begin_frame(&ui, w, h, dt)) {   // faux si masquée : aucun travail
 *         eng_ui_draw_panel(&ui, &data);
 *         eng_ui_end_frame(&ui);
 *     }
 * et, une fois, eng_renderer_set_overlay(&r, eng_ui_overlay_prepare, eng_ui_overlay_draw, &ui) et
 * platform.event_hook = eng_ui_event_hook (les entrées vont à l'interface avant le jeu).
 *
 * Sans OW_UI (CMake), tout ceci se compile mais n'a aucun effet : eng_ui_init retourne false. */

struct SDL_Window;
struct SDL_GPUDevice;
struct SDL_GPUCommandBuffer;
struct SDL_GPURenderPass;
struct EngGpu;

#define ENG_UI_EDITOR_MAX_ROWS 64U
#define ENG_UI_EDITOR_MAX_FIELDS 32U

typedef struct EngUiFieldRect {
    float rect[4];
    uint32_t component; /* identifiant du composant */
    uint32_t field;     /* indice du champ dans sa description */
} EngUiFieldRect;

typedef struct EngUi {
    bool ready;        /* contexte créé */
    bool visible;      /* affichée (F1) : masquée, begin_frame ne fait rien */
    bool frame_open;   /* begin_frame appelé, end_frame pas encore */
    bool draw_pending; /* des sommets attendent d'être dessinés (end_frame → draw) */
    uint32_t vertices; /* sommets de la dernière image terminée */
    uint64_t frames;   /* images d'interface terminées */
    /* Rectangles écran (x0, y0, x1, y1, pixels) des widgets de réglage de la dernière image, pour
     * piloter l'interface dans les tests (cliquer au centre d'un curseur) sans deviner la mise en
     * page. Vides si la section Réglages est repliée. */
    float tweak_rects[ENG_TWEAK_MAX][4];
    uint32_t tweak_rect_count;
    float reset_rect[4];
    /* Idem pour l'éditeur (eng_ui_draw_editor) : une ligne de hiérarchie par entité affichée. */
    float editor_rows[ENG_UI_EDITOR_MAX_ROWS][4];
    EngEntity editor_row_entities[ENG_UI_EDITOR_MAX_ROWS];
    float editor_save_rect[4];
    float editor_add_rect[4];
    float editor_delete_rect[4];
    EngUiFieldRect editor_fields[ENG_UI_EDITOR_MAX_FIELDS];
    uint32_t editor_row_count;
    uint32_t editor_field_count;
    bool has_reset_rect;
    bool has_editor_rects;
} EngUi;

/* Un arène à afficher dans le panneau. */
typedef struct EngUiArena {
    const char *name; /* littéral */
    size_t used;
    size_t peak;
    size_t capacity;
} EngUiArena;

#define ENG_UI_MAX_ARENAS 4U

/* Ce que le panneau montre. Tout est fourni par l'appelant : le panneau ne lit aucun état global.
 */
typedef struct EngUiPanelData {
    const EngFrameStats *frames; /* durées d'images ; NULL : section absente */
    /* Rendu, pour la DERNIÈRE image terminée (différences des compteurs cumulés du renderer) */
    uint32_t draws;
    uint32_t pipeline_binds;
    uint32_t uniform_pushes;
    uint64_t rejected_commands; /* cumulés : doivent rester à 0 */
    uint64_t dropped_commands;
    /* Terrain (M3-04) : chunks dessinés / chunks du terrain à la dernière image ; total 0 : ligne
     * absente */
    uint32_t terrain_chunks_drawn;
    uint32_t terrain_chunks_total;
    /* Mémoire */
    EngUiArena arenas[ENG_UI_MAX_ARENAS];
    uint32_t arena_count;
    uint64_t frame_allocs; /* allocations pendant les images depuis le début (objectif : 0) */
    EngTweaks *tweaks;     /* réglages en direct ; NULL : section absente */
    const char *animation; /* M4-16 : ligne d'état du lecteur d'animation ; NULL : absente */
    const char *combat;    /* M5-16 : état des combattants (texte multi-lignes) ; NULL : absente */
    const char *ai;        /* M6-10 : ennemi sélectionné au clic (texte) ; NULL : absente */
} EngUiPanelData;

/* true si le moteur a été compilé avec l'interface (OW_UI). */
bool eng_ui_available(void);

/* Crée le contexte. `window` NULL : mode sans fenêtre (tests : taille et entrées données à la
 * main). `color_format` : SDL_GPUTextureFormat de la cible de l'overlay (eng_gpu_swapchain_format).
 * false si OW_UI est absent, si un contexte existe déjà ou si un backend refuse. */
bool eng_ui_init(EngUi *ui, struct SDL_Window *window, struct SDL_GPUDevice *device,
                 uint32_t color_format);
void eng_ui_shutdown(EngUi *ui); /* sans effet si déjà arrêtée */

void eng_ui_set_visible(EngUi *ui, bool visible);

/* Commence l'image d'interface. false (rien à faire ni à terminer) si l'interface n'est pas prête,
 * masquée, ou si une image est déjà ouverte. */
bool eng_ui_begin_frame(EngUi *ui, int width, int height, float dt);
/* Panneau de debug (statistiques, mémoire, réglages) ; entre begin_frame et end_frame. */
void eng_ui_draw_panel(EngUi *ui, const EngUiPanelData *data);
/* Ce que la fenêtre d'éditeur a demandé pendant cette image. */
typedef struct EngUiEditorResult {
    bool save_requested; /* « Enregistrer » cliqué : c'est l'appelant qui écrit le fichier */
} EngUiEditorResult;

/* Fenêtre d'éditeur : barre d'outils (Enregistrer, nouvelle entité, nouvelle entité enfant,
 * supprimer), hiérarchie (ENG_UI_EDITOR_MAX_ROWS lignes affichées, ordre parent-puis-enfants) et
 * inspecteur de l'entité sélectionnée : un widget par champ de chaque composant présent, d'après la
 * description du composant (cases, entiers, flottants, vec3, quaternions en degrés d'Euler,
 * chaînes). Les valeurs sont modifiées en place. Entre begin_frame et end_frame. */
void eng_ui_draw_editor(EngUi *ui, EngWorld *world, EngEditor *editor, EngUiEditorResult *result);
/* Rectangle du widget du champ `field` (indice dans la description) du composant `component`
 * (identifiant) de la dernière image ; false s'il n'est pas affiché (au plus
 * ENG_UI_EDITOR_MAX_FIELDS champs sont suivis). */
bool eng_ui_editor_field_rect(const EngUi *ui, uint32_t component, uint32_t field, float out[4]);
/* Rectangle de la ligne de hiérarchie de `entity` ; false si elle n'est pas affichée. */
bool eng_ui_editor_row_rect(const EngUi *ui, EngEntity entity, float out[4]);

/* Termine l'image : les données de dessin sont prêtes pour l'overlay. */
void eng_ui_end_frame(EngUi *ui);

/* Hooks à donner à eng_renderer_set_overlay (user = EngUi*) : prepare envoie les données au GPU (et
 * retourne false s'il n'y a rien à dessiner) ; draw les dessine dans la passe overlay. */
bool eng_ui_overlay_prepare(void *ui, struct SDL_GPUCommandBuffer *commands);
void eng_ui_overlay_draw(void *ui, struct SDL_GPUCommandBuffer *commands,
                         struct SDL_GPURenderPass *pass);

/* Filtre d'événements pour EngPlatform.event_hook (user = EngUi*). */
bool eng_ui_event_hook(void *ui, const void *sdl_event);

/* Rectangle du widget du réglage `index` (celui de eng_tweaks) dans la dernière image : écrit
 * x0, y0, x1, y1 dans `out`. false s'il n'a pas été dessiné (section repliée, indice invalide). */
bool eng_ui_tweak_rect(const EngUi *ui, uint32_t index, float out[4]);
/* Idem pour le bouton « Tout réinitialiser ». */
bool eng_ui_reset_rect(const EngUi *ui, float out[4]);

/* L'interface veut la souris (pointeur sur un panneau) ou le clavier (champ de saisie actif). */
bool eng_ui_wants_mouse(const EngUi *ui);
bool eng_ui_wants_keyboard(const EngUi *ui);

/* Entrées synthétiques, pour les tests : position de la souris (pixels), bouton (0 : gauche). */
void eng_ui_add_mouse_pos(EngUi *ui, float x, float y);
void eng_ui_add_mouse_button(EngUi *ui, int button, bool down);
