/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ magie.c ────────────────────────────────────────────────────────────
** │ But         Emblèmes des classes magiques : étoile, flamme, crâne,
** │             portail, feuille, lune, sablier, orbe.
** │ Contenu     emb_d_etoile, emb_d_flamme, emb_d_crane, emb_d_portail,
** │             emb_d_feuille, emb_d_lune, emb_d_sablier, emb_d_orbe
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "emblemes_interne.h"

void emb_d_etoile(app_t *app, sfVector2f c, float t, sfColor col)
{
    if (app == NULL)
        return;

    emb_etoile(app, emb_pt(c, t, -0.08f, 0.08f), t * 0.85f, sinf(app->temps * 0.8f) * 0.12f,
        col);
    emb_etoile(app, emb_pt(c, t, 0.62f, -0.6f), t * 0.3f, app->temps * 0.9f, col);
    cercle(app, emb_pt(c, t, -0.62f, -0.56f), 0.08f * t, avec_alpha(col, 0.5f + 0.5f
        * sinf(app->temps * 3.f)), C_TRANSPARENT, 0.f);
}

void emb_d_flamme(app_t *app, sfVector2f c, float t, sfColor col)
{
    if (app == NULL)
        return;

    float s = 1.f + 0.05f * sinf(app->temps * 9.f);

    emb_flamme(app, c, t * s, col);
    emb_flamme(app, emb_pt(c, t, 0.f, 0.22f), t * 0.5f, melanger(col, C_BLANC, 0.6f));
}

void emb_d_crane(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float MACHOIRE[] = {-0.34f, 0.2f, 0.34f, 0.2f, 0.3f, 0.72f, -0.3f,
        0.72f};
    static const float NEZ[] = {0.f, 0.06f, 0.09f, 0.26f, -0.09f, 0.26f};
    static const float DENTS[3][4] = {{-0.12f, 0.46f, -0.12f, 0.72f}, {0.f, 0.46f, 0.f,
        0.72f}, {0.12f, 0.46f, 0.12f, 0.72f}};

    if (app == NULL)
        return;

    cercle(app, emb_pt(c, t, 0.f, -0.18f), 0.62f * t, col, C_TRANSPARENT, 0.f);
    polygone(app, MACHOIRE, 4, c, t, 0.f, col);
    cercle(app, emb_pt(c, t, -0.25f, -0.14f), 0.17f * t, emb_TROU, C_TRANSPARENT, 0.f);
    cercle(app, emb_pt(c, t, 0.25f, -0.14f), 0.17f * t, emb_TROU, C_TRANSPARENT, 0.f);
    polygone(app, NEZ, 3, c, t, 0.f, emb_TROU);
    for (int k = 0; k < 3; k++)
        emb_trait(app, c, t, DENTS[k], 0.05f, emb_TROU);
}

void emb_d_portail(app_t *app, sfVector2f c, float t, sfColor col)
{
    if (app == NULL)
        return;

    hexagone(app, c, 0.88f * t, app->temps * 0.6f, C_TRANSPARENT, col);
    hexagone(app, c, 0.58f * t, -app->temps, C_TRANSPARENT, avec_alpha(col, 0.75f));
    lueur(app, c, (sfVector2f){0.9f * t, 0.9f * t}, avec_alpha(col, 0.5f));
    cercle(app, c, 0.2f * t, melanger(col, C_BLANC, 0.5f), C_TRANSPARENT, 0.f);
}

void emb_d_feuille(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float NERVURE[4] = {-0.5f, 0.68f, 0.5f, -0.68f};
    float pts[36];
    float u = 0.f;

    if (app == NULL)
        return;

    for (int k = 0; k < 9; k++) {
        u = k * PI / 8.f;
        pts[2 * k] = 0.42f * sinf(u);
        pts[2 * k + 1] = -0.92f * cosf(u);
        pts[34 - 2 * k] = -0.42f * sinf(u);
        pts[35 - 2 * k] = -0.92f * cosf(u);
    }
    polygone(app, pts, 18, c, t, 0.62f, col);
    emb_trait(app, c, t, NERVURE, 0.06f, melanger(col, emb_TROU, 0.6f));
}

void emb_d_lune(app_t *app, sfVector2f c, float t, sfColor col)
{
    if (app == NULL)
        return;

    arc_cercle(app, c, 0.6f * t, emb_ep(t, 0.3f), (sfVector2f){0.95f, 5.33f}, col, 0);
    losange(app, emb_pt(c, t, 0.46f, -0.34f), 0.16f * t, melanger(col, C_BLANC, 0.4f));
}

void emb_d_sablier(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float HAUT[] = {-0.55f, -0.74f, 0.55f, -0.74f, 0.f, 0.f};
    static const float BAS[] = {0.f, 0.f, 0.55f, 0.74f, -0.55f, 0.74f};
    static const float BARRES[2][4] = {{-0.72f, -0.84f, 0.72f, -0.84f}, {-0.72f, 0.84f,
        0.72f, 0.84f}};

    if (app == NULL)
        return;

    polygone(app, HAUT, 3, c, t, 0.f, avec_alpha(col, 0.45f));
    polygone(app, BAS, 3, c, t, 0.f, col);
    for (int k = 0; k < 2; k++)
        emb_trait(app, c, t, BARRES[k], 0.14f, col);
}

void emb_d_orbe(app_t *app, sfVector2f c, float t, sfColor col)
{
    float a = 0.f;

    if (app == NULL)
        return;

    lueur(app, c, (sfVector2f){1.2f * t, 1.2f * t}, avec_alpha(col, 0.5f));
    cercle(app, c, 0.38f * t, col, C_TRANSPARENT, 0.f);
    ellipse(app, c, (sfVector2f){0.88f * t, 0.32f * t}, emb_ep(t, 0.06f),
        avec_alpha(col, 0.8f));
    for (int k = 0; k < 3; k++) {
        a = app->temps * 1.4f + k * 2.f * PI / 3.f;
        losange(app, emb_pt(c, t, cosf(a) * 0.88f, sinf(a) * 0.32f), 0.1f * t,
            melanger(col, C_BLANC, 0.5f));
    }
}
