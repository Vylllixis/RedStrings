#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/camera.h"
#include "engine/debug_lines.h"
#include "engine/math.h"
#include "engine/world.h"

/* Éditeur minimal en jeu (M2-10, docs/editor.md) : la LOGIQUE, sans SDL ni GPU (donc testable) :
 * rayon sous la souris, sélection par rayon (picking), poignées de déplacement (gizmo à trois axes)
 * et déplacement d'une entité en tenant compte de son parent. L'inspecteur (widgets ImGui) est dans
 * engine/ui.h ; le branchement aux entrées et au rendu est dans le lanceur.
 *
 *     EngRay ray;
 *     eng_ray_from_screen(&camera, aspect, mouse_x, mouse_y, width, height, &ray);
 *     clic : axis = eng_editor_gizmo_hit(&editor, &world, &ray, camera.position);
 *            si axis != 0 : eng_editor_drag_begin(&editor, &world, axis, &ray)
 *            sinon        : eng_editor_select(&editor, &world, eng_editor_pick(&world, bounds,
 * user, &ray, NULL)) souris déplacée et bouton maintenu : eng_editor_drag_update(&editor, &world,
 * &ray) bouton relâché : eng_editor_drag_end(&editor)
 *
 * Conventions : unités et repère du moteur (docs/conventions.md) ; toutes les positions de cette
 * API sont des positions MONDE ; le déplacement se fait selon les axes du monde, pas ceux de
 * l'objet. */

/* Rayon : origine monde et direction UNITAIRE. */
typedef struct EngRay {
    float origin[3];
    float dir[3];
} EngRay;

/* Rayon qui part de l'oeil et passe par le pixel (x, y) (origine en haut à gauche, comme les
 * événements de souris) d'une fenêtre de width × height pixels. `aspect` : largeur / hauteur de la
 * projection utilisée pour dessiner. false si une entrée n'est pas finie, si une dimension n'est
 * pas > 0 ou si le pixel est hors de la fenêtre. */
bool eng_ray_from_screen(const EngCamera *camera, float aspect, float x, float y, float width,
                         float height, EngRay *out);

/* Le rayon coupe-t-il la boîte [min, max] (repère local) posée dans le monde par `model` ? Si oui,
 * `*t` (si non NULL) reçoit la distance monde depuis l'origine du rayon jusqu'à l'entrée dans la
 * boîte (0 si l'origine est dedans). La boîte suit donc la rotation et l'échelle de l'objet. false
 * aussi si `model` n'est pas inversible (échelle nulle) ou si la boîte est inversée (min > max). */
bool eng_ray_vs_box(const EngRay *ray, const float box_min[3], const float box_max[3], mat4 model,
                    float *t);

/* Point le plus proche entre le rayon et la droite (point, dir) (`dir` unitaire) : `*line_s` =
 * abscisse sur la droite, `*ray_t` = distance sur le rayon (peut être négative : derrière
 * l'origine), `*distance` = écart entre les deux points. false si le rayon est (presque) parallèle
 * à la droite (sinus de l'angle < ENG_RAY_PARALLEL_SIN). */
#define ENG_RAY_PARALLEL_SIN 0.05F
bool eng_ray_closest_to_line(const EngRay *ray, const float point[3], const float dir[3],
                             float *line_s, float *ray_t, float *distance);

/* ── Sélection ── */

/* Boîte englobante LOCALE d'une entité (celle de son modèle, par exemple). false : pas de boîte,
 * l'entité n'est pas sélectionnable à la souris. */
typedef bool (*EngEditorBoundsFn)(void *user, EngEntity entity, float min[3], float max[3]);

enum {
    ENG_GIZMO_NONE = 0,
    ENG_GIZMO_X = 1,
    ENG_GIZMO_Y = 2,
    ENG_GIZMO_Z = 3,
};

typedef struct EngEditor {
    EngEntity selected;
    float drag_origin[3]; /* position monde de l'entité au début du glissement */
    float drag_start;     /* abscisse du point saisi sur l'axe, au début du glissement */
    uint32_t hover_axis;  /* poignée sous la souris (mise à jour par l'appelant), 0 si aucune */
    uint32_t drag_axis;   /* poignée en cours de glissement, 0 si aucun glissement */
    bool button_was_down; /* état du bouton à l'appel précédent de eng_editor_pointer */
} EngEditor;

void eng_editor_init(EngEditor *editor);

/* Sélectionne `entity` (ENG_ENTITY_NULL : désélectionne). false si l'entité n'existe plus ; la
 * sélection est alors inchangée. Termine un glissement en cours. */
bool eng_editor_select(EngEditor *editor, const EngWorld *world, EngEntity entity);

/* Entité sélectionnée, ou ENG_ENTITY_NULL si elle a été détruite depuis (la sélection est alors
 * effacée, et un glissement en cours interrompu). À appeler une fois par image. */
EngEntity eng_editor_selected(EngEditor *editor, const EngWorld *world);

/* L'entité la plus proche touchée par le rayon : parmi celles qui ont un MeshRenderer visible et
 * une boîte donnée par `bounds`. `*t` (si non NULL) reçoit la distance ; ENG_ENTITY_NULL si rien.
 */
