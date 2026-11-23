# 0010 — Rechargement à chaud en place, cuisson à la volée séparée

Statut : Accepté (M2-06)

## Contexte
Modifier une texture dans un logiciel de dessin doit se voir dans le jeu en moins de 2 s (critère M2-06),
sans relancer le jeu. La cuisson (M2-04) transforme les sources en fichiers cuits que le jeu lit ; le
gestionnaire (M2-05) garde des handles et des pointeurs vers des payloads.

## Options
1. **Détruire et recréer l'asset** : le handle change, tous les détenteurs doivent être prévenus.
2. **Recharger en place** (retenu) : même handle, mêmes références, même adresse de payload.
3. **Surveiller les sources et recuire dans le moteur** : le moteur lierait les formats sources et `assetc`.
4. **Chaîne en deux étages indépendants** (retenu) : (a) `EngAssetCook` lance une commande quand une source
   change ; (b) le gestionnaire recharge quand un fichier cuit change. Chaque étage marche seul.

## Décision
- `eng_assets_reload` charge le nouveau payload **à côté** de l'ancien ; succès : décharge l'ancien, copie le
  nouveau à sa place ; échec : l'ancien reste. Les dépendances sont remplacées par celles du nouveau.
- Le chargeur est le même que le premier chargement (pas de chemin de code spécial) ; la demande de soi-même
  reste refusée.
- La surveillance réutilise `EngFileWatch` (fichier stable avant signal) sur les dépendances déclarées.
- La cuisson à la volée passe par un processus séparé : un `assetc` qui plante ou boucle ne touche pas le jeu.

## Conséquences
- Un payload doit être déplaçable octet par octet.
- Aucune garantie pour des dessins déjà en file : le rechargement se fait avant de construire l'image.
- Deux étages de surveillance ajoutent leur délai de stabilité (≈ 0,25–0,5 s chacun) : ~1 s au total.
- Même taille et même date de modification : changement invisible (limite de `EngFileWatch`).
