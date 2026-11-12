# Ombre directionnelle (M1-17)

Le soleil de `EngLighting` projette des ombres : une **carte d'ombre** (profondeur vue du soleil)
est rendue avant l'image, puis `model.frag` la lit avec un filtrage **PCF** et des **biais réglables**.
Le calcul des matrices, des biais et du filtre existe deux fois : côté CPU (`engine/shadow.c`, testé
sans GPU) et dans le shader, qui en est la copie exacte ; les tests comparent le shader à la géométrie.

## Principe

1. **Caméra du soleil** (`eng_shadow_fit`) : une projection orthographique qui regarde la scène
   dans la direction opposée au soleil et voit la sphère englobante (`centre`, `rayon`) dans une
   texture carrée de `size` texels. La profondeur `[0, 1]` couvre l'épaisseur de la sphère
   (`0` : côté soleil). Marge de 2 texels autour de la sphère ; le centre est **aligné sur la grille des
   texels** : si la scène (donc le centre) bouge de moins d'un texel, les bords des ombres ne
   « nagent » pas. Un texel mesure `2 × rayon × size / (size − 4) / size` mètres.
2. **Passe d'ombre** (`eng_renderer_run_shadow_pass`, appelée par `eng_renderer_end_frame` avant la passe
   de l'image) : un rendu **sans cible couleur** dans une texture D32F (`eng_renderer_create_shadow_map`),
   profondeur effacée à 1. Les pipelines sont `depth_only` (`shadow.vert` / `shadow.frag`, ce dernier
   vide), sans élimination des faces : les modèles ouverts projettent leur ombre des deux côtés.
