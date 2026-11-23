# 0005 — Renderer : handles typés et file de commandes

Statut : Accepté (M1-05)

## Contexte
Le jeu ne doit pas dépendre de SDL_GPU (ADR 0001), et les ressources GPU doivent être libérées
proprement, y compris après un hot reload.

## Décision
- Ressources désignées par des handles à génération, jamais par des pointeurs : un handle périmé est
  détecté et refusé au lieu de provoquer un accès invalide.
- Le jeu enregistre des commandes (données pures) dans une file ; le moteur seul parle à SDL_GPU.
- Les pipelines possèdent leurs shaders et sont reconstruits derrière un handle stable.

## Conséquences
+ Jeu indépendant de SDL (vérifié par lint), validation centralisée, tests par lecture de pixels.
− Une copie de commande par dessin ; tampons immuables pour l'instant ; file non triée.
