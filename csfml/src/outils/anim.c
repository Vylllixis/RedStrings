/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ anim.c ─────────────────────────────────────────────────────────────
** │ But         Petits calculs d'animation et de couleur : bornes,
** │             interpolation, courbes d'accélération, ressort, mélange
** │             de couleurs.
** │ Contenu     bornerf, lerpf, progression, sortie_cubique,
** │             entree_cubique, entree_sortie_cubique, sortie_rebond,
** │             approcher, maj_ressort, aleatoire, avec_alpha, melanger,
** │             griser
** │ Testé par   csfml/tests/tests.c
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include <stdlib.h>
#include "gui_anim.h"
#include "gui_types.h"

float bornerf(float v, float min, float max)
{
    if (min > max) {
        float tmp = min;

        min = max;
        max = tmp;
    }
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
    if (duree <= 0.f)
        return t >= debut ? 1.f : 0.f;
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

/*
** ╭─ approcher ──────────────────────────────────────────────────────────
** │ Lissage exponentiel : `actuel` se rapproche de `cible`, de plus en
** │ plus lentement.
** │
** │ valeur = cible + (actuel - cible) x e^(-vitesse x dt)
** │ Le résultat ne dépend pas du nombre d'images par seconde : à 30 ou à
** │ 144 images/s, le mouvement dure pareil. Sert aux survols et aux
** │ positions animées.
** ╰──────────────────────────────────────────────────────────────────────
*/
float approcher(float actuel, float cible, float vitesse, float dt)
{
    if (dt <= 0.f || vitesse <= 0.f)
        return actuel;
    return cible + (actuel - cible) * expf(-vitesse * dt);
}

/*
** ╭─ maj_ressort ────────────────────────────────────────────────────────
** │ Ressort amorti : un mouvement organique (léger dépassement, puis
** │ stabilisation).
** │
** │ acc = (cible - x) x raideur - v x amorti ; puis v += acc x dt ; x +=
** │ v x dt
** │ Plus la raideur est grande, plus il rattrape vite ; plus l'amorti est
** │ grand, moins il oscille.
** ╰──────────────────────────────────────────────────────────────────────
*/
void maj_ressort(ressort_t *r, float cible, float raideur, float amorti,
    float dt)
{
    float acc = 0.f;

    if (r == NULL || dt <= 0.f)
        return;
    acc = (cible - r->x) * raideur - r->v * amorti;
    r->v += acc * dt;
    r->x += r->v * dt;
}

float aleatoire(float min, float max)
{
    if (min > max) {
        float tmp = min;

        min = max;
        max = tmp;
    }
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
