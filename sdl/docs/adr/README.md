# Registre des décisions d'architecture (ADR)

Une décision structurante = un fichier `NNNN-titre-court.md`, jamais réécrit : si elle change,
on écrit un nouvel ADR qui **remplace** l'ancien (statut « Remplacé par NNNN »).

Format : Contexte → Options → Décision → Conséquences. Statuts : Proposé, Accepté, Remplacé.

| N° | Titre | Statut |
|---|---|---|
| [0001](0001-jeu-sans-lien-avec-le-moteur.md) | Le jeu est une bibliothèque qui ne lie pas le moteur | Accepté |
| [0002](0002-regles-du-code-tiers.md) | Règles d'intégration du code tiers | Accepté |
| [0003](0003-compteur-d-allocations.md) | Compteur d'allocations et portée du « zéro malloc » | Accepté |
| [0004](0004-chaine-de-compilation-des-shaders.md) | Chaîne de compilation des shaders | Accepté |
| [0005](0005-renderer-handles-et-file-de-commandes.md) | Renderer : handles typés et file de commandes | Accepté |
| [0006](0006-format-binaire-des-assets.md) | Format binaire maison des assets cuisinés | Accepté |
| [0007](0007-compression-des-textures-bc7.md) | Compression des textures : BC7 avec bc7enc | Accepté |
| [0008](0008-cuisson-incrementale-par-fichier-de-dependances.md) | Cuisson incrémentale par fichier de dépendances | Accepté |
| [0009](0009-gestionnaire-d-assets-compte-de-references.md) | Gestionnaire d'assets à comptage de références | Accepté |
| [0010](0010-rechargement-a-chaud-en-place.md) | Rechargement à chaud en place, cuisson à la volée séparée | Accepté |
| [0011](0011-modele-d-entites-ecs-maison.md) | Modèle d'entités : un ECS maison à tables de composants | Proposé |
| [0012](0012-format-de-scene-texte-pilote-par-la-reflexion.md) | Format de scène : texte maison piloté par la description des composants | Accepté |
| [0013](0013-editeur-gizmo-et-inspecteur-maison.md) | Éditeur : poignées de translation maison, inspecteur généré par la réflexion | Accepté |
| [0014](0014-champ-de-hauteurs-16-bits-et-fastnoiselite.md) | Champ de hauteurs : échantillons 16 bits, FastNoiseLite, format .hmap | Accepté |
| [0015](0015-terrain-un-modele-une-partie-par-chunk.md) | Terrain : un modèle, un tableau de sommets partagé, une partie par chunk | Accepté |
| [0016](0016-terrain-un-fragment-shader-dedie-et-triplanaire.md) | Matériau de terrain : fragment shader dédié, triplanaire, splatmap par sommet | Accepté |
| [0017](0017-culling-par-frustum-cpu-et-boites-de-chunks.md) | Culling par frustum côté CPU sur les boîtes de chunks | Accepté |
| [0018](0018-ciel-analytique-a-diffusion-simple-et-masses-d-air.md) | Ciel analytique à diffusion simple et masses d'air | Accepté |
| [0019](0019-brouillard-integrale-exacte-couleur-du-ciel.md) | Brouillard à épaisseur optique exacte, couleur du ciel à l'horizon | Accepté |
| [0020](0020-instancing-par-second-flux-de-sommets.md) | Instancing par second flux de sommets au rythme « instance » | Accepté |
| [0021](0021-placement-grille-jitteree-hachage-sans-etat.md) | Placement procédural : grille jitterée, hachage sans état | Accepté |
| [0022](0022-herbe-vent-et-fondu-au-vertex-shader.md) | Herbe : vent et fondu de distance au vertex shader, un dessin instancié | Accepté |
| [0023](0023-eau-opaque-profondeur-par-sommet.md) | Eau : opaque, profondeur par sommet, reflet de Fresnel | Accepté |
| [0024](0024-jolt-via-joltc-isole-derriere-physics-h.md) | Jolt via joltc, isolé derrière physics.h | Accepté |
| [0025](0025-ia-machine-a-etats-donnees-et-jetons.md) | IA : machine à états à données, jetons d'attaque, navigation en grille | Accepté |
