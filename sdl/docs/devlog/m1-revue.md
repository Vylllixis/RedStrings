# Revue de jalon M1 — critères de sortie

Légende : **Prouvé** = vérifié par une commande rejouable (ici : Linux, SwiftShader, pas de GPU réel) ;
**Partiel** = prouvé pour une partie du critère, la partie manquante est dite ; **À faire** = à vérifier
sur ta machine (RTX 5070).

Vérification mécanique : `tools/scripts/m1-exit-check.sh [preset]` (critères 1 et 4 + suite complète).

## Critères de sortie du jalon

| # | Critère | Preuve | État |
|---|---|---|---|
| 1 | Le code de jeu n'appelle aucune fonction SDL_GPU | `tools/scripts/m1-exit-check.sh` : aucun symbole `SDL_GPU*` ni include `SDL_gpu` dans `game/` et `launcher/` ; `game/` n'inclut que `engine/{assert,log,game_api}.h` (les en-têtes de la table de services, ADR 0001) | Prouvé — portée ci-dessous |
| 2 | DamagedHelmet rendu de façon cohérente avec un viewer de référence | direct : three.js r170, 4 vues, 99,5 – 99,6 % des pixels à ±4/255, PSNR 45 – 52 dB (`docs/pbr.md`) ; éclairage par image : écart de 3 à 25 % de luminance (`docs/ibl.md`) | **Partiel** |
| 3 | 60 FPS sur la scène de test avec ombres | aucune mesure sur GPU réel (`docs/shadows.md`, limites) ; FPS affichés avec F1 | **À faire** |
| 4 | Hot reload des shaders fonctionnel | `engine.shader_hot` (9 tests) : modifier la source change ce qui est dessiné ; une erreur de compilation garde l'ancien shader ; includes résolus ; repli sur le shader hors ligne | Prouvé (Linux, SwiftShader) ; interactif **À faire** |

### Ce que disent (et ne disent pas) les preuves

**1. Portée.** « Le code de jeu » = `game/` (aucun symbole GPU ; seuls includes du moteur : assertions, journal, table de services) et `launcher/`
(aucun appel `SDL_GPU*`). Deux nuances :

- le lanceur lit `gpu.device` (champ de `EngGpu`, de type `SDL_GPUDevice *`) pour le passer à
  `eng_ui_init` : c'est un pointeur transmis, pas un appel, mais le type SDL_GPU traverse la frontière ;
- dans le moteur, `renderer.c`, `shader*.c`, `debug_lines.c` et `ui.c` appellent SDL_GPU directement : le
  renderer est la frontière **pour le jeu**, pas encore une couche exclusive (à resserrer si un second
  backend devient un objectif).

**2. Partiel, volontairement.** Ce qui est établi : l'éclairage direct (Cook-Torrance, normal map,
textures, émissif) concorde avec three.js **une fois deux écarts de three.js à la spécification
alignés** (diffus pondéré par `1 − F`, rugosité relevée) ; sans cet alignement : 60 – 85 % des pixels à
±4. Ce qui ne l'est pas : l'éclairage par image à l'identique. Avec un ciel uniforme, 3 – 4 % d'écart
(PSNR ≈ 32 dB) ; avec le HDR réel, le moteur est 16 – 25 % plus clair que three.js (PSNR ≈ 20 dB) ;
l'attribution à `PMREMGenerator` est une hypothèse non vérifiée. La cohérence des reflets est établie par
des tests contre une référence CPU et une force brute (cartes à moins de 7,4 %), pas par une
ressemblance pixel à pixel avec un viewer. À toi de décider si « cohérent » est atteint : regarde le
casque dans le lanceur à côté d'un viewer (par ex. https://github.khronos.org/glTF-Sample-Viewer-Release/)
avec le même HDR.

**3. À faire.** Mon environnement n'a que SwiftShader (processeur) : toute mesure de FPS y serait sans
valeur pour ta RTX 5070.

**4. Ce que couvre `engine.shader_hot`.** Le test modifie une source HLSL sur disque pendant qu'un
pipeline tourne et vérifie le pixel dessiné avant/après ; la poll du renderer (`renderer.c`) appelle
`eng_shader_hot_poll` pour le vertex et le fragment shader de chaque pipeline. Non couvert : le geste
interactif (éditer `shaders/multitex.frag.hlsl` pendant que le lanceur affiche le casque). Disponible en Debug
seulement (`OW_SHADER_HOT_RELOAD`).

## Livrable : « un casque PBR éclairé par image, avec soleil et ombres, dans un moteur à renderer abstrait »

