# Design du combat (M5-01) — v1 figée

Ce document fixe **ce que le combat doit faire** et les valeurs de départ. Les nombres sont des
points de départ à régler à la sensation (M5-14 les expose en direct) ; les **règles** et le
**vocabulaire** ne changent plus pendant M5 : toute modification passe par une note datée en bas.

## 1. Sensations visées

- **Lisible** : on comprend pourquoi on a été touché. Chaque attaque a un départ visible
  (anticipation), une zone d'impact précise, une fin où l'on est vulnérable.
- **Réactif** : une entrée n'est jamais perdue (buffer de 150 ms, M5-09) ; passé le départ d'une
  attaque, rien ne la rend « lente » sans raison visible.
- **Punitif mais juste** : une erreur coûte de l'endurance et de la posture, pas un coup mortel
  sans prévenir.
- Références de genre (sensations, pas de copie) : combats au corps à corps à engagement, où
  l'esquive et la parade ont des fenêtres précises (famille « soulslike » / action à posture).

## 2. Temps et déterminisme

- Toute la logique de combat vit sur le **pas fixe de 60 Hz** (1 tick = 1 frame = 1/60 s). Les
  durées de combat sont des **entiers de ticks**, jamais des flottants de secondes.
- Aucune source d'aléa non graine ; pas d'horloge murale ; l'ordre de résolution des combattants
  est l'ordre croissant de leur indice. Un même flux d'entrées par tick donne **le même état
  final** (hash identique, M5-18).
- Le rendu interpole ; il ne décide jamais d'un coup.

## 3. Verbes du joueur

| Verbe | Entrée | Effet |
|---|---|---|
| Attaque légère | Attack (clic / X) | chaîne de 3 coups, rapide, peu coûteuse |
| Attaque lourde | Heavy (clic droit / Y) | un coup lent, forts dégâts, forte posture ; peut terminer un combo |
| Esquive | Dodge (Ctrl / B) | roulade dans la direction visée, **invulnérable** pendant une fenêtre |
| Garde | Block maintenu (clic milieu / gâchette) | réduit les dégâts de 80 %, consomme de la posture |
| Parade | Block pressé au bon moment | les `parry_frames` premiers ticks de la garde : dégâts annulés, l'attaquant est mis en vulnérabilité |
| Verrouillage | LockOn (Tab / clic stick) | choisit la cible ; la caméra cadre joueur + cible, le mouvement passe en strafe |

## 4. Ressources

| Ressource | Départ | Règle |
|---|---|---|
| PV | 100 | 0 → état Dead au tick du coup fatal |
| Endurance | 100 | chaque action a un coût ; régénération 25/s après 1 s sans action ; à 0 aucune attaque ni esquive (la garde tient mais la posture casse plus vite) |
| Posture | 100 | les coups gardés l'abaissent ; à 0 → Stagger (vulnérable 60 ticks) puis remise à 100 ; régénère 20/s hors combat (≥ 1,5 s sans coup) |

## 5. États de gameplay (M5-02)

`Idle, Move, Attack, Dodge, Block, Parry, HitStun, Stagger, Dead`.

```
Idle ⇄ Move
Idle/Move  → Attack | Dodge | Parry          (Parry = pression de la garde)
Attack     → Idle (fin de recovery) | Attack (combo) | Dodge (annulation) | HitStun | Stagger | Dead
Dodge      → Idle | HitStun (hors invulnérabilité) | Stagger | Dead
Parry      → Block (fenêtre finie, garde tenue) | Idle (relâchée) | HitStun | Stagger | Dead
Block      → Idle (relâchée) | Attack | Dodge | HitStun | Stagger | Dead
HitStun    → Idle | Stagger | Dead
Stagger    → Idle | Dead
Dead       → (aucun)
```
Parry est l'état des `parry_frames` premiers ticks de la garde ; Block la suite.

Toute transition absente de cette table est **illégale** : elle déclenche une assertion en debug
et est refusée en release (l'état reste inchangé). Le tableau est une donnée du module, pas du
code dispersé.

## 6. Frame data par défaut (en ticks à 60 Hz)

| Action | Startup | Actif | Recovery | Dégâts | Posture | Endurance | Annulable dès |
|---|---|---|---|---|---|---|---|
| Légère 1 | 8 | 4 | 14 | 10 | 8 | 10 | recovery 6 (vers légère 2 / esquive) |
| Légère 2 | 7 | 4 | 14 | 10 | 8 | 10 | recovery 6 |
| Légère 3 | 10 | 5 | 20 | 14 | 14 | 14 | esquive seulement |
| Lourde | 20 | 5 | 24 | 25 | 30 | 25 | esquive seulement |
| Esquive | — | invulnérable ticks 3 à 14 | 24 au total | — | — | 15 | — |
| Parade | fenêtre de 8 ticks (≈ 133 ms) après la pression de garde | | | | | 0 | — |

- Dégâts multipliés par zone : tête ×1,5 ; torse ×1,0 ; membres ×0,75.
- **Hitstop** à l'impact : 3 ticks (légère), 6 (lourde) pour les deux combattants.
- Invincibilité après un coup subi : 6 ticks pour éviter les doubles touches sur une même attaque
  multi-hitbox ; une attaque ne touche **jamais deux fois la même cible** (M5-07).

## 6 bis. Garde et parade

- Garde : dégâts × 0,2 ; la posture reçue est celle du coup × 1,0 (plein) ; si la posture tombe à 0
  → Stagger.
- Parade réussie (coup reçu dans les `parry_frames` premiers ticks de la garde) : 0 dégât, 0 posture
  perdue, l'**attaquant** passe en Stagger (60 ticks) → fenêtre de contre.

## 7. Verrouillage de cible (M5-15)

Cible = ennemi vivant dans un cône de 60° devant la caméra, à ≤ 20 m, le plus proche de l'axe de
visée. Basculer de cible choisit le voisin gauche/droite dans l'ordre angulaire. Perdre la cible
(mort, distance > 25 m) libère le verrouillage.

## 8. Hors périmètre v1

Ennemis multiples et IA (M6), parades directionnelles, armes à distance, magie, ripostes
contextuelles, projection/saisie, rotation de l'arme libre. Chaque point sera rouvert avec la
revue de M5 si les testeurs le demandent.

## 9. Journal des changements

- v1 (M5-01) : version figée.
