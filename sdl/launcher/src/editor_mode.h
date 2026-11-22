#pragma once

#include <stdbool.h>

#include "demo_scene.h"
#include "engine/camera.h"
#include "engine/editor.h"
#include "engine/world.h"

/* Mode éditeur du lanceur (M2-10b, option --editor) : possède le monde édité et l'état de
 * l'éditeur, charge et enregistre la scène. Le dessin et la souris sont branchés dans main.c ; ce
 * module ne connaît ni SDL ni la fenêtre. */

#define EDITOR_DEFAULT_SCENE "assets/scenes/demo.scene"
#define EDITOR_WORLD_CAPACITY 4096U

typedef struct EditorMode {
    EngWorld world;
    EngEditor editor;
    char scene_path[512]; /* chemin tel que donné (relatif à l'exécutable, ou absolu) */
    bool
        save_allowed; /* faux si le fichier existait mais n'a pas pu être lu : on ne l'écrase pas */
    bool ready;
} EditorMode;

/* Crée le monde et le remplit depuis `scene_path` (NULL : EDITOR_DEFAULT_SCENE). Fichier absent :
 * monde par défaut (un modèle central), le fichier sera créé à l'enregistrement. Fichier présent
 * mais invalide : erreur journalisée avec la ligne fautive, monde par défaut, ENREGISTREMENT
 * REFUSÉ (le fichier abîmé n'est jamais écrasé). false si le monde n'a pas pu être créé. */
bool editor_mode_init(EditorMode *mode, const char *scene_path);
void editor_mode_shutdown(EditorMode *mode);

/* Écrit le monde dans la scène ; false (journalisé) si l'enregistrement est refusé ou échoue. */
bool editor_mode_save(const EditorMode *mode);

/* Autotest de bout en bout (--editor-selftest), à appeler quand le modèle central est chargé :
 * projette l'objet à l'écran, le saisit avec la machine à états de la souris (rayons issus de
 * pixels par eng_ray_from_screen), tire sa poignée X de 3 m, enregistre la scène dans
 * `output_path`, la relit dans un monde neuf et compare. Journalise « AUTOTEST ÉDITEUR : OK » ou la
 * raison de l'échec. `width`/`height` : taille de la fenêtre (pixels). */
bool editor_mode_selftest(EditorMode *mode, DemoScene *scene, const EngCamera *camera, float aspect,
                          float width, float height, const char *output_path);
