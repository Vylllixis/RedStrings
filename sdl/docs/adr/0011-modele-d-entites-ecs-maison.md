# 0011 — Modèle d'entités : un ECS maison à tables de composants

Statut : Proposé (M2-07) — à confirmer ; M2-08 à M2-10 s'appuient dessus.

## Contexte
M2 introduit les entités (position, hiérarchie, rendu), un format de scène lisible (M2-09) et un éditeur en
jeu (M2-10). Ensuite viendront terrain, végétation instanciée, physique, IA : des milliers d'entités du même
genre, parcourues par type de composant. Trois contraintes propres au projet pèsent sur le choix :

1. **Le jeu est une bibliothèque qui ne lie pas le moteur** (ADR 0001) : il n'appelle que la table
   `EngServices`. Tout ce que le jeu fait avec les entités doit donc passer par une poignée de fonctions
   exportées dans cette table.
2. **Mémoire explicite** (arènes, pools à capacité fixe, zéro allocation par image : ADR 0003).
3. **Format de scène et éditeur** : il faut décrire chaque composant (champs, types) pour le sérialiser et
   l'afficher dans un inspecteur.

## Options
1. **flecs** (C99, MIT ; v4.2.0 à la date de cette note). Stockage par archétypes, requêtes, hiérarchies et
   relations, réflexion et sérialisation JSON, scheduler multithread, interface web de supervision.
   Pour : fonctionnalités déjà là, éprouvé. Contre : (a) l'API C est large (centaines de fonctions, beaucoup
   de macros) : incompatible avec une table de services courte (contrainte 1), sauf à compiler flecs dans le
   jeu, donc deux mondes ; (b) allocation interne propre, hors des arènes du moteur (contrainte 2) ; (c)
   le format de sérialisation et la réflexion sont ceux de flecs, avec des changements incompatibles d'une
   version mineure à l'autre (les notes de la 4.2 listent plusieurs renommages et suppressions d'API) ; (d)
   on apprend flecs plus que l'on n'apprend le problème.
2. **ECS maison minimal** (retenu) : entités = handles générationnels (`EngHandle`, `EngPool`), un composant
   = une table dense (ensemble creux) avec sa description de champs, la hiérarchie en composant natif. Pour :
   API étroite et stable (une dizaine de fonctions suffisent à la table de services), mémoire dans nos pools
   et arènes, réflexion faite pour nos besoins (scène, inspecteur). Contre : à écrire et à tester ; pas de
   requêtes générales, de relations ni de scheduler : on ne les aura que si on les écrit.
3. **Objets avec héritage** (un type d'entité par classe). Simple au début, mais les combinaisons de
   comportements explosent (« arbre qui brûle et qui bouge ») et ça ne se sérialise ni ne s'édite
   uniformément. Écarté.

## Décision
Option 2, avec une frontière nette pour pouvoir changer d'avis :

- `EngWorld` : capacité d'entités fixe ; une entité est un `EngHandle` (handle périmé détecté, comme partout).
- Composant : `EngComponentDesc` (nom, taille, liste de champs : nom, type, décalage). La même description
  sert à la sérialisation (M2-09) et à l'inspecteur (M2-10). Stockage : ensemble creux, données contiguës.
- Composants natifs de M2-08 : `Name`, `Transform` (position, quaternion, échelle ; locale), hiérarchie
  parent/enfants, `MeshRenderer` (asset modèle, du gestionnaire M2-05).
- Détruire une entité détruit **son sous-arbre**. Les matrices monde se calculent à la demande, parents
  avant enfants.
- Parcours : par table de composant ; une jointure parcourt la table la plus courte et teste les autres.
- Le jeu n'accède au monde que par une petite table de fonctions (création, destruction, ajout/lecture/
  retrait de composant, parcours) ajoutée à `EngServices` quand le jeu en aura besoin.

## Quand revenir sur ce choix
Adopter flecs (ou un autre ECS) en remplaçant l'implémentation derrière la même API serait justifié si l'un
de ces cas se produit, **mesuré ou constaté** : plus de ~10 types de composants requêtés ensemble dans les
systèmes (jointures coûteuses ou illisibles) ; besoin de relations générales (pas seulement parent/enfant) ;
besoin de systèmes parallèles que nos tables ne permettent pas sans réécriture. La réflexion maison, elle, est
indépendante de flecs et resterait.

## Conséquences
- Plus de code à maintenir dans le moteur (estimation : ~600 lignes avec tests pour M2-08).
- La capacité d'entités est un réglage à choisir (et à signaler quand elle est atteinte).
- Un composant ajouté plus tard = une description de champs à écrire : c'est le prix de l'inspecteur et de la
  scène sans code supplémentaire.
- Statut « Proposé » : si Théo préfère flecs, M2-08 à M2-10 changent de forme (le format de scène suivrait
  flecs) ; à décider avant de fusionner M2-08.

Sources : [flecs, README](https://github.com/SanderMertens/flecs) · [flecs, dernière version](https://github.com/SanderMertens/flecs/releases/latest).
