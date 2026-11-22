# 0015 — Terrain : un modèle, un tableau de sommets partagé, une partie par chunk

Statut : Accepté (M3-02)

## Contexte
M3-02 demande un terrain découpé en chunks de 64 m (16 × 16 pour 1 km²), « sans couture visible ». M3-04
ajoutera le culling par frustum par chunk, M3-03 un matériau de terrain, M7 le streaming.

## Options
1. **Un modèle par chunk**, sommets propres : simple à charger/décharger (streaming), mais les bords sont
   dupliqués : normales et UV à raccorder à la main (sinon couture d'éclairage), tableau de sommets × 256.
2. **Un `EngModel` unique, un tableau de sommets partagé, une partie par chunk** (retenu) : les sommets de
   bord sont les mêmes, donc la couture est impossible par construction ; réutilise `eng_model_gpu_*` sans
   nouveau code GPU ; le culling de M3-04 sera « ne dessiner que certaines parties ».
3. Maillage unique sans chunk : pas de culling possible, pas d'AABB par chunk.

## Décision
Option 2. Normales sur la grille globale. `EngTerrain.chunks[i]` porte l'AABB du chunk `i`.

## Conséquences
- Un chunk = un dessin (256 par km²) : peu coûteux, mais `eng_model_gpu_draw` dessine tout ; M3-04 demandera
  d'ajouter le dessin d'un sous-ensemble de parties.
- Un seul tampon GPU de 75 Mio à 1025² ; le streaming (M7) devra re-découper en plusieurs tampons : la
  décision sera alors revue.
- Limite `size ≤ 1673` héritée d'`EngModel` (index sur 32 bits, 16 777 216 au plus).
- La passe d'ombre limitée à 256 dessins (`max_shadow_commands`) ne peut pas encore accueillir le terrain.
