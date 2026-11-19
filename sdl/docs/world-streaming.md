# Monde ouvert (M7) — partition, streaming, origine flottante, persistance, LOD

Tout ce document décrit des modules **CPU testés** (`test_world_stream`, `test_world_ai`,
`test_world_lod`, `test_jobs`, benchmark `launcher --world-bench`). **Aucun n'est encore branché au
rendu** : le terrain affiché par le lanceur reste celui de M3 (1 km², un seul bloc).

## Cellules (M7-01) — `world_cell`

- Grille infinie de cellules de **256 m** ; la cellule (cx, cz) couvre x ∈ [256·cx ; 256·(cx+1)[.
- Relief : 65 × 65 hauteurs, une tous les 4 m, **bords partagés** avec les voisines (égalité au bit
  près, testée). Bruit OpenSimplex2 fractal évalué à la coordonnée monde : une cellule est une
  fonction pure de (graine, cx, cz).
- Objets : rochers (rayon 0,6 à 2 m), arbres (0,4 m), un camp sur ~1 cellule sur 4 (au centre).
- Mémoire : 17 Ko par cellule ; 64 emplacements préalloués = **1,1 Mo** fixes pour le streaming.

### Format « RSCL » v1 (petit-boutiste)

| Octets | Contenu |
|---|---|
| 0–3 | `RSCL` |
| 4–7 | version (1) |
| 8–11 / 12–15 | cx / cz (int32) |
| 16–19 | résolution (65) |
| 20–23 | nombre d'objets n (≤ 48) |
| 24 … | 65 × 65 float32, ligne par ligne (z puis x) |
| puis n × 21 | type (u8), x, z, y, cap, échelle (float32) |

Taille exacte obligatoire ; mauvaise signature, version, résolution, type d'objet ou hauteur non
finie : refusé. Un fichier `cell_X_Z.rscl` dans le dossier du monde **remplace** la génération
(cellule éditée) ; sinon la cellule est générée.

## Jobs (M7-02) — `jobs`

Pool de threads SDL, file FIFO bornée (1024), compteurs atomiques C11, dépendance « après tel
compteur ». Le thread qui attend exécute des jobs. Vérifié sous ThreadSanitizer :
`tools/scripts/tsan-jobs.sh` (jobs + streaming ; « TSAN JOBS : OK »).

## Streaming (M7-03) — `world_stream`

Carré de (2r+1)² cellules autour du joueur (r = 2 : 25 cellules, 1,3 km de côté). Chaque image :
récolte des jobs finis, libération au-delà de r + 1 (hystérésis : un va-et-vient sur une frontière
ne recharge rien), au plus 4 nouvelles demandes, les plus proches d'abord. Chargement (lecture de
fichier ou génération) **dans un job** : le thread principal ne fait que de la comptabilité.

**Écart** : le tracker prévoit l'API d'E/S asynchrone de SDL3 ; la lecture se fait ici avec
`SDL_LoadFile` **dans un thread de travail**, ce qui ne bloque pas non plus l'image.

## Uploads étalés (M7-04) — `upload_queue`

Budget d'octets par image, plus proches d'abord, une demande plus grosse que le budget passe seule,
annulation si la cellule est déchargée avant l'upload. Ordonnancement seulement : l'upload GPU réel
n'est pas branché.

## LOD du terrain (M7-05) — `terrain_lod`

Sélection CDLOD sur quadtree (8 km : 166 nœuds, 0,25 % des triangles de la pleine résolution à
1 m) ; couverture exacte ; voisins à au plus un niveau d'écart ; facteur de morph. **Pas de shader
de morph ni de rendu** : à faire.

## LOD des meshes (M7-06) — `mesh_lod`

Regroupement de sommets sur grille (sphère : 32 768 → 7 874 triangles, erreur ≤ une case) ;
sélection avec hystérésis (pas de clignotement sur un seuil). **Écart** : pas de meshoptimizer
(indisponible hors ligne) ; pas encore appelé par le cooker.

## Ombres en cascade (M7-09) — `csm`

2 à 4 cascades (découpage pratique), sphère englobante par tranche indépendante de l'orientation
(tourner la tête ne change pas la taille des texels), centre aligné sur les texels (avancer de 3 mm
ne déplace l'ombre que d'un nombre entier de texels). **Pas encore dans les shaders** : le rendu
garde l'ombre unique de M1-17.

## Origine flottante (M7-10) — `world_origin`

Positions monde en double, locales en float = monde − origine ; recentrage sur un coin de cellule
au-delà de 512 m. À 10 km : erreur de quelques micromètres contre 0,4 mm sans recentrage. Chaque
système ajoute `shift` à ses positions locales (démontré pour la simulation de combat et l'IA).

## Physique et navigation par cellule (M7-11) — `ai_area`

Fenêtre de navigation de 64 m autour du joueur, obstacles tirés des cellules prêtes, reconstruite
quand le joueur s'éloigne de 16 m ou que des cellules arrivent. Un ennemi suit le joueur sur
225 m, à travers 2 frontières de cellule et 2 recentrages d'origine, sans jamais décrocher de plus
de 6 m ni entrer dans un obstacle. **Jolt n'est pas découpé par cellule** (pas d'ajout/retrait de
corps) : à faire avec le rendu.

## Persistance (M7-12) — `world_state`

Par cellule : objets retirés (masque 64 bits), camps vaincus ; table fixe de 4096 cellules ;
sauvegarde texte ; un chargement invalide laisse l'état vide (jamais à moitié).
