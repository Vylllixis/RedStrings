# ADR 0023 : eau opaque, profondeur calculée par sommet, reflet de Fresnel

Statut : accepté (M3-10)

## Contexte

M3-10 (optionnel) : un plan d'eau à la couleur selon la profondeur et au reflet approché du ciel, crédible de près et
de loin.

## Décision

Un maillage de plan construit sur le champ de hauteurs, qui porte la **profondeur dans `uv.x`** (par sommet) ; un
fragment shader `water.frag` procédural (absorption exponentielle, Fresnel de Schlick, ciel = dégradé horizon / zénith
du modèle atmosphérique, deux rides, reflet du soleil) suivi du brouillard du terrain. Surface **opaque**.

## Alternatives

- **Eau transparente (alpha) avec fond visible** : demande un état de mélange dans le renderer (aujourd'hui absent) et
  un ordre de dessin ; reportable. La couleur de profondeur suggère déjà le fond.
- **Profondeur par tampon de profondeur de la scène (comme les moteurs classiques)** : demande de lire la profondeur
  d'une passe précédente (cible de rendu séparée) ; trop lourd pour ce palier.
- **Texture de profondeur** : une texture de plus à créer, à lier et à garder synchrone avec le terrain ; la profondeur
  par sommet est exacte pour un fond plan et suffisante ailleurs.
- **Cubemap du ciel pour le reflet** : l'environnement existe, mais lier trois textures au fragment shader pour un
  dégradé que l'on connaît analytiquement n'apporte pas grand-chose.

## Conséquences

- Aucune texture, aucun nouveau concept de renderer.
- La profondeur est approchée entre sommets (linéaire) ; la rive suit les cellules du champ.
- Passer plus tard à l'alpha ou à une profondeur de scène ne change pas l'API `eng_water_*` de couleur.
