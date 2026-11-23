# 0006 — Format binaire maison des assets cuisinés

Statut : Accepté (M2-01)

## Contexte
Jusqu'à M1, le moteur lit du glTF et du PNG à chaque lancement : analyse de texte JSON, décodage d'image,
génération des mips, calcul des tangentes. C'est lent et inutile à l'exécution. Un monde ouvert (M3+) a besoin
de charger beaucoup d'assets vite, de façon prévisible, et de refuser proprement un fichier périmé ou abîmé.

## Options
1. **Continuer à lire glTF/PNG à l'exécution.** Rien à écrire, mais démarrage lent, aucun contrôle de version
   de nos propres structures, et le streaming (M3) devient impossible à budgéter.
2. **Un format générique existant** (KTX2 pour les textures, GLB pour les modèles, FlatBuffers...). Moins de
   spécification à écrire, mais il faut quand même convertir vers nos structures, et on dépend du format
   (extensions, versions, bibliothèque de plus).
3. **Un conteneur maison minimal + un contenu aligné sur nos structures** (retenu).

## Décision
- Quatre types de fichiers (`.mesh`, `.tex`, `.skel`, `.anim`) partageant **un conteneur unique** : en-tête de
  32 octets, table de sections, sections alignées sur 16 octets. Spécification : `docs/asset-formats.md`.
- **Petit-boutiste uniquement**, lecture en place sans conversion. Choix assumé : x86-64 et ARM64 le sont
  tous ; un `#error` protège contre le reste.
- **Version = un entier par type, égalité exacte exigée au chargement.** Pas de migration : un asset est dérivé
  de sa source, on le recuit. Un fichier d'une autre version est refusé avec un message qui dit quoi faire.
- Le **conteneur** est un module du moteur (`asset_format.{h,c}`), sans SDL ni GPU, qui contient à la fois la
  vérification **et** l'écriture (`eng_asset_build`) : une seule définition des règles de disposition, partagée
  par le cuiseur et le moteur. Le **contenu** de chaque type est lu par son propre module.
- Références entre assets **par chemin** ; les identifiants numériques relèvent du gestionnaire d'assets (M2-05).
- `.skel` et `.anim` sont **provisoires** jusqu'à M4 (premier usage réel).

## Conséquences
+ Chargement = lire un fichier, vérifier l'en-tête, pointer dans le tampon : pas d'analyse, pas de copie.
+ Un fichier tronqué, corrompu, d'une autre version ou d'un autre type est refusé avant tout accès (fuzz par
  modification d'octet sous ASan/UBSan dans `test_asset_format.c`).
+ Sortie du cuiseur déterministe (remplissage à zéro) : pas de recuisson inutile, diff d'assets fiable.
− Un changement de disposition invalide tous les assets cuits : acceptable tant que la cuisson est
  automatique ; à surveiller quand les assets seront nombreux et gros.
− Offsets et tailles sur 32 bits : 4 Gio par fichier. Suffisant pour un asset ; un monde se découpe en
  plusieurs fichiers (M3).
− La vérification du conteneur ne couvre pas le contenu des sections : chaque lecteur doit contrôler
  `size` contre `count × taille d'élément`.
− Pas de compression ni de signature : décision reportée à un besoin mesuré (taille de livraison, M9).