| Élément | Preuve | État |
|---|---|---|
| Navigation à la caméra libre (souris + ZQSD) | lancer `build/debug/bin/launcher` sur un vrai écran | À faire |
| Modèle glTF avec textures (cube de démo) | `assets/models/demo.gltf` ; tests `engine.model`, `engine.model_gpu` | Prouvé (Linux) ; visuel À faire |
| Ciel, éclairage par image, soleil, tonemapping | tests `engine.environment_gpu`, `engine.tonemap_gpu`, `engine.lighting` ; F1 pour les curseurs | Prouvé (Linux) ; visuel À faire |
| Ombres du soleil | tests `engine.shadow`, `engine.shadow_gpu` ; case « ombre » dans F1 | Prouvé (Linux) ; visuel À faire |
| Interface de débogage (F1) et tweaks | tests `engine.ui`, `engine.tweak` | Prouvé (Linux) ; visuel À faire |
| Images en vol 1 à 3 | `launcher --frames-in-flight 1|2|3` ; `docs/frames-in-flight.md` | Prouvé (Linux) ; mesure À faire |
| Passes et ressources nommées (RenderDoc) | 29 tests `engine.gpu_debug` + trace au niveau du pilote (`docs/gpu-debug.md`) | Prouvé jusqu'au pilote ; RenderDoc lui-même **À faire** |

## Captures et mesures à faire (sur ta machine)

1. Une capture du casque dans le lanceur (F1 ouvert) et une du viewer de référence avec le même HDR :
   à placer dans `docs/devlog/img/` (nommer `v0.1-casque.png`, `v0.1-reference.png`).
2. FPS affichés en F1, scène de test, résolution de ta fenêtre habituelle : ombres cochées / décochées,
   rayon du filtre 0 / 3, `--frames-in-flight 1` / `2`. Noter les chiffres dans le devlog `v0.1.md`
   (section « Mesures sur RTX 5070 »). Critère : ≥ 60 FPS avec ombres.
3. Une capture RenderDoc d'une image (procédure : `docs/gpu-debug.md`) montrant « Frame N » > passes.
4. Hot reload : lanceur en Debug, éditer un fichier de `shaders/` (`*.hlsl`), enregistrer : l'image change sans relancer ;
   introduire une erreur : l'ancien rendu reste et le journal signale l'erreur.

## Hygiène du dépôt (restes de M0 et de M1)

- [ ] commits M0-15 à M1-19 sur ta branche (beaucoup de fichiers modifiés non commités), puis fusion dans `main`
- [ ] `docs/conventions-maths.md` : à fusionner dans `docs/conventions.md` (ou à assumer comme fichier séparé)
- [ ] `Makefile` racine : `clean`/`fclean` doivent appeler `make -C sdl clean`
- [ ] restes de la revue M0 (`docs/devlog/m0-revue.md`, section « Hygiène ») encore cochés « à faire »
- [ ] tableau de suivi : tâches M1-14 à M1-19 passées à jour
- [ ] sur ta machine : `ctest --preset debug` → 52 entrées passent (M1-17 : 48, M1-18 : 51, M1-19 : 52)
- [ ] `tools/scripts/format.sh --check` et `tools/scripts/lint.sh` propres sur `main`

## Dette connue à l'entrée de M2

- Deux allocations dans la boucle de frame restent inexpliquées (ADR 0003).
- Un SEGV observé avec le pilote NVIDIA n'a pas de cause établie.
- Avertissements « dessin ignoré » au démarrage du lanceur : à trier (bruit ou vrai défaut).
- glTF : seul `TEXCOORD_0` ; pas d'`alphaMode`, de double face ni d'extensions (`docs/pbr.md`).
- Un seul sampler pour les cinq textures d'un dessin (celui de glTF est ignoré).
- Ombres : un seul soleil, instables en rotation ; le disque de soleil du ciel `.hdr` n'est pas ombré.
- Les shaders compilés à chaud ne sont pas nommés dans les captures.
- Éventuel service `EngServices` v3 (tweaks) si le jeu doit exposer des curseurs.

## Clôture

```bash
tools/scripts/m1-exit-check.sh debug      # critères 1 et 4 + suite
git switch main && git pull
git tag -a v0.1 -m "M1 : rendu de base"
git push origin v0.1
```

Ne tague qu'une fois les lignes « À faire » du tableau des critères de sortie confirmées (au minimum le
critère 3 et un coup d'œil au critère 2). Si le critère 3 échoue, c'est une information, pas un échec
de la revue : note les chiffres, ouvre une tâche d'optimisation, et tague quand même en le disant dans
le devlog — à toi de choisir.
