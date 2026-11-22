# ADR 0029 : post-process en une passe — ni bloom en chaîne, ni FXAA 3.11

Date : 2026-11-20 (finition, M8-06). Écart avec le tracker, qui citait un bloom par chaîne de flous
sous-échantillonnés et le FXAA 3.11 de Lottes (ou SMAA). Hypothèse de la question Q11 (choix de
périmètre) : si Théo répond non, cet ADR passe à « Remplacé » et les deux effets sont codés.

## Contexte

Le post-process vit dans la passe de résolution HDR (`tonemap.frag.hlsl`) : bloom en 16 lectures sur
deux anneaux, FXAA simplifié (direction de l'arête, deux et quatre prises), SSAO (12 prises, moyenne
2 x 2 par les dérivées), échelle de résolution avec agrandissement bilinéaire. Le coût est mesuré
(`launcher --post-bench`, RTX 5070 Laptop en 2560 x 1600, scène de la tranche verticale) : FXAA
+0,19 ms, bloom +0,20 ms, SSAO +0,76 ms, les trois +1,01 ms ; à l'échelle 75 %, les trois ne
coûtent plus rien par rapport à l'image sans effet.

Un bloom en chaîne demande 5 à 6 textures intermédiaires (descente puis remontée), autant de passes
et leur gestion au redimensionnement et à l'échelle de résolution ; le FXAA 3.11 complet ajoute la
recherche des extrémités de l'arête (jusqu'à 12 prises de part et d'autre). Environ 2 h de travail,
pour un gain visible surtout sur les très grandes sources lumineuses (soleil, eau) et les arêtes
presque horizontales.

## Décision

On garde le post-process **en une seule passe** : pas de bloom en chaîne, pas de FXAA 3.11 ni de
SMAA dans la finition de M0 à M8. Le temps va aux critères de sortie qui manquent (M7 : objets et
ennemis dans le monde ; bugs B5, B18).

## Conséquences

- Halo court (rayon fixe en pixels) : pas de grand flou autour du soleil ou de l'eau très lumineuse.
- Arêtes presque horizontales moins bien lissées qu'avec un FXAA complet ou un TAA.
- Aucune texture intermédiaire de plus : rien à gérer au redimensionnement, mémoire inchangée.
- Le critère 2 de M8 est considéré comme rempli avec cet écart (FXAA, bloom, SSAO, échelle de
  résolution, coût par effet mesuré et documenté dans `docs/tonemapping.md`).
- Si un testeur juge le halo ou l'anticrénelage insuffisant, ces deux effets sont les premiers à
  reprendre (ils restent dans les « Limites connues » de `docs/tonemapping.md`).
