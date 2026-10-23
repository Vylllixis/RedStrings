/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ formes.c ───────────────────────────────────────────────────────────
** │ But         Formes de base : rectangles arrondis, ombres, halos,
** │             cercles, polygones, lignes épaisses.
** │ Contenu     rect_arrondi, rect_arrondi_bord, ombre, lueur, cercle,
** │             polygone, ligne_epaisse
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_types.h"

#define POINTS_COIN 8

#define MAX_POINTS_LIGNE 160

void rect_arrondi(app_t *app, sfFloatRect r, float rayon, sfColor fond)
{
    if (app == NULL)
        return;

    rect_arrondi_bord(app, r, rayon, fond, C_TRANSPARENT);
}

void rect_arrondi_bord(app_t *app, sfFloatRect r, float rayon, sfColor fond,
    sfColor bord)
{
    float rc = bornerf(rayon, 0.f, fminf(r.width, r.height) / 2.f);
    const sfVector2f centres[4] = {
        {r.left + r.width - rc, r.top + rc},
        {r.left + r.width - rc, r.top + r.height - rc},
        {r.left + rc, r.top + r.height - rc},
        {r.left + rc, r.top + rc}
    };
    float angle = 0.f;
    int k = 0;

    if (app == NULL)
        return;
    rayon = rc;
    sfConvexShape_setPointCount(app->forme, 4 * POINTS_COIN);
    for (int c = 0; c < 4; c++) {
        for (int i = 0; i < POINTS_COIN; i++) {
            angle = (c * 90.f - 90.f + i * 90.f / (POINTS_COIN - 1)) * PI / 180.f;
            sfConvexShape_setPoint(app->forme, k++, (sfVector2f){
                centres[c].x + cosf(angle) * rayon,
                centres[c].y + sinf(angle) * rayon});
        }
    }
    sfConvexShape_setFillColor(app->forme, avec_alpha(fond, app->alpha));
    sfConvexShape_setOutlineColor(app->forme, avec_alpha(bord, app->alpha));
    sfConvexShape_setOutlineThickness(app->forme, bord.a > 0 ? -1.5f : 0.f);
    cible_convexe(app, app->forme, NULL);
}

/** Ombre portée douce : plusieurs couches de plus en plus larges. */
void ombre(app_t *app, sfFloatRect r, float rayon, float force)
{
    float e = 0.f;

    if (app == NULL)
        return;

    for (int i = 6; i >= 1; i--) {
        e = i * 3.f;
        rect_arrondi(app, (sfFloatRect){r.left - e, r.top - e + 6.f + i,
            r.width + 2.f * e, r.height + 2.f * e}, rayon + e,
            avec_alpha(C_NOIR, force * 0.075f * (1.f - i / 7.f)));
    }
}

/** Halo lumineux (dégradé radial en mélange additif). */
void lueur(app_t *app, sfVector2f centre, sfVector2f rayon, sfColor c)
{
    if (app == NULL)
        return;

    if (c.a == 0 || app->alpha <= 0.f)
        return;
    sfSprite_setTexture(app->sprite, app->tex_lueur, sfTrue);
    sfSprite_setOrigin(app->sprite, (sfVector2f){64.f, 64.f});
    sfSprite_setPosition(app->sprite, centre);
    sfSprite_setRotation(app->sprite, 0.f);
    sfSprite_setScale(app->sprite, (sfVector2f){rayon.x / 64.f,
        rayon.y / 64.f});
    sfSprite_setColor(app->sprite, avec_alpha(c, app->alpha));
    cible_sprite(app, app->sprite, &app->additif);
}

void cercle(app_t *app, sfVector2f centre, float r, sfColor fond,
    sfColor bord, float epaisseur)
{
    if (app == NULL)
        return;

    sfCircleShape_setRadius(app->cercle, r);
    sfCircleShape_setPointCount(app->cercle, r > 24.f ? 64 : 32);
    sfCircleShape_setOrigin(app->cercle, (sfVector2f){r, r});
    sfCircleShape_setPosition(app->cercle, centre);
    sfCircleShape_setFillColor(app->cercle, avec_alpha(fond, app->alpha));
    sfCircleShape_setOutlineColor(app->cercle, avec_alpha(bord, app->alpha));
    sfCircleShape_setOutlineThickness(app->cercle, -epaisseur);
    cible_cercle(app, app->cercle, NULL);
}

/** Polygone en coordonnées unitaires, mis à l'échelle et tourné. */
void polygone(app_t *app, const float *pts, int n, sfVector2f centre,
    float taille, float angle, sfColor c)
{
    if (app == NULL || pts == NULL)
        return;

    float ca = cosf(angle);
    float sa = sinf(angle);

    sfConvexShape_setPointCount(app->forme, n);
    for (int i = 0; i < n; i++) {
        sfConvexShape_setPoint(app->forme, i, (sfVector2f){
            centre.x + (pts[2 * i] * ca - pts[2 * i + 1] * sa) * taille,
            centre.y + (pts[2 * i] * sa + pts[2 * i + 1] * ca) * taille});
    }
    sfConvexShape_setFillColor(app->forme, avec_alpha(c, app->alpha));
    sfConvexShape_setOutlineThickness(app->forme, 0.f);
    cible_convexe(app, app->forme, NULL);
}

/** Polyligne épaisse construite en bande de triangles. */
void ligne_epaisse(app_t *app, const sfVector2f *pts, int n, float epaisseur,
    sfColor c, int additif)
{
    sfVertex v[MAX_POINTS_LIGNE * 2];
    sfVector2f d;
    float len = 0.f;
    int a = 0;
    int b = 0;

    if (app == NULL || pts == NULL)
        return;

    if (n > MAX_POINTS_LIGNE)
        n = MAX_POINTS_LIGNE;
    c = avec_alpha(c, app->alpha);
    for (int i = 0; i < n; i++) {
        a = i > 0 ? i - 1 : i;
        b = i < n - 1 ? i + 1 : i;
        d = (sfVector2f){pts[b].x - pts[a].x, pts[b].y - pts[a].y};
        len = sqrtf(d.x * d.x + d.y * d.y);
        if (len < 0.0001f)
            len = 1.f;
        d = (sfVector2f){-d.y / len * epaisseur / 2.f, d.x / len * epaisseur / 2.f};
        v[2 * i] = (sfVertex){{pts[i].x + d.x, pts[i].y + d.y}, c, {0, 0}};
        v[2 * i + 1] = (sfVertex){{pts[i].x - d.x, pts[i].y - d.y}, c, {0, 0}};
    }
    cible_primitives(app, v, n * 2, sfTriangleStrip,
        additif ? &app->additif : NULL);
}
