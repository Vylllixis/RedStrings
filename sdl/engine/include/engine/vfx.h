#ifndef ENG_VFX_H
#define ENG_VFX_H

#include <stdbool.h>
#include <stdint.h>

#include "engine/combat_sim.h"
#include "engine/debug_lines.h"

/* Effets visuels de combat (M5-20) : étincelles d'impact (particules) et traînée d'arme.
 * Module de DONNÉES, déterministe (générateur xorshift à graine) : il calcule des positions, des
 * couleurs et des opacités. Le rendu final (billboards, rubans texturés) n'est pas fait ici ; en
 * attendant, `*_draw_debug` les trace avec les lignes de debug (visibles dans la vue de debug). */

#define ENG_PARTICLES_MAX 512U
#define ENG_TRAIL_MAX 24U

typedef struct EngParticle {
    float pos[3];
    float vel[3];
    uint32_t age;
    uint32_t life;
    float size;
    float gravity; /* m/s², positif = vers le bas */
    EngDebugColor start;
    EngDebugColor end;
} EngParticle;

typedef struct EngParticleSpec {
    uint32_t count;    /* 1 à 64 */
    float speed_min;   /* m/s, ≥ 0 */
    float speed_max;   /* ≥ speed_min */
    float gravity;     /* m/s², positif = vers le bas */
    uint32_t life_min; /* ticks, ≥ 1 */
    uint32_t life_max; /* ≥ life_min */
    float size;        /* m, > 0 */
    EngDebugColor start;
    EngDebugColor end;
} EngParticleSpec;

typedef enum EngVfxPreset {
    ENG_VFX_SPARK_LIGHT = 0, /* touche légère : jaune → orange */
    ENG_VFX_SPARK_HEAVY,     /* touche lourde : plus nombreuse, plus rapide, rouge */
    ENG_VFX_SPARK_BLOCK,     /* garde : bleu clair */
    ENG_VFX_FLASH_PARRY,     /* parade : cyan, éclat bref */
    ENG_VFX_PRESET_COUNT
} EngVfxPreset;

typedef struct EngParticles {
    EngParticle items[ENG_PARTICLES_MAX];
    uint32_t count; /* vivantes, rangées au début */
    uint32_t rng;
    uint32_t spawned;
    uint32_t dropped; /* particules refusées faute de place */
} EngParticles;

void eng_particles_init(EngParticles *p, uint32_t seed);
bool eng_particle_spec_valid(const EngParticleSpec *s);
bool eng_particle_preset(EngVfxPreset preset, EngParticleSpec *out);

/* Émet `spec.count` particules depuis `origin`, dans des directions tirées sur la sphère
 * (déterministe pour une graine donnée). Retourne le nombre créé (le reste est compté dans
 * `dropped`), 0 si arguments invalides. */
uint32_t eng_particles_burst(EngParticles *p, const EngParticleSpec *spec, const float origin[3]);

/* Un tick (1/60 s) : intégration, gravité, vieillissement, retrait des mortes. */
void eng_particles_update(EngParticles *p);

/* Couleur interpolée (début → fin) et opacité (1 → 0) d'une particule. */
EngDebugColor eng_particle_color(const EngParticle *q);
float eng_particle_alpha(const EngParticle *q);

/* Une touche déclenche le préréglage correspondant à son type. Retourne le nombre de particules. */
uint32_t eng_vfx_on_event(EngParticles *p, const EngSimEvent *e);

/* Trace chaque particule en petit trait orienté selon sa vitesse. Retourne le nombre de traits. */
uint32_t eng_particles_draw_debug(const EngParticles *p, EngDebugLines *lines);

/* ── Traînée d'arme ── */

typedef struct EngTrailSample {
    float a[3]; /* base de la lame */
    float b[3]; /* pointe */
    uint32_t age;
} EngTrailSample;

typedef struct EngWeaponTrail {
    EngTrailSample samples[ENG_TRAIL_MAX]; /* du plus ancien au plus récent */
    uint32_t count;
    uint32_t life; /* ticks avant disparition complète, 1 à ENG_TRAIL_MAX */
} EngWeaponTrail;

bool eng_trail_init(EngWeaponTrail *t, uint32_t life);

/* Ajoute la position courante de l'arme (un appel par tick d'attaque active). */
bool eng_trail_push(EngWeaponTrail *t, const float a[3], const float b[3]);

/* Un tick sans ajout : vieillit les échantillons et retire les expirés. */
void eng_trail_update(EngWeaponTrail *t);

/* Opacité de l'échantillon i : 1 pour le plus récent, 0 à l'expiration. */
float eng_trail_alpha(const EngWeaponTrail *t, uint32_t i);

/* Ruban : 2 sommets par échantillon (base, pointe) dans l'ordre d'une bande de triangles.
 * `out_alpha` reçoit l'opacité de chaque paire. Retourne le nombre de sommets écrits (2 ×
 * échantillons dans la limite de `max_vertices`). */
uint32_t eng_trail_build_strip(const EngWeaponTrail *t, float (*out_pos)[3], float *out_alpha,
                               uint32_t max_vertices);

uint32_t eng_trail_draw_debug(const EngWeaponTrail *t, EngDebugLines *lines);

#endif