3. **Lecture** (`model.frag`, texture et sampler à l'emplacement 8) : le point est décalé, projeté dans
   la carte, comparé, filtré ; le résultat, `1 − force × (1 − visibilité)`, multiplie **la lumière
   directe du soleil seulement** (`direct`). L'éclairage par image (`docs/ibl.md`) et l'émissif ne
   sont pas ombrés : ce sont des lumières sans direction.

## Biais

Une carte d'ombre compare la profondeur d'un point avec celle de la surface la plus proche lue dans
un texel : sur une surface éclairée, la profondeur du point et celle du texel diffèrent à cause de la
discrétisation, et la surface se met à l'ombre d'elle-même (**acné**). Retirer trop de profondeur
décolle l'ombre de son objet (**peter-panning**). Les trois biais de `EngShadowParams` sont en
**texels** (indépendants de la résolution et de la taille de la zone couverte) :

| Réglage | Effet | Défaut |
|---|---|---|
| `constant_bias` | retire `n` texels de profondeur (1 texel = `1/size`) | 1,0 |
| `slope_bias` | retire en plus `slope × (rayon_PCF + 1) × min(tan θ, 8)` texels, θ = angle entre la normale et le soleil | 1,0 |
| `normal_bias` | décale le point lu le long de la normale géométrique de `normal × sin θ` texels (en mètres : × `texel_world`) | 1,0 |
| `pcf_radius` | rayon du filtre en texels, 0 à 3 | 1 |
| `strength` | part de la lumière directe retirée dans l'ombre | 1,0 |

Pourquoi ces formes (déduction, vérifiée par les tests ci-dessous, pas une propriété démontrée) :
sur un plan d'inclinaison θ, la profondeur varie de `tan θ` texels de profondeur par texel de la carte,
et un noyau de rayon `r` lit des texels jusqu'à `r + 1` texels du point : il faut donc retirer
`(r + 1) tan θ` pour qu'aucune prise du noyau ne voie le plan au-dessus du point. Le décalage normal
corrige les surfaces courbes, que le biais de pente suit mal ; il est proportionnel à `sin θ` pour ne
pas déplacer une surface vue de face. `tan θ` est plafonné à 8 (82,9°) : au-delà, le biais
deviendrait énorme pour une surface presque parallèle au soleil, qui reçoit de toute façon presque
rien (`N·L` tend vers 0).

Le sens de chaque réglage est vérifié : un biais nul fait apparaître l'acné
(`shadow_acne.without_any_bias_the_same_ground_shows_acne`) ; un biais énorme décolle l'ombre
(`shadow_peter_panning.huge_biases_do_detach_the_shadow_so_the_measure_is_meaningful`).

## Filtrage PCF

Noyau de `(2r + 1)²` prises, **chacune** une interpolation bilinéaire de quatre comparaisons
`profondeur_de_référence ≤ profondeur_stockée` (1 : lumière). Les comparaisons sont faites à la main
sur des lectures au plus proche voisin (`SampleLevel`, sampler `NEAREST` + bords prolongés) : pas de
comparaison matérielle (`SamplerComparisonState`) pour que le CPU et le GPU fassent exactement le
même calcul, ligne à ligne.

Largeur de la pénombre mesurée au bord d'une ombre droite (de 10 % à 90 % de la lumière,
`the_penumbra_widens_with_the_pcf_radius`) : **1,05 ; 3,14 ; 5,24 ; 7,29 texels** pour les rayons 0 à 3
(environ 2,1 texels par unité de rayon). Coût (déduit, **non mesuré** : SwiftShader n'est pas un GPU) :
`4 (2r + 1)²` lectures par pixel éclairé par le soleil, soit 4, 36, 100 et 196 pour r = 0 à 3 ; le
défaut r = 1 est un compromis, pas une mesure.

## Utilisation

```c
EngRendererDesc desc = {.depth_buffer = true, .hdr = true, .max_shadow_commands = 256};
/* ... */
EngTextureHandle map = eng_renderer_create_shadow_map(&renderer, 2048);   /* NULL si D32F refusé */
EngModelGpu ground, model;   /* eng_model_gpu_create */
EngPipelineDesc d; eng_model_shadow_pipeline_desc(&d);
EngPipelineHandle caster = eng_renderer_create_pipeline(&renderer, &d);

/* à chaque image */
EngShadowMap sun;
eng_shadow_fit(&sun, lighting.sun_direction, centre, rayon, 2048);
EngRenderQueue *shadow_queue = eng_renderer_begin_shadow_queue(&renderer);
eng_model_gpu_draw_depth(&model, shadow_queue, caster, model_matrix, sun.view_proj);
eng_renderer_set_shadow_map(&renderer, map);          /* end_frame rend la carte */

EngModelShadow receive = {.map = map, .shadow = sun, .params = params};
eng_model_gpu_draw_shadowed(&model, queue, pipeline, model_matrix, view_proj, &lighting, &receive);
```

`eng_model_gpu_draw` (sans ombre) reste valide : il lie une texture blanche à l'emplacement 8 et
désactive l'ombre dans les uniforms. Un dessin qui porte `shadow_map` exige les huit textures de
l'éclairage par image et un sampler ; sinon le renderer le rejette et le compte (`rejected_commands`).

## Lanceur

Le sol (une boîte de 56 m de côté dont le dessus est à y = −0,5, la base des cubes) reçoit les
ombres des 48 cubes de la grille et du modèle central ; la carte fait 2048 × 2048 et couvre la
sphère de rayon 42 m autour de l'origine (un texel : 4,1 cm). Le modèle reçoit aussi sa propre
ombre. Les cubes texturés (`textured.*`) ne sont pas éclairés : ils projettent une ombre mais n'en
reçoivent pas. Panneau de debug (F1) : `ombre : active`, biais constant, biais de pente, biais
normal, rayon du filtre, force. Chaque image ouvre une passe d'ombre quand l'ombre est active ; le
lanceur journalise leur nombre à l'arrêt (test `launcher.shadow`).

## Vérification

- `tests/test_shadow.c` (CPU, 24 tests) : conteneur de la sphère avec 1,5 texel de marge,
  orientation (haut de l'image, profondeur 0 côté soleil), taille des texels, alignement,
  conversion des biais en texels, plafond de pente, décalage normal, rampes de visibilité et
  largeur du PCF, égalité (lumière), hors carte (lumière), prises au bord, force. **Acné** sur un sol
  éclairé, 8 hauteurs de soleil de 5° à 85° × rayons 0 à 3 (le rayon 3 est testé à partir de 8°) ;
  **garde-fou** : sans biais le sol s'ombre (moins de 90 % éclairé) ; **peter-panning** : écart
  inférieur à 4 texels aux azimuts 0, 30, 90, 200° et supérieur à 10 avec un biais énorme.
- `tests/test_shadow_gpu.c` (SwiftShader, 13 tests) : une vraie passe d'ombre et le vrai shader, vus
  du dessus, sur un sol et une boîte de 2 m. Référence **géométrique indépendante** du shader (le
  rayon du point vers le soleil traverse-t-il la boîte ?), comparée sur les pixels « certains » (ceux
  dont tous les voisins à 3 texels répondent pareil) : **16 positions du soleil** (hauteurs 25, 40, 55,
  70° × azimuts 0, 37, 130, 250°), **zéro** pixel éclairé noirci (acné), **zéro** ombre manquante.
  Aussi : ombre dessinée là où la matrice de modèle met la boîte, ombre qui commence au pied de la
  boîte (à 2 texels près des deux bouts), pixels éclairés **identiques à 0 niveau près** avec et
  sans carte, toit de la boîte sans acné (20°, 45°, 80°), force, pénombre, sol hors de la carte,
  dessins rejetés si le couple carte/sampler ou les huit textures sont incohérents.
  **Parité exacte** (`the_shader_equals_the_cpu_reference_on_a_known_depth_map`) : une carte de
  profondeur synthétique dont chaque texel est connu (sol + trois boîtes de hauteurs différentes, dont
  deux dépassent le bord de la carte), vue du dessus ; le shader est comparé pixel par pixel à
  `eng_shadow_receiver` + `eng_shadow_visibility` + `eng_shadow_factor` pour 10 jeux de réglages (chaque
  terme isolé, défauts, rayons 0 à 3, force 0,6, soleil rasant à 6°) : écart maximal mesuré 2,1 niveaux
  sur 255 (tolérance 3). C'est ce test, pas la comparaison géométrique (qui tolère plusieurs
  texels), qui épingle les biais et le filtre du shader.
- `tests/test_model.c` : uniforms d'ombre (matrice, biais, rayon, force copiés dans le bon champ,
  réglages invalides corrigés dans la copie), `eng_model_gpu_draw_depth`, boîte procédurale
  (`eng_model_make_box`).

