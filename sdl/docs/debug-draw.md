# Debug draw (M1-11)

Des **lignes colorées dans le monde** — segments, boîtes, sphères, repères, grille, flèches, frustum —
pour voir ce que le code calcule (volumes de collision, trajectoires, repères) sans écrire de
modèle ni de shader. Deux modules :

- `engine/debug_lines.h` — **côté CPU** : accumule les lignes de l'image. Ne touche ni SDL ni le GPU
  (se teste seul).
- `engine/debug_lines_gpu.h` — **côté GPU** : envoie les lignes par un buffer de streaming et met en
  file au plus deux dessins (`eng_debug_lines_gpu_draw`).

(`engine/debug_draw.h`, le carré du jeu pour l'affichage logiciel provisoire, est un autre sujet : il
reste tel quel jusqu'à ce que le jeu dessine par le renderer.)

```c
EngDebugLines lines;      eng_debug_lines_init(&lines, &permanent, 4096);          /* lignes par liste */
EngDebugLinesGpu gpu;     eng_debug_lines_gpu_create(&gpu, &renderer, 4096, 0);    /* 0 : swapchain */

chaque image :
    eng_debug_lines_clear(&lines);
    eng_debug_grid(&lines, (vec3){0, -0.5F, 0}, 30, 2.0F, ENG_DEBUG_GRAY, ENG_DEBUG_WHITE);
    eng_debug_aabb(&lines, box_min, box_max, ENG_DEBUG_YELLOW);
    eng_debug_lines_set_overlay(&lines, true);                 /* visible à travers tout */
    eng_debug_axes(&lines, transform, 1.0F);
    eng_debug_lines_set_overlay(&lines, false);
    EngRenderQueue *q = eng_renderer_begin_frame(&renderer);
    ...dessin de la scène...
    eng_debug_lines_gpu_draw(&gpu, &renderer, q, &lines, view_proj);   /* EN DERNIER */
    eng_renderer_end_frame(&renderer);
```

## Les formes

| Fonction | Lignes | Remarque |
|---|---|---|
| `eng_debug_line(a, b, couleur)` | 1 | |
| `eng_debug_aabb(min, max, c)` | 12 | l'ordre min/max n'importe pas |
| `eng_debug_box(transform, half, c)` | 12 | boîte orientée : le cube `[-half, +half]³` transformé ; `half` ≥ 0 |
| `eng_debug_circle(centre, u, v, c, segments)` | `segments` | `u` et `v` déjà à l'échelle du rayon ; boucle fermée exactement |
| `eng_debug_sphere(centre, rayon, c, segments)` | 3 × `segments` | trois cercles (plans XY, YZ, XZ) ; rayon ≥ 0 |
| `eng_debug_axes(transform, taille)` | 3 | X rouge, Y vert, Z bleu, depuis l'origine de la matrice |
| `eng_debug_grid(centre, cases, taille_case, c, c_majeure)` | 2 × (2 × cases + 1) | plan XZ à la hauteur de `centre` ; ligne majeure toutes les 10 cases ; ≤ 1024 cases |
| `eng_debug_arrow(de, vers, c)` | 5 | quatre barbes de 15 % de la longueur ; longueur nulle : la ligne seule |
| `eng_debug_frustum(view_proj, c)` | 12 | coins reconstruits par l'inverse de la matrice ; refusé si non inversible |

`segments` : 0 → 32 ; sinon borné à [3, 256]. Les couleurs : `ENG_DEBUG_RGB(r, g, b)` ou les
constantes (`ENG_DEBUG_WHITE`, `_GRAY`, `_RED`, `_GREEN`, `_BLUE`, `_YELLOW`, `_CYAN`, `_MAGENTA`).

## Règles

- **Tout ou rien.** Une forme entre en entier ou pas du tout : pas de boîte à moitié dessinée quand la
  liste est presque pleine. Les lignes perdues sont comptées dans `lines.dropped`.
- **Paramètres invalides refusés.** Coordonnée NaN ou infinie, rayon ou demi-dimension négatif,
  `taille_case` ≤ 0, matrice non finie ou frustum non inversible : la forme n'est pas ajoutée,
  `lines.rejected` augmente et la fonction retourne `false`. (Un NaN envoyé au GPU ferait juste
  disparaître la ligne, sans aucun message : on préfère le compter.)
- **Deux listes**, chacune de `capacity` lignes : testées en profondeur (défaut ; cachées par la
  géométrie) et superposées (`set_overlay(true)` ; toujours visibles, dessinées en dernier).
- `eng_debug_lines_clear` remet les listes et les compteurs à zéro **chaque image** ; le mode
  superposé est conservé.
- **Aucune allocation** après `init` (les listes vivent dans l'arène donnée) : testé.

## Côté GPU

- **Buffer de streaming** (nouveau dans le renderer, `docs/renderer.md`) : une écriture par image dans
  un tampon de transfert permanent, copiée vers un buffer GPU dans une passe de copie qui précède
  la passe de rendu (`eng_renderer_end_frame` s'en charge). Aucune ressource créée par image :
  mesuré, 0 allocation sur 12 images.
- **Deux pipelines** (`shaders/lines.vert.hlsl`, `lines.frag.hlsl`, topologie `LINES`, sans culling) :
  *testé* (test de profondeur, **sans écriture** : les lignes ne cachent rien et se dessinent
  dans l'ordre de la liste) et *superposé* (sans profondeur). Si le renderer n'a pas de depth
  buffer, le pipeline « testé » se replie sans test.
- Sommet : `EngDebugVertex` = position (3 flottants) + couleur (4 octets RGBA), 16 octets.
  Uniform : une `mat4` proj × vue (`register(b0, space1)`).
- **À mettre en file en dernier**, après la scène : les lignes testées ne sont cachées que par ce qui
  est déjà dessiné.

## Limites (connues, voulues pour l'instant)

- **1 pixel de large** : SDL_GPU ne gère pas l'épaisseur de ligne. Des lignes épaisses demanderaient de
  les dessiner comme des rectangles (hors périmètre).
- **Opaque** : l'alpha de `EngDebugColor` n'a pas d'effet (pas de mélange).
- **Pas de texte** (étiquettes) : arrive avec l'interface.
- **Pas accessible depuis `game/`** : la bibliothèque du jeu ne lie pas le moteur (ADR 0001) ; elle
  ne peut donc pas appeler ces fonctions. Il faudra un service dans `EngServices` ou une file de
  lignes dans `game_api.h` quand le jeu en aura besoin (collisions, M3 ?).
- **Une seule écriture complète par image** pour `eng_debug_lines_gpu_draw` : il envoie l'enveloppe
  des deux listes (la zone entre la fin de la liste testée et le début de la superposée est envoyée
  aussi, jamais dessinée). Avec 4096 lignes par liste : au plus 256 Kio par image.
- Contenu d'un buffer de streaming **indéfini** hors des octets écrits depuis l'image précédente.

## Dans le lanceur

La démonstration dessine (touche **F3** pour afficher / masquer, affiché au départ) : une grille au
sol (une ligne plus claire tous les 10 cases), des sphères magenta autour des quatre cubes d'angle,
la boîte orientée jaune qui entoure le modèle glTF qui tourne (ses limites calculées par
`eng_model_parse`), et deux repères superposés (monde et modèle). Les lignes de grille disparaissent
derrière les cubes ; les repères restent visibles à travers.
