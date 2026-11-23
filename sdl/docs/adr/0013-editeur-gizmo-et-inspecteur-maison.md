# 0013 — Éditeur : poignées de translation maison, inspecteur généré par la réflexion

Statut : Accepté (M2-10)

## Contexte
M2-10 demande de sélectionner un objet à la souris, le déplacer et sauvegarder la scène sans quitter le
jeu. Le plan évoquait ImGuizmo pour les poignées. Le moteur utilise déjà Dear ImGui via cimgui (API C,
ADR 0004) et un monde dont les composants se décrivent eux-mêmes (ADR 0011).

## Options
1. **ImGuizmo** : poignées complètes (translation, rotation, échelle). Contre : bibliothèque C++ qui
   n'a pas de binding cimgui entretenu pour la version d'ImGui utilisée ; le moteur est en C11 et
   compile sans exception C++ hors ImGui ; ajouter un second pont C++ pour trois flèches est
   disproportionné, et son comportement (inversion d'axes, snapping) est difficile à tester sans
   fenêtre.
2. **Poignées maison** (retenu) : trois flèches dessinées avec les lignes de debug (M1-13), sélection du
   plus proche axe par distance rayon–droite, déplacement contraint sur la droite de l'axe. Pour : ~250
   lignes de C pur, toute la géométrie est testable analytiquement sans GPU (rayon, boîte, droite).
   Contre : translation seulement ; rotation et échelle passent par l'inspecteur.
3. **Pas de poignées** (inspecteur seul). Contre : l'AC est « déplacer un objet », pas « saisir trois
   nombres ».

## Décision
Option 2. L'inspecteur n'a pas de code par composant : il parcourt `EngComponentDesc`/`EngFieldDesc`
(un widget par type de champ, quaternion affiché en degrés d'Euler XYZ). Tout nouveau composant
enregistré est donc éditable sans code supplémentaire, comme il est sérialisable (ADR 0012).

## Conséquences
- Le déplacement est calculé en repère MONDE puis converti dans le repère du parent : un objet enfant
  d'un parent tourné/mis à l'échelle suit la souris correctement.
- Le pas d'un glisser ne dépend que du rayon courant et du rayon du début du glisser (pas
  d'accumulation d'erreur) ; un rayon quasi parallèle à l'axe (sin < 0,05) est refusé plutôt que de
  projeter l'objet à l'infini.
- Limites assumées : pas d'annulation (undo), pas de rotation/échelle par poignées, pas de
  duplication, pas de réaffectation de parent à la souris, hiérarchie limitée à 64 lignes affichées,
  quaternions édités via Euler (l'affichage peut passer d'une représentation équivalente à une autre
  près du blocage de cardan).
- Si une édition plus riche devient nécessaire (M5+ : niveaux), ImGuizmo ou une commande « undo » se
  rajoutent par-dessus l'API `eng_editor_*` sans la changer.
