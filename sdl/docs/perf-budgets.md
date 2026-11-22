# Budgets de performance (M7-14)

## Configuration de référence

À fixer par toi (je n'ai pas accès à ta machine) : CPU, GPU, RAM, résolution. Mes mesures sont faites
sur un conteneur Linux à 2 cœurs, **sans GPU** (SwiftShader), build debug ASan : elles valent pour le
CPU du thread principal, pas pour le rendu.

## Budget par image (60 Hz = 16,7 ms ; plafond d'une image isolée : 25 ms)

| Système | Budget (ms, thread principal) | Mesuré (debug ASan, 2 cœurs) |
|---|---|---|
| Simulation de combat + IA (8 combattants) | 1,0 | non isolé (inclus dans les autotests) |
| Streaming (récolte + demandes) | 0,5 | moyenne 0,08 ms (tout le monde ouvert) |
| Fenêtre de navigation (reconstruction) | 2,0 occasionnel | pire image 10 ms (à déplacer dans un job) |
| Uploads GPU | 64 Ko / image | 57 Ko au plus |
| Rendu (CPU de soumission) | 6,0 | non mesuré sur GPU réel |
| Marge | reste | |

## Benchmark rejouable

```
build/release/bin/launcher --world-bench 10 --world-bench-report perf-world.txt
```
Traversée de 10 km à 30 m/s en arc (passages de coins de cellules), sans fenêtre. Rapport :
moyenne / p99 / pire temps CPU par image, trous sous le joueur, cellules chargées/déchargées,
mémoire, recentrages d'origine, reconstructions de navigation, uploads. Code de sortie ≠ 0 si une
image dépasse 25 ms ou s'il y a un trou. Le test `launcher.world_bench` (ctest) rejoue 2 km.

## Limites

Le budget du rendu n'est pas mesuré (pas de GPU réel ici) ; la reconstruction de la navigation est
faite sur le thread principal (pire image mesurée 10 ms en debug) ; à passer en job.
