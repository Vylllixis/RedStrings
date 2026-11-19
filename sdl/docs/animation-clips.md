# Clips d'animation (M4-13)

Un **clip** est un ensemble de canaux de clés (translation, rotation, échelle **locales**) qui animent les
articulations d'un squelette (`docs/skeleton.md`). Il est cuit depuis une animation glTF en un `.anim`
(format : `docs/asset-formats.md` §6) et lu par `engine/anim_clip.h`. L'évaluation d'une pose complète (M4-14, `docs/animation-pose.md`),
le skinning GPU (M4-15) et les mélanges (M4-17 et suivants) s'appuient dessus ; ce module ne fait ni l'un ni
l'autre : il fournit les données et `eng_anim_sample`.

## Cuisson

```
build/debug/bin/assetc anim perso_marche.glb perso_marche.anim [--anim N] [--skin N]
                          [--skeleton chemin/perso.skel] [--depfile f.d]
```

- `--anim N` : animation glTF (0 par défaut). `--skin N` : le **même** skin que celui passé à
  `assetc skel` (les indices d'articulation du clip sont ceux du squelette cuit, parents d'abord).
- `--skeleton` : chemin du `.skel` inscrit dans le clip ; défaut : nom de l'entrée + `.skel` (celui que
  produit `assetc skel perso.glb perso.skel`).
- Affiche : durée, nombre de canaux et de clés, nombre d'articulations recalculées et de canaux ignorés.
- `assetc info clip.anim` liste les sections.

Mixamo livre un fichier par animation (avec le squelette et souvent le maillage dedans) : on cuit le
squelette **une fois** (`assetc skel`), puis chaque animation avec `assetc anim` en désignant le même
`--skin`. Le pas à pas complet est écrit en M4-21.

## Ce que fait l'import

| Cas dans le glTF | Résultat |
|---|---|
| Canal sur une articulation du skin (translation, rotation, échelle) | gardé, rangé par (articulation, propriété), indices ramenés à ceux du squelette |
| Cible hors du skin (accessoire, maillage), propriété « weights » | **ignoré** et compté (avertissement) |
| Articulation dont des ancêtres non-os ont été repliés dans le squelette (ex. `Armature` à l'échelle 0,01 : c'est le cas de la racine d'un export Mixamo / Blender) | canaux **recalculés** (voir ci-dessous) |
| Interpolation STEP ou CUBICSPLINE | refusé, avec le message « réexporter en LINEAR » |
| Instants négatifs, non croissants, valeurs non finies, quaternion nul, deux canaux sur la même cible, plus de 1 048 576 clés par canal, aucun canal utilisable | refusé |
| Quaternions non unitaires | normalisés |

Les refus laissent les arènes dans leur état d'origine (testé par épuisement de mémoire).

### Pourquoi un recalcul ?

L'import du squelette (M4-12) replie les ancêtres non-os dans la pose de repos de la racine : la racine de
Mixamo reçoit l'échelle 0,01 de `Armature`, et sa translation de 100 (cm) devient 1 (m). Un canal de
translation brut (en cm) écraserait cette pose sans l'échelle : l'animation serait 100 fois trop grande. Pour
les articulations concernées, l'import échantillonne donc leurs canaux à l'**union de leurs instants**, compose
`ancêtres repliés × T·R·S`, puis redécompose en translation / rotation / échelle (quaternions ramenés dans le
même hémisphère d'une clé à la suivante). Les trois propriétés sont alors émises, même celles qui n'étaient
pas animées (le repos des ancêtres les modifie). Les autres articulations gardent leurs clés d'origine.
Vérifié par test : les matrices monde composées depuis les canaux recalculés égalent celles calculées
directement (`T·R·S` d'origine précédé des ancêtres), à 10⁻⁴ près, sur un ancêtre qui translate, tourne et
met à l'échelle.

**Limite connue** : une animation qui cible directement un nœud intermédiaire non-os (ex. `Armature` animé)
est ignorée (comptée dans `canaux ignorés`), car le squelette ne le représente pas.

## Échantillonnage

`eng_anim_sample(clip, canal, temps, out)` : interpolation linéaire (translation, échelle), sphérique par le
plus court chemin (rotation, renormalisée ; repli sur la moyenne normalisée quand les deux clés sont presque
identiques) ; valeur tenue hors de la plage des clés. Le clip **ne boucle pas** : le temps est ramené dans
`[0 ; duration]` par l'appelant (M4-14). `eng_anim_find_channel(clip, articulation, propriété)` cherche par
dichotomie.

## Ce qui n'a pas été vu

Comme M4-12 : aucun fichier Mixamo réel n'était disponible et rien n'est affiché à l'écran. Les tests
utilisent des GLB construits en mémoire (valeurs calculées à la main). La lecture d'un vrai export (noms
d'os `mixamorig:Hips`, `Armature` à 0,01, 30 images/s, centaines de clés) reste à confirmer sur ton fichier :
lance `assetc anim` dessus et donne-moi la sortie (durée, canaux, clés, articulations recalculées).
