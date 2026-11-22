# 0007 — Compression des textures : BC7 avec bc7enc

Statut : Accepté (M2-03)

## Contexte
Après M2-02, les `.tex` sont en RGBA8 avec mips : Sponza pèse 380 Mo de textures en mémoire GPU. L'objectif
de M2-03 est une réduction d'au moins 4×. Le GPU doit lire le format sans décompression côté CPU.

## Options
1. **Garder RGBA8.** Aucun travail, mais aucune réduction.
2. **BC1/BC3.** Rapides à encoder, 8 ou 16 octets par bloc, mais qualité visiblement moindre (bandes de
   couleur, mauvaise gestion des normales).
3. **BC7 partout** (retenu) : 16 octets par bloc de 16 texels, soit 1 octet par texel contre 4 : 4× exactement
   sur les niveaux dont les dimensions sont multiples de 4.
4. **BC7 + BC5 pour les normales.** BC5 est plus fidèle sur deux canaux, mais occupe aussi 16 octets par bloc :
   aucun gain de mémoire, et le shader devrait reconstruire `z`.

## Décision
- **BC7 pour tous les rôles** : `BC7_SRGB` pour les couleurs, `BC7_UNORM` pour les données et les normales.
- **Encodeur bc7enc** (richgel999, épinglé au commit `f66c2e4`), récupéré par `FetchContent`, compilé comme
  code tiers (ADR 0002) via `engine/src/third_party_bc7.c`, sans `-Werror`. Le moteur ne voit que
  `engine/bc7.h` (init, encodage d'un bloc). Limite assumée : bc7enc n'emploie que les modes 1 et 6 (opaque)
  et 5, 6, 7 (transparence), donc moins fin qu'un encodeur complet. Il est suffisant ici (PSNR moyen 43 dB sur
  Sponza) ; un encodeur plus fin se substituerait derrière `bc7.h` sans toucher au reste.
- **Le cuiseur compresse chaque niveau** de la chaîne de mips calculée en RGBA8 (lumière linéaire pour le
  sRGB). Le renderer téléverse la chaîne telle quelle (`eng_renderer_create_texture_data`).
- **Rôle strict** : le format GPU est celui du fichier ; un désaccord avec le rôle dans le matériau est une
  erreur de chargement, jamais une conversion silencieuse.
- **BC5 reporté** (raison ci-dessus). À reconsidérer si un jalon ultérieur manque de mémoire ou de précision
  sur les normales.

## Conséquences
- Le GPU doit gérer BC7 (vérifié par `SDL_GPUTextureSupportsFormat`) ; échec explicite sinon. Tous les GPU
  de bureau récents le font ; certains mobiles non (hors périmètre).
- Le premier `cmake --preset` a besoin du réseau (GitHub) pour bc7enc.
- La cuisson de Sponza prend environ 73 s sur un fil. M2-04 (cuisson au build, avec cache) en dira plus ; le
  multi-fil n'est pas fait ici car le coût est payé une fois par asset modifié.
- Le gain est de 3,9998× au pire cas (blocs de 16 octets sur les petits mips), 4,00× arrondi sur Sponza.
