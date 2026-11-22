# Dépendances

Récupérées par CMake (FetchContent) dans `third_party/_deps/`, versions figées.

| Bibliothèque | Version | Commit | Usage |
|---|---|---|---|
| SDL3 | release-3.4.18 | 829a65d769d935c4852f8159e964312c0957260a | fenêtre, entrées, audio, GPU (liée en statique) |
| cglm | v0.9.7 | 888aca08d5bdf38e873ec23d6cac42d7f23a669c | maths 3D (en-têtes seuls) |
| cgltf | v1.15 | 360db1a95480fe102ae9c69b27c5d101167ff5ba | chargement glTF |
| stb | (sans tag) | 003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5 | stb_image, stb_truetype |

## Mettre à jour une dépendance
Changer `GIT_TAG` dans `third_party/CMakeLists.txt`, supprimer `third_party/_deps/<nom>-src`, reconfigurer, puis mettre ce tableau à jour.

## Hors ligne
Après le premier téléchargement : `cmake --preset debug -DFETCHCONTENT_FULLY_DISCONNECTED=ON`.
