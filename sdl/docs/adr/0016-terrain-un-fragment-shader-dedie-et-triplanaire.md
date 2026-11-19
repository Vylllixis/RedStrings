# 0016 — Matériau de terrain : fragment shader dédié, triplanaire, splatmap par sommet

Statut : Accepté (M3-03)

## Contexte
M3-03 : quatre matériaux (herbe, roche, terre, neige) mélangés par une splatmap, « aucun étirement de texture
sur les falaises ». Le terrain est déjà un `EngModel` dessiné par `model.frag` (ADR 0015).

## Options
**Où mélanger.** (1) Cuire le mélange dans une texture unique `UV → couleur` : s'étire sur les falaises (c'est le
défaut à éviter). (2) **Mélanger dans le fragment shader** (retenu) : la splatmap pilote des textures tileables lues
en triplanaire.
**Comment réutiliser l'éclairage.** (1) `#include` HLSL : non disponible dans notre chaîne (shadercross sans dossier
d'inclusion). (2) Étendre `model.frag` avec des branches : alourdit tous les modèles. (3) **Fichier dédié
`terrain.frag`, copie des formules d'éclairage** (retenu), gardé aligné par un test pixel à pixel avec `model.frag`.
**Résolution de la splatmap.** Un texel par sommet (1025² : 4 Mio), lue en bilinéaire avec le décalage d'un demi-texel :
simple, alignée sur la géométrie ; une résolution plus fine (détail de peinture) se décidera avec l'outil de peinture.

## Décision
`terrain.frag` + `EngTerrainGpu`, mapping triplanaire en mètres (répétition 8 m, netteté 4), couches générées en
attendant l'art. `eng_model_gpu_draw_ex` ajoute au dessin de modèles des textures de remplacement, trois valeurs
libres (cbuffer `environment.yzw`) et une liste de parties.

## Conséquences
- 12 lectures de texture par pixel : à mesurer sur une vraie GPU (M3-04 et la revue M3 vérifieront le budget 60 FPS).
- Toute évolution de l'éclairage de `model.frag` doit être reportée dans `terrain.frag` (le test la détecte).
- Pas de normal map de terrain ni de rugosité par couche pour l'instant ; pas d'atténuation du triplanaire à
  distance (le tiling visible lointain sera traité avec le brouillard, M3-06).
