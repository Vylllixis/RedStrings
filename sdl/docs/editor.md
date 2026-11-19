# Éditeur minimal (M2-10)

L'éditeur déplace des objets du monde (ADR 0012/0011) à la souris et enregistre la scène sans quitter
le jeu. Ce document décrit la couche moteur (`engine/editor.h`) et la fenêtre (`eng_ui_draw_editor`).
L'intégration au lanceur (`--editor`) est décrite plus bas.

## Couche moteur : `engine/editor.h`

Aucun GPU, aucune fenêtre : tout est testé analytiquement (`tests/test_editor.c`).

| Fonction | Rôle |
|---|---|
| `eng_ray_from_screen` | rayon monde passant par un pixel (caméra, aspect, taille) ; refuse pixel hors écran, taille ≤ 0, NaN |
| `eng_ray_vs_box` | rayon contre boîte alignée DANS le repère local d'une matrice modèle (rotation/échelle OK) ; `t` est une distance monde |
| `eng_ray_closest_to_line` | point de la droite le plus proche du rayon (`s` sur la droite, `t` sur le rayon, distance) ; refuse les rayons quasi parallèles |
| `eng_editor_pick` | plus proche MeshRenderer visible touché ; les bornes viennent d'un callback (le modèle chargé) |
| `eng_editor_set_world_position` | pose la position MONDE via l'inverse de la matrice du parent ; ajoute un Transform si absent |
| `eng_editor_gizmo_hit` / `drag_begin` / `drag_update` / `drag_end` | poignées X/Y/Z : sélection par distance rayon–axe, glisser contraint à la droite de l'axe |
| `eng_editor_draw` | boîte de sélection + trois flèches (jaune si survolée/glissée) en overlay de debug |
| `eng_editor_pointer` | machine à états de la souris, une fois par image : appui sur une poignée → glissement, sinon sélection de l'objet touché (ou désélection) ; bouton maintenu → l'objet suit ; relâché → fin, survol recalculé ; `over_ui` ignore l'appui et abandonne un glissement |
| `eng_editor_new_entity` / `delete_selected` | création (« entité N » libre, enfant optionnel), suppression du sous-arbre |
| `eng_editor_quat_to_euler` / `euler_to_quat` | conversion degrés XYZ ↔ quaternion pour l'inspecteur |

La longueur des poignées vaut 20 % de la distance caméra–objet (taille écran à peu près constante,
minimum 5 cm). Tolérance de sélection d'une poignée : 8 % de sa longueur.

Une sélection devient « périmée » quand l'entité est détruite : `eng_editor_selected` la remet alors à
zéro (et annule un glisser en cours), sans jamais rendre un handle mort.

## Fenêtre : `eng_ui_draw_editor`

- **Barre** : Enregistrer (met `result->save_requested` ; l'appelant écrit le fichier), Nouvelle,
  Supprimer.
- **Hiérarchie** : ordre parent-puis-enfants, enfants décalés ; clic = sélection ; 64 lignes au plus
  (« … et N autre(s) » au-delà).
- **Inspecteur** : pour chaque composant présent sur l'entité sélectionnée, un widget par champ décrit
  (bool : case ; i32/u32 : entier ; float : glisseur ; vec3 : trois glisseurs ; quat : trois angles en
  degrés ; string : champ texte). Modification en place.

Les rectangles des lignes, des boutons et des champs sont conservés dans `EngUi` pour les tests
(`eng_ui_editor_row_rect`, `eng_ui_editor_field_rect`).

## Dans le lanceur : `launcher --editor`

```
launcher --editor                       # charge assets/scenes/demo.scene (relatif à l'exécutable)
launcher --editor --scene ~/RedStrings/sdl/assets/scenes/demo.scene   # scène du dépôt (chemin absolu)
```

- La scène chargée REMPLACE le modèle central tournant ; la grille de cubes, le sol, le ciel et les
  ombres restent. Chaque entité avec Transform + MeshRenderer visible est dessinée avec sa matrice monde
  (parents compris) ; le modèle est demandé au gestionnaire d'assets d'après `MeshRenderer.model`
  (16 chemins distincts au plus).
- La fenêtre « Éditeur » est affichée dès le départ et la souris est libre. **Clic gauche** : sélection
  (boîte cyan + trois flèches). **Glisser une flèche** : déplacement sur l'axe. **Enregistrer** : écrit
  la scène. **F2** capture la souris pour voler (ZQSD…), F2 la libère ; F1 masque l'interface.
- Où est écrit le fichier ? Là où pointe `--scene`. Sans option, c'est la COPIE dans le dossier de
  build (`assets/` y est recopié à chaque `cmake --build`, ce qui l'écrase) : pour garder vos
  modifications, passez le chemin de la scène du dépôt.
- Fichier absent : monde par défaut (un modèle central), créé à l'enregistrement. Fichier présent mais
  illisible : erreur avec le numéro de ligne, monde par défaut, **enregistrement désactivé** (le fichier
  n'est jamais écrasé).

### Autotest de bout en bout (`ctest -R launcher.editor`)

`launcher --editor-selftest` projette le modèle central à l'écran, le sélectionne et tire sa poignée X de
3 m avec `eng_editor_pointer` (les rayons viennent de pixels via `eng_ray_from_screen`, donc la
cohérence projection ↔ rayon est vérifiée sur la vraie caméra), enregistre dans `logs/`, relit dans un
monde neuf et compare la position bit à bit. Un `eng_ray_from_screen` au signe de y inversé fait
échouer ce test (vérifié). Ce que l'autotest ne couvre PAS : les événements SDL réels
(`SDL_GetMouseState`, boutons), le clic sur le bouton Enregistrer dans le lanceur (le bouton est
testé dans `engine.editor_ui`, l'écriture dans `launcher.editor`), l'apparence à l'écran.

## Limites

Translation par poignées seulement ; pas d'annulation ; pas de duplication ni de re-parentage à la
souris ; le champ quaternion passe par des angles d'Euler (près de ±90° en Y, plusieurs triplets
représentent la même rotation) ; au plus 32 champs suivis pour les tests. Voir ADR 0013.
