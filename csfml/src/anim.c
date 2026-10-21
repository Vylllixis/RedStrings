/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <math.h>
#include <stdlib.h>
#include "gui.h"

float bornerf(float v, float min, float max)
{
    if (v < min)
        return min;
    if (v > max)
        return max;
    return v;
}

float lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}

/** Avancement 0..1 d'une animation qui commence à `debut` et dure `duree`. */
float progression(float t, float debut, float duree)
{
    return bornerf((t - debut) / duree, 0.f, 1.f);
}

float sortie_cubique(float t)
{
    t = 1.f - t;
    return 1.f - t * t * t;
}

float entree_cubique(float t)
{
    return t * t * t;
}

float entree_sortie_cubique(float t)
{
    if (t < 0.5f)
        return 4.f * t * t * t;
    t = -2.f * t + 2.f;
    return 1.f - t * t * t / 2.f;
}

/** Sortie avec léger dépassement (ease_out_back). */
float sortie_rebond(float t)
{
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.f;

    t -= 1.f;
    return 1.f + c3 * t * t * t + c1 * t * t;
}

/** Lissage exponentiel indépendant du framerate. */
float approcher(float actuel, float cible, float vitesse, float dt)
{
    return cible + (actuel - cible) * expf(-vitesse * dt);
}

void maj_ressort(ressort_t *r, float cible, float raideur, float amorti,
    float dt)
{
    float acc = (cible - r->x) * raideur - r->v * amorti;

    r->v += acc * dt;
    r->x += r->v * dt;
}

float aleatoire(float min, float max)
{
    return min + (max - min) * ((float)rand() / (float)RAND_MAX);
}

sfColor avec_alpha(sfColor c, float a)
{
    c.a = (sfUint8)bornerf(c.a * a, 0.f, 255.f);
    return c;
}

sfColor melanger(sfColor a, sfColor b, float t)
{
    sfColor c;

    t = bornerf(t, 0.f, 1.f);
    c.r = (sfUint8)lerpf(a.r, b.r, t);
    c.g = (sfUint8)lerpf(a.g, b.g, t);
    c.b = (sfUint8)lerpf(a.b, b.b, t);
    c.a = (sfUint8)lerpf(a.a, b.a, t);
    return c;
}

/** Désature une couleur (personnage K.O.). */
sfColor griser(sfColor c, float t)
{
    sfUint8 g = (sfUint8)((c.r * 0.3f + c.g * 0.59f + c.b * 0.11f) * 0.7f);

    return melanger(c, (sfColor){g, g, (sfUint8)(g + 10 > 255 ? 255 : g + 10),
        c.a}, t);
}
