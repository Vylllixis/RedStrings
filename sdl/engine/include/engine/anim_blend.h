#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/anim_clip.h"
#include "engine/arena.h"
#include "engine/pose.h"
#include "engine/skeleton.h"

/* Mélange de poses et fondu enchaîné (M4-17).
 *
 * eng_pose_blend : mélange de deux poses locales articulation par articulation. Translation et
 * échelle : interpolation linéaire. Rotation : nlerp par le chemin le plus court (b est retourné si
 * a . b < 0, puis le résultat est normalisé) : pas de rotation « par le long chemin » entre deux
 * quaternions opposés, et un quaternion toujours unitaire. nlerp et non slerp : la vitesse
 * angulaire n'est pas parfaitement uniforme (écart maximal de l'ordre de 20 % à mi-chemin pour 180
 * degrés, imperceptible pour un fondu de 0,1 à 0,3 s) mais coûte trois fois moins.
 *
 * EngCrossfade : lecteur à deux sources. `eng_crossfade_play` démarre un nouveau clip ; l'ancien
 * continue de jouer pendant le fondu puis disparaît. Le poids du nouveau suit un smoothstep
 * (3t^2 - 2t^3) : vitesse nulle aux deux extrémités, donc pas d'à-coup au début ni à la fin du
 * fondu. Si l'on relance un clip PENDANT un fondu, la pose affichée à cet instant est figée comme
 * « source » du nouveau fondu : la pose reste continue (aucun saut), au prix de figer l'ancien
 * mouvement. Rien n'est alloué après eng_crossfade_init. */

/* out = a (1 - w) + b w. `w` est borné à [0 ; 1] (NaN -> 0). Les trois poses doivent avoir le même
 * nombre d'articulations ; `out` peut être a ou b. false si un argument est invalide. */
bool eng_pose_blend(EngPose *out, const EngPose *a, const EngPose *b, float weight);

/* Poids du nouveau clip pour une progression t du fondu (0..1) : smoothstep, borné. */
float eng_crossfade_curve(float t);

typedef struct EngAnimLayer {
    const EngAnimClip *clip; /* NULL : couche vide */
    float time;              /* secondes, dans le clip */
    float speed;
    bool loop;
} EngAnimLayer;

/* Avance une couche de `dt` secondes : bouclage ou butée (sans boucle). true si elle ARRIVE à sa
 * fin (vitesse > 0 et temps = durée, ou vitesse < 0 et temps = 0). `layer.clip` non NULL exigé. */
bool eng_anim_layer_advance(EngAnimLayer *layer, float dt);

typedef struct EngCrossfade {
    const EngSkeleton *skeleton;
    EngAnimLayer to;   /* clip courant */
    EngAnimLayer from; /* clip qui s'efface */
    bool has_from;
    bool from_frozen; /* la source est `snapshot`, pas un clip qui continue */
    float duration;   /* durée du fondu en cours, s */
    float elapsed;
    bool finished; /* clip courant sans boucle arrivé à sa fin */
    EngPose pose;  /* SORTIE : pose locale courante */
    EngPose snapshot;
    EngPose scratch;
} EngCrossfade;

/* Alloue trois poses dans `arena`. La pose de sortie est la pose de repos. false (arène restituée)
 * si un argument est invalide ou la mémoire manque. */
bool eng_crossfade_init(EngCrossfade *cf, EngArena *arena, const EngSkeleton *skeleton);

/* Joue `clip` avec un fondu de `fade_seconds` (>= 0 ; 0 = changement immédiat). `speed` : facteur
 * de temps (fini, non nul n'est pas exigé : 0 gèle). `start_time` : instant de départ. Sans clip
 * courant, il n'y a rien dont se fondre : le clip démarre tout de suite. Refuse (false, état
 * inchangé) un clip qui vise une articulation absente du squelette ou des valeurs non finies. */
bool eng_crossfade_play(EngCrossfade *cf, const EngAnimClip *clip, float fade_seconds, bool loop,
                        float speed, float start_time);

/* Avance de `dt` secondes (négatif ou non fini : ignoré) et évalue `cf->pose`. false si un clip
 * échoue à s'évaluer. */
bool eng_crossfade_update(EngCrossfade *cf, float dt);

/* true tant qu'un fondu est en cours ; poids actuel du nouveau clip (1 hors fondu). */
bool eng_crossfade_active(const EngCrossfade *cf);
float eng_crossfade_weight(const EngCrossfade *cf);
