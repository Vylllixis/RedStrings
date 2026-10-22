# Politique de sécurité

RedStrings est un jeu local, sans réseau, développé dans un cadre étudiant. Il ne traite aucune donnée sensible et n'ouvre aucune connexion.

## Versions concernées

Seule la dernière version publiée sur `main` (dernier tag `vX.Y.Z`) est suivie. Les versions antérieures ne reçoivent pas de correctif.

## Ce qui est considéré comme une vulnérabilité

Tout défaut qui permet, à partir d'une entrée que le jeu lit, de provoquer un comportement mémoire indéfini ou l'exécution de code, par exemple :

- un fichier de sauvegarde `personnages.txt` construit pour faire déborder un tampon ou planter le chargement ;
- une saisie (nom, nombre) qui corrompt la mémoire ;
- un chemin de ressource (polices, images) qui sort du dossier prévu.

Un simple plantage sur une entrée absurde est un bug ordinaire : une [issue](../../issues/new/choose) suffit.

## Comment signaler

Utilise le **signalement privé de vulnérabilité de GitHub** : onglet *Security* du dépôt, puis *Report a vulnerability*. Si ce bouton n'est pas disponible, ouvre une issue **sans aucun détail technique** en demandant un canal privé : le détail sera échangé ensuite hors de l'issue.

Indique la version, le système, et comment reproduire. Il n'y a pas de délai de réponse garanti : c'est un projet personnel, maintenu quand son auteur est disponible.
