# Placement procédural (M3-08)

## Ce que c'est

Semer des instances (touffes, rochers, cailloux) sur le terrain selon des **règles** : où (pente, altitude, matériau
de la splatmap) et avec quelle densité. Le résultat est un tableau d'`EngInstance` (voir `docs/instancing.md`) : un
seul dessin le rend.

- `engine/placement.h` : `EngPlacementRule`, `eng_placement_vegetation_rule` / `eng_placement_rock_rule`,
  `eng_placement_rule_valid`, `eng_placement_suitability`, `eng_splatmap_weight_at`, `eng_placement_count`,
  `eng_placement_generate`.
- Lanceur : `--placement` (et `--placement-seed N`) : rochers sur la roche, touffes vertes sur l'herbe.

## Algorithme

Une grille de cellules de `cell_meters` mètres couvre le champ. Chaque cellule propose **un** candidat, à une
position tirée dans la cellule (jitter). Le candidat est accepté avec la probabilité **adéquation** (0 à 1) :

```
adéquation = pente × couche            (0 hors marge, hors bande d'altitude, ou si la roche dépasse rock_weight_max)
pente      = 1 jusqu'à slope_max − slope_falloff, puis 1 − smoothstep jusqu'à 0 à slope_max
couche     = poids bilinéaire de density_layer (si density_layer >= 0 ; 0 si < layer_weight_min), sinon 1
```

La densité moyenne vaut donc `adéquation / cell_meters²` par m² : elle **suit la splatmap** (deux fois moins d'herbe
sur un sol à moitié herbe = deux fois moins de touffes). Au plus une instance par cellule : la densité maximale est
`1 / cell_meters²`, et deux instances ne sont jamais dans la même cellule (pas d'amas). Pour semer plus dense, réduire
la cellule.

- **Pente** : différences centrées de `eng_heightmap_sample` à ± un espacement d'échantillon, au point exact du
  candidat (pas au sommet le plus proche).
- **Altitude** : relative à la plage du champ, bornes incluses.
- **Roche** : `rock_weight_max` < 1 interdit la règle là où le poids de roche dépasse la limite : c'est ce qui garde
  la végétation hors des pentes rocheuses.

### Déterminisme

Le tirage d'une cellule est un hachage pur de (graine, i, j, canal) : sans état et sans ordre. Mêmes entrées (champ,
splatmap, règle, origine) → mêmes instances, **octet pour octet** ; une autre graine → un autre semis. Un candidat
ne dépend pas de ses voisins : changer la règle d'une cellule ne décale pas les autres.

### Mémoire

`eng_placement_generate` compte d'abord (sans stocker), puis alloue exactement `count × 36` octets dans l'arène et
remplit. En cas d'échec (règle invalide, splatmap absente ou de mauvaise taille alors que la règle la demande, arène
trop petite) : `false`, journalisé, `*instances` = NULL, arène restituée. Aucune instance : `true`, NULL, 0.

## Règles par défaut

| | végétation | rochers |
|---|---|---|
| cellule | 2 m | 6 m |
| pente | 28° max, décroissance sur 8° | toutes |
| couche | herbe, poids ≥ 0,2 | roche, poids ≥ 0,35 |
| roche max | 0,15 | sans limite |
| échelle | 0,6 à 1,2 | 0,5 à 2,0 |
| enfoncement | 5 % de l'échelle | 15 % |

La position y est `hauteur du champ − sink × échelle` : l'instance s'enfonce un peu dans le sol et ne flotte pas sur
une pente. La teinte est un gris tiré dans `[tint_min, 255]`.

## Limites

- L'instance est posée à la hauteur du champ, **sans s'aligner sur la normale** : une touffe sur 25° reste verticale
  (voulu pour de la végétation ; pour des rochers l'enfoncement masque l'écart).
- Pas de distance minimale entre instances de règles différentes : un rocher peut chevaucher une touffe.
- La pente est évaluée à l'échelle d'un échantillon : un relief plus fin que l'espacement du champ n'est pas vu.
- Le lanceur réutilise le maillage de rocher (80 triangles) teinté en vert pour la végétation : c'est une
  démonstration du placement, pas de l'herbe (M3-09).
- Le débit sur GPU réel n'est pas mesuré ici.

## Tests

- `engine.placement` (18) : règle par défaut valide, chaque champ invalide refusé ; même graine = mêmes octets, autre
  graine = autre semis ; densité 1 = exactement une instance par cellule dans la marge (62 × 62) ; instances valides,
  dans la marge, au sol (à l'enfoncement près) avec une origine monde décalée ; zone raide vide et zone plate plantée
  (pente) ; décroissance lisse (adéquation 0,5 à mi-chemin) ; bande d'altitude respectée, bornes incluses ; densité
  proportionnelle au poids de la splatmap (rapport ≈ 0,5 pour 128/255) et coupure nette sous `layer_weight_min` ;
  aucune végétation sur la roche, rochers seulement sur la roche ; suitability nulle hors champ / marge / NaN ;
  poids bilinéaire ; `count` = `generate`, échecs sans effet sur l'arène ; règle qui ne place rien ; terrain généré
  réel : aucune touffe sur une pente rocheuse (pente ≤ max + 12°, roche ≤ limite, herbe ≥ minimum) ; bornes des
  règles (inclusives / exclusives) ; échelle, cap et teinte tirés sur toute leur plage ; marge inclusive ; espacement
  de 2 m (pente et poids de splatmap en mètres).
- `launcher.terrain` : `--placement` journalise les nombres (> 0 touffes), mêmes nombres à chaque lancement, graine
  invalide refusée.

## Mutations (vérification des tests)

`engine/src/placement.c` : 270 mutants d'opérateurs au total sur trois passes ; les tests ont été renforcés après
chaque passe (bornes des règles, plage de l'échelle / du cap / de la teinte, espacement ≠ 1 m, pente selon z,
altitude relative avec `altitude_min` ≠ 0, dernière ligne et colonne de la splatmap, validation des entrées).
Survivants restants, tous équivalents ou non observables : bornes de saturation à une valeur identique (`<` / `<=`
d'un clamp), lecture au texel du bord où l'interpolation vaut zéro, signe de la différence centrée (élevée au
carré), constantes internes du hachage (la qualité statistique est vérifiée par la répartition de l'échelle, du cap
et de la teinte), cellules supplémentaires hors du champ (rejetées par la marge), décalage d'une cellule du jitter
(même nombre et même répartition), égalités exactes de flottants (pente = pente max).
