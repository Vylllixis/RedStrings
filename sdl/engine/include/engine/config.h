#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "engine/arena.h"
#include "engine/input.h"

/* Fichier de configuration : texte simple « clé = valeur », un réglage par ligne.
 *
 *   # commentaire (aussi avec « ; »)
 *   resolution = 1920x1080
 *   fullscreen = false
 *   vsync = true
 *   mouse_sensitivity = 0.002
 *   stick_deadzone = 0.2
 *   bind.attack = mouse:left, pad:west
 *   bind.move.keys = A, D, S, W
 *
 * Règles :
 *  - fichier absent -> valeurs par défaut (cas normal, aucun message d'erreur) ;
 *  - clé absente -> valeur par défaut de cette clé ;
 *  - ligne invalide (clé inconnue, valeur illisible ou hors limites) -> avertissement avec
 *    nom du fichier et numéro de ligne, la ligne est ignorée, le reste du fichier est lu.
 *    Une faute de frappe ne doit jamais empêcher de jouer.
 *
 * Liaisons : `bind.<action> = source, source, ...` REMPLACE les liaisons par défaut de cette
 * action (au plus ENG_MAX_BUTTON_BINDINGS). `none` désactive l'action. Sources :
 *   key:<nom>      nom de touche SDL, position physique : W, Space, Left Ctrl, F5...
 *   mouse:<nom>    left, middle, right, x1, x2
 *   pad:<nom>      south east west north (ou a b x y), back, guide, start, left_stick,
 *                  right_stick, left_shoulder, right_shoulder, dpad_up/down/left/right
 *   trigger:<nom>  left, right
 * Actions : attack, heavy_attack, dodge, jump, interact, sprint, lock_on, pause. */

typedef struct EngConfig {
    int window_width;
    int window_height;
    bool fullscreen;
    bool vsync;
    float mouse_sensitivity;
    float stick_deadzone;

    bool action_overridden[ENG_ACTION_COUNT];
    EngButtonBinding action_bindings[ENG_ACTION_COUNT][ENG_MAX_BUTTON_BINDINGS];
    bool move_keys_set;
    int move_keys[4]; /* gauche, droite, bas, haut (scancodes) */

    int issue_count; /* lignes ignorées lors des lectures */
} EngConfig;

#define ENG_CONFIG_MAX_FILE_SIZE ((size_t)256 * 1024)

void eng_config_defaults(EngConfig *config);

/* Lit un texte (terminé par un octet nul) par-dessus les valeurs déjà présentes. `source` sert
 * aux messages. Retourne le nombre de lignes ignorées. */
int eng_config_parse(EngConfig *config, const char *text, const char *source);

/* Valeurs par défaut, puis lecture du fichier s'il existe. `scratch` sert de mémoire de lecture
 * temporaire (remise dans son état d'origine ensuite). Retourne true si le fichier a été lu,
 * false s'il est absent ou illisible (les défauts sont alors utilisés). */
bool eng_config_load(EngConfig *config, EngArena *scratch, const char *path);

/* Applique les liaisons, la sensibilité et la zone morte à un EngInput déjà muni des liaisons
 * par défaut, avant tout événement. */
void eng_config_apply_input(const EngConfig *config, EngInput *input);

/* Exposé pour les tests : « key:Space », « pad:west »... false si illisible. */
bool eng_config_parse_binding(const char *text, EngButtonBinding *out);
