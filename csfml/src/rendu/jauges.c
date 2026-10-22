/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Instruments circulaires : arcs, graduations, ellipses et radar de statistiques.
 */

#include <math.h>
#include "personnage.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_hud.h"
#include "gui_types.h"

#define MAX_POINTS_ARC 96

/** Arc de cercle épais entre les angles a0 et a1 (radians). */
void arc_cercle(app_t *app, sfVector2f centre, float rayon, float epaisseur,
    sfVector2f angles, sfColor c, int additif)
{
    sfVector2f pts[MAX_POINTS_ARC];
    int n = (int)bornerf(fabsf(angles.y - angles.x) * rayon / 6.f, 4.f,
        MAX_POINTS_ARC);
    float a = 0.f;

    for (int i = 0; i < n; i++) {
        a = lerpf(angles.x, angles.y, i / (float)(n - 1));
        pts[i] = (sfVector2f){centre.x + cosf(a) * rayon, centre.y + sinf(a) * rayon};
    }
    ligne_epaisse(app, pts, n, epaisseur, c, additif);
}

/** Couronne de `n` graduations ; une sur cinq est plus longue. */
void graduations(app_t *app, sfVector2f centre, float rayon, int n,
    float longueur, float rotation, sfColor c)
{
    sfVertex v[2 * 120];
    float a = 0.f;
    float l = 0.f;

    if (n > 120)
        n = 120;
    c = avec_alpha(c, app->alpha);
    for (int i = 0; i < n; i++) {
        a = rotation + i * 2.f * PI / n;
        l = i % 5 == 0 ? longueur * 1.8f : longueur;
        v[2 * i] = (sfVertex){{centre.x + cosf(a) * rayon, centre.y + sinf(a) * rayon},
            c, {0, 0}};
        v[2 * i + 1] = (sfVertex){{centre.x + cosf(a) * (rayon + l),
            centre.y + sinf(a) * (rayon + l)}, c, {0, 0}};
    }
    cible_primitives(app, v, 2 * n, sfLines, NULL);
}

/** Contour d'ellipse (socle, orbites). */
void ellipse(app_t *app, sfVector2f centre, sfVector2f rayon, float epaisseur,
    sfColor c)
{
    sfVector2f pts[65];
    float a = 0.f;

    for (int i = 0; i <= 64; i++) {
        a = i * 2.f * PI / 64.f;
        pts[i] = (sfVector2f){centre.x + cosf(a) * rayon.x,
            centre.y + sinf(a) * rayon.y};
    }
    ligne_epaisse(app, pts, 65, epaisseur, c, 1);
}

static sfVector2f sommet_radar(sfVector2f c, float r, int i)
{
    float a = -PI / 2.f + i * 2.f * PI / NB_STATS;

    return (sfVector2f){c.x + cosf(a) * r, c.y + sinf(a) * r};
}

/**
 * Radar des statistiques : un axe par statistique (PV en haut, puis dans le
 * sens des aiguilles d'une montre). `v` contient des valeurs entre 0 et 1.
 */
void radar_stats(app_t *app, sfVector2f centre, float rayon,
    const float v[NB_STATS], sfColor c)
{
    sfVector2f pts[NB_STATS + 1];
    sfVertex fond[NB_STATS + 2];
    float k = 0.f;

    for (int n = 1; n <= 4; n++) {
        k = rayon * n / 4.f;
        for (int i = 0; i <= NB_STATS; i++)
            pts[i] = sommet_radar(centre, k, i % NB_STATS);
        ligne_epaisse(app, pts, NB_STATS + 1, 1.f, avec_alpha(C_BLANC, n == 4 ? 0.16f
            : 0.06f), 0);
    }
    for (int i = 0; i < NB_STATS; i++) {
        pts[0] = centre;
        pts[1] = sommet_radar(centre, rayon, i);
        ligne_epaisse(app, pts, 2, 1.f, avec_alpha(couleur_stat((stat_t)i), 0.25f), 0);
    }
    fond[0] = (sfVertex){centre, avec_alpha(c, 0.10f * app->alpha), {0, 0}};
    for (int i = 0; i <= NB_STATS; i++) {
        pts[i] = sommet_radar(centre, rayon * bornerf(v[i % NB_STATS], 0.04f, 1.f),
            i % NB_STATS);
        fond[i + 1] = (sfVertex){pts[i], avec_alpha(c, 0.42f * app->alpha), {0, 0}};
    }
    cible_primitives(app, fond, NB_STATS + 2, sfTriangleFan,
        NULL);
    ligne_epaisse(app, pts, NB_STATS + 1, 2.f, melanger(c, C_BLANC, 0.25f), 0);
    for (int i = 0; i < NB_STATS; i++) {
        lueur(app, pts[i], (sfVector2f){16.f, 16.f}, avec_alpha(couleur_stat((stat_t)i),
            0.6f));
        losange(app, pts[i], 4.f, melanger(couleur_stat((stat_t)i), C_BLANC, 0.35f));
    }
}
