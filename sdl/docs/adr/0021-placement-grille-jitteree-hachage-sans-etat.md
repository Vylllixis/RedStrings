# ADR 0021 : placement procédural par grille jitterée et hachage sans état

Statut : accepté (M3-08)

## Contexte

M3-08 demande un placement par pente, altitude et densité de la splatmap, reproductible, sans végétation sur les
pentes rocheuses. Les instances alimentent le buffer d'instancing (ADR 0020).

## Décision

Une **grille jitterée** : une cellule = un candidat, position tirée dans la cellule ; acceptation avec la probabilité
« adéquation » (pente, altitude, poids de la couche). Tous les tirages sont un **hachage pur** de (graine, cellule,
canal) : pas de générateur à état.

## Alternatives

- **Disque de Poisson (Bridson)** : meilleure répartition (distance minimale garantie), mais dépend de l'ordre et de
  l'état (le résultat d'une région change si l'on déplace une autre), coût mémoire d'une grille d'accélération. La grille
  jitterée donne une répartition acceptable pour de la végétation et des rochers.
- **Générateur pseudo-aléatoire séquentiel** : changer une règle ou une cellule décale tout ce qui suit ; incompatible
  avec une génération par chunk plus tard.
- **Rejet sur la densité cible (nombre fixe d'instances)** : la densité ne suivrait plus la surface ni la splatmap.

## Conséquences

- Déterminisme octet pour octet, indépendant de l'ordre : la génération par chunk (streaming) donnera les mêmes
  instances que la génération globale.
- Au plus une instance par cellule : densité plafonnée à `1 / cell²`, pas d'amas ; l'aspect reste légèrement
  « régulier » à grande densité.
- Pas d'interaction entre règles (chevauchement possible).
- Deux passes (compte puis remplissage) : le coût CPU double, la mémoire est exacte.
