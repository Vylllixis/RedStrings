# ADR 0020 : instancing par second flux de sommets au rythme « instance »

Statut : accepté (M3-07)

## Contexte

M3-07 demande l'instancing GPU de 50 000 rochers à 60 FPS. Il faut un moyen de passer les données de chaque copie au
GPU sans un dessin ni un envoi d'uniforms par copie.

## Décision

Un **second flux de sommets** (slot 1) déclaré dans le pipeline avec `SDL_GPU_VERTEXINPUTRATE_INSTANCE`, rempli par un
buffer immuable d'`EngInstance` (36 octets : position + échelle, quaternion, teinte). `EngDrawCmd` porte le buffer et
le nombre d'instances ; le renderer valide la cohérence (flux, taille du buffer) et rejette sinon. Le vertex shader
`instanced.vert` reproduit les sorties de `model.vert` : le fragment shader `model.frag` est réutilisé tel quel.

## Alternatives

- **Buffer de stockage lu par `SV_InstanceID`** : plus souple (matrices complètes, culling GPU futur), mais demande
  d'ajouter les buffers de stockage au renderer (création, liaison côté vertex, usage de SDL_GPU) ; reportable.
- **Uniforms par instance (un dessin chacune)** : c'est ce qu'on évite : 50 000 dessins.
- **Tableau de matrices dans un uniform** : limité à 4096 octets par dessin dans ce renderer (64 matrices).
- **Matrice 4 × 4 complète par instance** (64 octets) : permet l'échelle non uniforme, mais 77 % de plus de mémoire et
  une matrice normale à calculer par sommet ; la rotation + échelle uniforme suffit aux rochers, arbres, herbes.

## Conséquences

- Les ombres ne sont pas instanciées (pas de variante instanciée du shader d'ombre) : à ajouter si nécessaire.
- L'échelle est uniforme ; une échelle non uniforme demandera la matrice complète ou un second attribut.
- Les 16 emplacements d'attributs au total (8 par flux) restent bien au-dessus des 7 utilisés.
- Les performances sur GPU réel ne sont pas mesurées ici (SwiftShader seulement).
