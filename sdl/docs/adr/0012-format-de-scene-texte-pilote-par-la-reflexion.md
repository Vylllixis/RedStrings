# 0012 — Format de scène : texte maison piloté par la description des composants

Statut : Accepté (M2-09)

## Contexte
M2-09 demande une scène « lisible » dont l'écriture puis la relecture redonne le même résultat ; M2-10
(éditeur) écrira et rouvrira ces fichiers ; plus tard, l'équipe (ici : une personne) les relira dans les
diffs git. Les composants viennent d'un monde dont la description de champs existe déjà (ADR 0011).

## Options
1. **JSON** (cJSON, parson ou un lecteur maison). Pour : outils existants. Contre : une dépendance de
   plus (ADR 0002), ou un lecteur JSON complet à écrire et à tester ; verbeux pour des vecteurs
   (`[0,1,0]`) ; pas de commentaires ; erreurs de numéro de ligne rarement exploitables selon la
   bibliothèque.
2. **Binaire** (comme les assets cuits, ADR 0006). Pour : compact, rapide. Contre : illisible, diffs
   inutilisables, c'est précisément ce que M2-09 ne veut pas.
3. **Texte maison orienté ligne** (retenu) : `entity N parent P`, `[Composant]`, `champ = valeur`.
   Pour : lisible, commentable, diff git ligne à ligne, lecteur de ~300 lignes que je comprends
   entièrement ; les erreurs ont un numéro de ligne exact. Contre : format à documenter et à maintenir
   nous-mêmes ; pas d'outil tiers.

## Décision
Option 3. Le lecteur et l'écrivain ne connaissent AUCUN composant : ils parcourent `EngComponentDesc` et
`EngFieldDesc` (nom, type, décalage). Lecture stricte et atomique ; écriture déterministe (ordre fixe,
`%.9g` pour les float) ; version dans l'entête.

## Conséquences
- Un nouveau composant est sérialisé sans code supplémentaire dès qu'il est enregistré avec ses champs.
- L'aller-retour exact est vérifiable par un simple test sur le texte (réécriture identique).
- Changement de format = nouvelle version dans l'entête + conversion ; pas de rétrocompatibilité implicite.
- Le travail d'édition massif (milliers d'entités générées par script) reste possible : le format est
  facile à produire depuis Python.
- Si une scène devient énorme (terrain peuplé de dizaines de milliers d'objets, M3), le texte reste
  utilisable mais le chargement devra être mesuré ; la parade prévue est un format binaire cuit dérivé de
  celui-ci par l'outil de cuisson, le texte restant la source.
