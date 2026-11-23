# ADR 0017 : culling par frustum côté CPU, sur les boîtes des chunks

Statut : accepté (M3-04)

## Contexte

Le terrain est dessiné par chunks (ADR 0015) : un dessin par chunk, avec une boîte alignée sur les axes
chacun. À 1025² échantillons il y a 256 chunks ; la caméra n'en voit qu'une fraction. Il faut ne soumettre
que les chunks visibles, et le prouver (le nombre baisse quand on regarde le ciel).

## Décision

- Pyramide de vision = six plans extraits de la matrice vue-projection (Gribb–Hartmann), normalisés,
  stockés `(n, d)`. Code dans `engine/frustum.c`, sans dépendance au renderer.
- Test boîte/pyramide : coin le plus avancé par plan (conservatif). Pas de test exact de l'intersection
  boîte/pyramide (les faux positifs près des arêtes sont rares et ne coûtent qu'un dessin).
- Pyramide invalide (matrice non finie ou dégénérée) : rejette tout.
- Le lanceur conserve les boîtes des chunks et la liste visible est passée à `eng_terrain_gpu_draw`.

## Alternatives

- **Culling GPU (compute / indirect)** : inutile à 256 boîtes, et demande des fonctionnalités que SDL_GPU
  n'expose pas partout. À reconsidérer avec M3-07 (50 000 instances).
- **Quadtree de chunks** : gain nul à 676 boîtes maximum.
- **Sphères englobantes** : test plus simple mais bien moins serré sur un terrain plat et large (une boîte
  de 64 × 2 × 64 m a une sphère de 45 m de rayon).

## Conséquences

- Le test est vérifié contre un test indépendant par coins en espace de découpage (24 000 boîtes).
- Le culling ne règle pas l'occlusion ; le coût CPU est de 6 plans par chunk et par image.