EngEntity eng_editor_pick(const EngWorld *world, EngEditorBoundsFn bounds, void *user,
                          const EngRay *ray, float *t);

/* ── Position monde ── */

/* Position monde de l'origine de l'entité (colonne de translation de sa matrice monde). */
bool eng_editor_world_position(const EngWorld *world, EngEntity entity, float out[3]);

/* Place l'origine de l'entité à `position` (monde) en modifiant sa position LOCALE à travers
 * l'inverse de la matrice monde de son parent. Rotation et échelle ne bougent pas. false si
 * l'entité est invalide, si `position` n'est pas finie ou si le parent n'est pas inversible.
 * L'entité reçoit un Transform si elle n'en avait pas. */
bool eng_editor_set_world_position(EngWorld *world, EngEntity entity, const float position[3]);

/* ── Poignées de déplacement (gizmo) ── */

#define ENG_GIZMO_SCREEN_FRACTION 0.2F /* longueur d'une poignée / distance à la caméra */
#define ENG_GIZMO_MIN_LENGTH 0.05F     /* mètres */
#define ENG_GIZMO_PICK_RADIUS 0.08F    /* tolérance de saisie / longueur de la poignée */

/* Longueur en mètres des poignées d'un objet en `origin` vu de `camera_position` : proportionnelle
 * à la distance, donc de taille constante à l'écran. */
float eng_editor_gizmo_length(const float camera_position[3], const float origin[3]);

/* La poignée (ENG_GIZMO_X/Y/Z) touchée par le rayon, ENG_GIZMO_NONE sinon (y compris sans entité
 * sélectionnée). Les trois poignées partent de la position monde de l'entité sélectionnée. */
uint32_t eng_editor_gizmo_hit(const EngEditor *editor, const EngWorld *world, const EngRay *ray,
                              const float camera_position[3]);

/* Commence à tirer la poignée `axis` : le point de la poignée le plus proche du rayon est « saisi
 * ». false : pas de sélection, axe invalide, ou rayon parallèle à la poignée. */
bool eng_editor_drag_begin(EngEditor *editor, const EngWorld *world, uint32_t axis,
                           const EngRay *ray);

/* Déplace l'entité pour que le point saisi reste sous le rayon, sur l'axe monde choisi. false (rien
 * ne bouge) sans glissement en cours, ou si le rayon est presque parallèle à l'axe (la position
 * serait instable : le dernier emplacement valide est gardé). */
bool eng_editor_drag_update(EngEditor *editor, EngWorld *world, const EngRay *ray);

void eng_editor_drag_end(EngEditor *editor);

/* ── Dessin ── */

/* Ajoute à `lines` (listes SUPERPOSÉES, puis restaure l'état précédent) la boîte de l'entité
 * sélectionnée (si `bounds` en donne une) et les trois poignées : rouge X, vert Y, bleu Z ; la
 * poignée survolée ou tirée est jaune. */
void eng_editor_draw(const EngEditor *editor, const EngWorld *world, EngEditorBoundsFn bounds,
                     void *user, EngDebugLines *lines, const float camera_position[3]);

/* ── Édition de la scène (utilisée par l'inspecteur, testable sans interface) ── */

/* Crée une entité « entité N » (premier N libre) avec un Transform par défaut, enfant de la
 * sélection si `as_child` et qu'il y en a une, et la sélectionne. ENG_ENTITY_NULL si le monde est
 * plein. */
EngEntity eng_editor_new_entity(EngEditor *editor, EngWorld *world, bool as_child);
/* Détruit la sélection et ses descendants, puis désélectionne. false s'il n'y avait rien. */
bool eng_editor_delete_selected(EngEditor *editor, EngWorld *world);

/* Quaternion (x, y, z, w) ↔ angles d'Euler en degrés (ordre XYZ, comme l'inspecteur les affiche).
 * Le quaternion produit est unitaire ; l'aller-retour est exact hors du blocage de cardan
 * (|Y| proche de 90°). */
void eng_editor_quat_to_euler(const float quat[4], float degrees[3]);
void eng_editor_euler_to_quat(const float degrees[3], float quat[4]);

/* Machine à états de la souris, à appeler UNE fois par image avec le rayon sous le pointeur et
 * l'état du bouton gauche (le pointeur ne doit pas être sur l'interface : sinon l'appelant passe
 * `over_ui` à true, ce qui annule un glissement en cours et ignore l'appui).
 *   - appui (bouton passé de haut en bas) : une poignée sous le rayon démarre un glissement ;
 *     sinon on sélectionne l'objet touché, ou rien (désélection) ;
 *   - bouton maintenu pendant un glissement : l'objet suit la souris sur l'axe ;
 *   - bouton relâché : fin du glissement ; sans bouton, `hover_axis` suit le pointeur.
 * Retourne true si une position a été modifiée pendant cet appel. */
bool eng_editor_pointer(EngEditor *editor, EngWorld *world, EngEditorBoundsFn bounds, void *user,
                        const EngRay *ray, const float camera_position[3], bool button_down,
                        bool over_ui);
