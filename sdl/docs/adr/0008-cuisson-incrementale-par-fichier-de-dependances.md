# 0008 — Cuisson incrémentale par fichier de dépendances

Statut : Accepté (M2-04)

## Contexte
Les assets sont cuits par `assetc` (M2-02, M2-03). Il faut que `cmake --build` les cuise, et seulement
quand c'est nécessaire : un modèle de plusieurs dizaines de secondes à cuire ne doit pas être recuit à
chaque build. Un glTF dépend de fichiers que CMake ne connaît pas à la configuration (`.bin`, images), et
produit un nombre de textures inconnu avant lecture.

## Options
1. **Un script qui recuit tout à chaque build, ou à la demande.** Simple, mais lent, ou oublié.
2. **Lister les dépendances à la main dans CMake.** Fragile : un glTF qui change de texture n'est pas vu.
3. **Un fichier de dépendances écrit par `assetc`** (retenu), au format Makefile lu par Ninja et make
   (`DEPFILE` de `add_custom_command`) : l'outil sait ce qu'il a lu, personne d'autre n'a à le deviner.
4. **Un cache de contenu (hash) dans `assetc`**, qui saute la cuisson si rien n'a changé. Plus robuste aux
   horodatages, mais il refait la lecture et le calcul de hash à chaque build, et double le travail de Ninja.

## Décision
- `assetc mesh|tex --depfile f.d` ; la liste d'un glTF vient du moteur (`eng_model_dependencies`), qui
  seul connaît la lecture des URI (décodage, `data:`).
- `ow_cook_assets` (`cmake/Assets.cmake`) : une règle par asset, `DEPENDS` sur la source et sur `assetc`.
- Le `.mesh` est écrit **après** ses textures et sert de sortie déclarée ; les `.tex` périmés sont supprimés.
- Pas de cache propre à `assetc` : l'horodatage de Ninja suffit.

## Conséquences
- Dépendance au générateur : Ninja ou Makefiles pour le suivi complet des `.bin` et images.
- Un `.tex` supprimé seul n'est pas détecté (supprimer le `.mesh` recuit).
- Chemins absolus obligatoires dans la ligne de commande (voir `docs/asset-formats.md` §9).
- Si les mêmes octets sont recuits, les dates des sorties changent quand même : ce qui en dépend (le
  rechargement à chaud, M2-06) verra une modification. Réécrire seulement si le contenu change serait un
  ajout possible à `assetc`.
