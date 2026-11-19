# Herbe animée (M3-09)

## Ce que c'est

Un champ d'herbe dense en UN dessin : des brins instanciés (`docs/instancing.md`), placés par la règle d'herbe
(`docs/placement.md`), dont le vent et la disparition à distance sont calculés dans le vertex shader.

- `engine/grass.h` : `EngGrassStyle`, `eng_grass_style_default/valid`, `eng_grass_pack`, `eng_grass_fade`,
  `eng_grass_vertex_world` (référence CPU du shader), `eng_grass_blade_build`, `eng_grass_default_material`,
  `eng_grass_pipeline_desc`.
- `shaders/grass.vert.hlsl` : mêmes sorties que `instanced.vert` ; le fragment shader est `model.frag`, inchangé.
- `eng_model_gpu_draw_ex` : `EngModelDrawOptions.vertex_tail` / `vertex_tail_floats` (≤ 16, multiple de 4) :
  flottants ajoutés après `{view_proj, color}` dans le bloc de vertex d'un dessin instancié.
- `eng_placement_grass_rule` (cellule de 1 m, herbe ≥ 0,3, roche ≤ 0,1, pente ≤ 30°) ; lanceur : `--grass`.

## Le brin

Un ruban de `segments` segments (1 à 8), haut de 1 m, large de `width` à la base, pointu ; **deux faces** (normales
+z et −z) pour être vu des deux côtés sans variante de culling : `2 × (2n + 1)` sommets, `2 × (2n − 1)` triangles
(n = 3 : 14 sommets, 10 triangles). L'échelle de l'instance donne la hauteur réelle.

## Vent et fondu (formules)

```
d       = distance du brin (sa base) à la caméra
échelle = échelle de l'instance × (1 − smoothstep(fade_start, fade_end, d))
onde    = 0,5 + 0,5 sin(phase + k · <direction, position de l'instance>)      dans [0, 1]
décalage de la pointe = direction × force × échelle × y² × onde                (y : hauteur dans le brin)
```

- La base (y = 0) ne bouge jamais ; la pointe bouge le plus (y²). L'onde est une vague qui traverse le champ le
  long du vent (`wave_scale` = k en rad/m) ; `phase = 2 pi f t`, ramenée dans [0, 2 pi[ côté CPU pour garder la
  précision après des heures de jeu.
- À `fade_end` l'échelle vaut 0 : tous les sommets du brin se confondent avec sa base (triangles dégénérés,
  éliminés avant la rastérisation). Entre `fade_start` et `fade_end` le brin rapetisse sans à-coup.
- Défauts : vent 0,25 m vers +x, 0,35 Hz, k = 0,4 rad/m ; fondu de 60 à 90 m.

## Mesures et limites

- `engine.grass_gpu` : pixel pour pixel (≤ 3 niveaux sur 255), le rendu du vertex shader égale celui d'un maillage
  ordinaire dont chaque sommet a été déplacé par `eng_grass_vertex_world` ; 20 000 brins = 1 dessin, 20 000
  instances au compteur du renderer. Le temps est écrit sur la sortie d'erreur, **non exigé** : SwiftShader est un
  rasteriseur logiciel.
- **Le critère « champ dense à 60 FPS » n'est PAS vérifié ici** : aucun GPU réel. À mesurer sur ta machine avec
  `launcher --terrain … --grass` (le journal d'arrêt donne le nombre de brins ; Tracy donne le temps GPU).
- Le fondu rapetisse le brin mais ne supprime pas l'instance : le GPU traite encore chaque brin lointain
  (sommets seulement). Au-delà de plusieurs centaines de milliers de brins, le culling par chunk (CPU) ou GPU sera
  nécessaire (M7).
- Pas d'ombre portée ni de brouillard sur l'herbe (comme les rochers) ; pas de transparence (pointe pleine).
- L'éclairage utilise la normale du ruban (±z tourné par le cap) : l'herbe est éclairée comme un plan vertical,
  pas comme une touffe arrondie.

## Tests

- `engine.grass` (CPU, 11) : validité du style, disposition des uniforms, fondu, géométrie du brin (fermé, deux
  faces, anti-horaire par face, 1 m de haut, se rétrécit), rejets, matériau, vent (base immobile, pointe dans le
  sens du vent, bornée), onde qui se propage et se répète, brin lointain qui s'effondre, rotation du cap.
- `engine.grass_gpu` (4) : vent = référence CPU ; fondu = référence CPU et disparition au-delà de `fade_end` ;
  champ dense en un dessin ; queue de vertex trop grande ou ignorée sans effet.
- `launcher.terrain` : `--grass` journalise le nombre de brins et « 1 dessin par image ».

## Mutations (vérification des tests)

150 mutants de `grass.c` (opérateurs inversés), tests CPU seuls : 117 tués au premier passage. Les survivants
révélaient deux vrais trous, comblés : brin placé hors de l'origine (distance de fondu en 3D, signe de la position)
et signe de l'inclinaison dans le sens du vent par composante, plus la boîte englobante et les uv du brin. Restent,
tous équivalents ou inobservables : seuils d'égalité (`> 1e-12`, bornes du clamp), termes du quaternion
nuls pour une rotation de lacet seul (lignes de `rotate_quat`), `<` → `<=` des boucles de remplissage (un tour de
plus écrit dans l'arène au-delà du compte, invisible aux tests), `>` à l'intérieur d'un message de journal.