## Limites

- **Une seule carte statique** qui couvre une sphère fixe : pas de cascades (M7), pas de zone qui suit
  la caméra. Hors de la sphère : lumière (jamais d'ombre à bords prolongés). Le biais de pente fait
  entrer dans la carte des points qui la dépassent de `(constant + slope × (r + 1) × tan θ) / size` de la
  profondeur : à 7° de hauteur, avec les défauts, l'ombre déborde ainsi d'environ 1 m sur la limite
  de la carte (mesuré : jusqu'à x = 9,15 m pour une limite à 8,1 m, `ground_beyond_the_map_stays_lit`).
- **Rayon de filtre 3 à moins de 8° de hauteur** : les biais par défaut ne suffisent plus (mesuré à 5° :
  un pixel éclairé sur 10 noirci). Augmenter le biais normal, ou réduire le rayon. Les rayons 0 à 2
  sont sans acné dès 5° (CPU) ; sous 5° la surface ne reçoit presque plus de lumière.
- Le biais est réglé sur des plans et sur des surfaces lisses (boîte, sol, tore, sphère) ; un
  maillage très détaillé ou une surface fine peut demander de les ajuster (curseurs). Les biais
  décollent légèrement les très petits objets (moins que le biais).
- L'ombre multiplie le soleil **directionnel** seulement : le disque de soleil d'un ciel `.hdr`
  (`docs/ibl.md`) et l'éclairage par image ne sont pas ombrés, donc une surface à l'ombre garde le
  ciel. Pas d'ombre de contact (SSAO, M7).
- Pas de projeteur « alpha » : les cubes texturés projettent leur boîte, pas la transparence du texel.
- Les modèles reçoivent l'ombre calculée sur leur **normale de sommet** ; la normal map ne modifie
  pas le biais.
- Les ombres sont **instables en rotation** : l'alignement du centre sur la grille évite le
  scintillement quand la scène **se déplace**, pas quand le soleil **tourne** (les texels se
  réalignent à chaque changement de direction).
- Le coût GPU n'a pas été mesuré (pas de GPU dans mon environnement de test) : l'objectif « 60 FPS avec
  ombres » de la sortie de M1 reste à vérifier sur ta machine (affichage des FPS avec F1 ; compare avec
  `ombre : active` coché et décoché, puis avec le rayon du filtre à 0 et à 3).

## Mutants (tests de tests)

Campagne de 70 mutations manuelles (CPU, shader, model_gpu, renderer, boîte procédurale) : chacune est
appliquée seule, tout est recompilé, les tests rejouent. Survivants **conservés et assumés** :

- `vis_oob_d_lo`, `recv_cos_floor`, `fs_radius_cap`, `fs_perturbed_n`, `mg_sampler_nearest`,
  `mg_sampler_mag` : **équivalents ou redondants** (le plafond de pente rend le plancher du cosinus
  inobservable ; le rayon est déjà plafonné côté CPU avant l'envoi ; la lecture à `SampleLevel` tombe
  sur des centres de texels, où NEAREST et LINEAR donnent la même valeur ; aucun modèle de test n'a de
  normal map).
- `fs_texel_center` : lecture sur le bord du texel au lieu de son centre ; le résultat dépend de
  l'arrondi du GPU sur la frontière, aucun test ne peut l'épingler de façon stable.
- `r_begin_null`, `r_depth_fmt`, `r_map_unbind_pipeline`, `box_tangent_w` : **non couverts** (retour NULL
  d'une file d'ombre désactivée ; format de profondeur du renderer égal à D32F dans SwiftShader ;
  remise à zéro de la liaison de la carte au changement de pipeline ; signe de la tangente de la boîte,
  qui n'a pas de normal map). À revoir si l'un de ces chemins devient critique.
