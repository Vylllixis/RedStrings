# ADR 0028 : audio — flux SDL3 et mixeur maison, sans miniaudio ; sons synthétisés

Date : 2026-11-20 (finition, M8-01). Écart avec le tracker, qui prévoyait miniaudio.

## Contexte

La première passe (M5) a sorti le son du combat par `SDL_AudioStream` (`audio_device`) et un
mixeur maison (`mixer` : 16 voix, bus SFX / musique / ambiance / interface, son 3D simplifié),
avec des effets synthétisés (`sfx`, `combat_audio`). Le tracker demandait miniaudio. La finition de
M8 ajoute l'ambiance (vent, oiseaux, grillons) et une musique : il faut trancher.

## Décision

On **garde SDL3 + le mixeur maison**, sans miniaudio :

- SDL3 est déjà la plateforme (fenêtre, entrées, GPU) : son flux audio marche partout où le jeu
  tourne (PipeWire / PulseAudio / ALSA sous Linux, WASAPI sous Windows) ; aucune dépendance de plus
  à construire sous MSVC et sous Linux, ni à licencier ;
- ce que miniaudio apporterait en plus (décodage de fichiers MP3 / FLAC / Vorbis, graphe d'effets,
  périphériques multiples) ne sert pas : **tous les sons sont synthétisés** au chargement (aucun
  fichier son dans le dépôt, aucune licence à suivre) ;
- le mixeur maison est testé sans périphérique (aucune dépendance au matériel dans les tests :
  `engine.mixer`, `engine.combat_audio`, `engine.ambience`).

L'ambiance et la musique (`engine/ambience.h`) : quatre couches synthétisées (vent, oiseaux le
jour, grillons la nuit, nappe de quatre accords sur 32 s), rejouées en boucle **sans trou** : la
lecture suivante est lancée avec un retard qui la fait commencer à l'image exacte où finit la
précédente ; chaque boucle se raccorde (fondu de 250 ms, ou fréquences à un nombre entier de
périodes sur la boucle pour la musique).

## Preuves

- `engine.ambience` : couches finies, audibles, sans écrêtage ; raccord de chaque boucle sans saut
  plus grand qu'un pas ordinaire ; gains jour / nuit ; enchaînement exact (la boucle suivante part
  pile à la fin, sur le bon bus).
- Mesure à la sortie PipeWire pendant le jeu (enregistrement de 7 s) : bande des oiseaux
  (2,7–3,9 kHz) 780 fois plus forte à midi qu'à 23 h ; bande des grillons (4,5–5,2 kHz) 72 fois plus
  forte la nuit ; bande de la musique (170–400 Hz) identique aux deux heures.

## Conséquences et limites

- Pas de fichiers son : quand le jeu aura une vraie musique enregistrée, il faudra un décodeur
  (stb_vorbis, une seule source, domaine public, est la voie la plus légère) et la lecture en flux.
- Le volume jour / nuit d'une couche change au début de sa boucle (au plus 6 à 8 s de retard).
- Le rendu sonore n'a pas été écouté par un humain (Q2, Q9).
