# 0009 — Gestionnaire d'assets à comptage de références

Statut : Accepté (M2-05)

## Contexte
Le jeu demandera le même modèle ou la même texture depuis plusieurs endroits (niveau, entités, éditeur).
Il faut qu'un asset ne soit chargé qu'une fois, soit libéré sans fuite, et qu'une référence vers un asset
déchargé soit détectée plutôt que de lire de la mémoire libérée. Le rechargement à chaud (M2-06) doit
pouvoir retrouver les fichiers dont dépend un asset.

## Options
1. **Cache sans comptage** (chargé une fois, libéré à la fin du niveau). Simple, mais tout reste en
   mémoire et une fuite de GPU passe inaperçue.
2. **Pointeurs partagés à comptage dans chaque type.** Duplique la logique dans chaque type d'asset.
3. **Un gestionnaire générique, handles générationnels et comptage de références** (retenu), les types
   n'apportent que `load` et `unload`.
4. **Chargement asynchrone dès maintenant.** Utile plus tard ; compliquerait la durée de vie des handles
   avant d'en avoir le besoin mesuré.

## Décision
- `EngAssets` = un `EngPool` de handles (capacité fixe, génération) + un `EngMap` (hachage de « type:chemin »,
  vérifié par comparaison du chemin : une collision est refusée, pas confondue).
- Un type décrit `payload_size`, `load`, `unload` ; le gestionnaire alloue et libère le payload, qui garde
  une **adresse stable** (le rechargement à chaud pourra le remplir sur place).
- Un échec de chargement n'est pas mis en cache ; une demande de soi-même pendant son chargement est refusée.
- `eng_assets_shutdown` rend le nombre d'assets non rendus, comme `eng_renderer_shutdown` pour le GPU.
- Les dépendances sont déclarées par le chargeur (`eng_asset_load_dependency`) et consultables par handle.

## Conséquences
- Le lanceur lit `assets/cooked/models/demo.mesh` : plus aucun parsing glTF à l'exécution.
- Chargement synchrone : un gros modèle bloque l'image qui le demande.
- La capacité est fixe : un niveau qui dépasse `ASSET_CAPACITY` voit ses chargements refusés (journalisé).
- La clé inclut le chemin tel quel : deux écritures du même fichier font deux assets (documenté).
