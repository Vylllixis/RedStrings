# ADR 0002 — Règles d'intégration du code tiers

Statut : Accepté (M0-05, M0-06, M0-21) · Date : 2026-11

## Contexte

Le projet impose `-Wall -Wextra -Wpedantic -Werror` et ASan/UBSan en Debug. Le code tiers n'est
pas écrit selon ces règles ; certaines dépendances sont en C++ ou en un seul en-tête.

## Décision

1. **Versions figées** (tag ou commit) et listées dans `docs/dependencies.md`. Les sources sont
   téléchargées une seule fois (FetchContent, dossier partagé entre presets) et ne sont pas
   recontactées à chaque configuration.
2. **Options de qualité appliquées à notre code seulement**, via la cible `project_options`
   (jamais globalement) : un avertissement dans une dépendance ne casse pas la build.
3. **Bibliothèques à en-tête unique** (cgltf, stb) : leur implémentation est compilée dans
   **un seul fichier**, `engine/src/third_party_impl.c`, sans `project_options`.
4. **Dépendance C++** : autorisée si elle expose une API C et reste derrière un module du moteur ;
   le code du jeu n'en voit jamais les types. Le compilateur C++ n'est exigé que si l'option
   correspondante est activée (aujourd'hui `OW_PROFILE`).
5. **Dépendance optionnelle** = option CMake désactivée par défaut, cible `INTERFACE` vide
   quand elle est désactivée, macros qui se réduisent à rien : aucun coût ni téléchargement.
6. **Avant d'adopter une dépendance**, on vérifie ce qu'elle fait au processus (symboles qu'elle
   remplace, gestionnaires de signaux, threads) : Tracy neutralise `dlclose` sous Linux, ce qui
   a nécessité une adaptation de l'hôte de rechargement (`docs/profiling.md`).

## Conséquences

- Les dépendances se mettent à jour une par une, en changeant une ligne de version et en
  relançant les tests.
- Une dépendance qui impose un style de code intrusif (macros globales, allocation cachée) est
  à écarter ou à isoler dans un module dédié.
