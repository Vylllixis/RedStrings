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
