# ADR 0022 : herbe animée par vertex shader, brin à deux faces, fondu par l'échelle

Statut : accepté (M3-09)

## Contexte

M3-09 demande un champ d'herbe dense à 60 FPS avec du vent et une disparition progressive à distance, sur l'instancing
de M3-07.

## Décision

Un seul maillage de brin à **deux faces** (normales opposées), instancié ; le **vent** et le **fondu** sont faits
dans un vertex shader dédié `grass.vert` (mêmes sorties que `instanced.vert`, donc `model.frag` réutilisé). Le fondu
règle l'**échelle** du brin (smoothstep sur la distance) : à zéro, les triangles dégénèrent. Les réglages passent par
une « queue » de uniforms de vertex (`EngModelDrawOptions.vertex_tail`), comme la queue de fragment du brouillard.

## Alternatives

- **Culling par distance côté CPU** : réécrire le buffer d'instances à chaque image : trop coûteux pour des centaines
  de milliers de brins. Reporté à M7 (culling par chunk / GPU).
- **Fondu par alpha** : le renderer n'a pas de mélange alpha ; et l'herbe en alpha pose des problèmes de tri.
- **Pipeline sans culling pour un brin à une face** : demande une variante de pipeline (état de rastérisation) ; deux
  faces coûtent le double de triangles mais aucun changement de renderer.
- **Vent par texture de bruit** : plus riche (rafales), mais une texture et un échantillonnage de vertex ; une onde
  sinusoïdale le long du vent suffit à « lire » du vent et se vérifie exactement par une référence CPU.

## Conséquences

- La référence CPU (`eng_grass_vertex_world`) permet un test pixel pour pixel du shader.
- Le coût du fondu n'est pas nul : les brins lointains passent encore au vertex shader.
- Pas d'ombre ni de brouillard sur l'herbe pour l'instant.
