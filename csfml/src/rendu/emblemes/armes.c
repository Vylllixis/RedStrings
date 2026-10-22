/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Emblèmes des classes de combat : épées, lame, bouclier, hache, lance, arc, cible.
 */

#include <math.h>
#include "gui_anim.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "emblemes_interne.h"

const sfColor emb_TROU = {12, 14, 28, 255};

void emb_d_epees(app_t *app, sfVector2f c, float t, sfColor col)
{
    emb_epee(app, c, t, -0.72f, col);
    emb_epee(app, c, t, 0.72f, col);
}

void emb_d_lame(app_t *app, sfVector2f c, float t, sfColor col)
{
    emb_epee(app, c, t * 1.1f, 0.62f, col);
}

void emb_d_bouclier(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float ECU[] = {-0.68f, -0.78f, 0.68f, -0.78f, 0.68f, 0.05f, 0.f, 0.9f,
        -0.68f, 0.05f};
    static const float CREUX[] = {-0.48f, -0.6f, 0.48f, -0.6f, 0.48f, 0.f, 0.f, 0.64f,
        -0.48f, 0.f};
    static const float BARRES[2][4] = {{0.f, -0.6f, 0.f, 0.6f}, {-0.48f, -0.18f,
        0.48f, -0.18f}};

    polygone(app, ECU, 5, c, t, 0.f, col);
    polygone(app, CREUX, 5, c, t, 0.f, melanger(col, emb_TROU, 0.7f));
    for (int k = 0; k < 2; k++)
        emb_trait(app, c, t, BARRES[k], 0.13f, col);
}

void emb_d_hache(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float MANCHE[4] = {-0.05f, 0.92f, 0.05f, -0.92f};
    static const float FER[] = {0.05f, -0.8f, 0.55f, -0.95f, 0.82f, -0.45f, 0.55f,
        0.05f, 0.05f, -0.15f};
    static const float POINTE[] = {-0.05f, -0.7f, -0.45f, -0.52f, -0.05f, -0.34f};

    emb_trait(app, c, t, MANCHE, 0.13f, col);
    polygone(app, FER, 5, c, t, 0.f, col);
    polygone(app, POINTE, 3, c, t, 0.f, col);
}

void emb_d_lance(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float HAMPE[4] = {-0.62f, 0.86f, 0.4f, -0.46f};
    static const float FER[] = {0.f, -1.f, 0.36f, 0.f, 0.f, 0.42f, -0.36f, 0.f};

    emb_trait(app, c, t, HAMPE, 0.11f, col);
    polygone(app, FER, 4, emb_pt(c, t, 0.52f, -0.62f), 0.42f * t, 0.66f, col);
}

void emb_d_arc(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float POINTE[] = {0.88f, -0.88f, 0.739f, -0.541f, 0.541f, -0.739f};
    static const float PLUME_A[] = {-0.378f, 0.378f, -0.236f, 0.236f, -0.215f, 0.399f,
        -0.342f, 0.526f};
    static const float PLUME_B[] = {-0.378f, 0.378f, -0.236f, 0.236f, -0.399f, 0.215f,
        -0.526f, 0.342f};
    sfVector2f bezier[24];
    sfVector2f corde[3] = {emb_pt(c, t, 0.38f, 0.78f), emb_pt(c, t, -0.42f, 0.42f),
        emb_pt(c, t, -0.78f, -0.38f)};
    sfVector2f fleche[2] = {emb_pt(c, t, -0.42f, 0.42f), emb_pt(c, t, 0.66f, -0.66f)};
    float u = 0.f;

    for (int i = 0; i < 24; i++) {
        u = i / 23.f;
        bezier[i] = emb_pt(c, t,
            (1 - u) * (1 - u) * 0.38f + 2 * (1 - u) * u * 0.507f + u * u * -0.78f,
            (1 - u) * (1 - u) * 0.78f + 2 * (1 - u) * u * -0.507f + u * u * -0.38f);
    }
    ligne_epaisse(app, corde, 3, fmaxf(1.f, 0.035f * t), avec_alpha(col, 0.75f), 0);
    ligne_epaisse(app, bezier, 24, emb_ep(t, 0.11f), col, 0);
    ligne_epaisse(app, fleche, 2, emb_ep(t, 0.075f), col, 0);
    polygone(app, POINTE, 3, c, t, 0.f, col);
    polygone(app, PLUME_A, 4, c, t, 0.f, col);
    polygone(app, PLUME_B, 4, c, t, 0.f, col);
}

void emb_d_cible(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float CROIX[4][4] = {{0.f, -1.f, 0.f, -0.25f}, {0.f, 0.25f, 0.f, 1.f},
        {-1.f, 0.f, -0.25f, 0.f}, {0.25f, 0.f, 1.f, 0.f}};

    arc_cercle(app, c, 0.76f * t, emb_ep(t, 0.1f), (sfVector2f){0.f, 2.f * PI}, col, 0);
    arc_cercle(app, c, 0.44f * t, emb_ep(t, 0.08f), (sfVector2f){0.f, 2.f * PI},
        avec_alpha(col, 0.7f), 0);
    cercle(app, c, 0.13f * t, col, C_TRANSPARENT, 0.f);
    for (int k = 0; k < 4; k++)
        emb_trait(app, c, t, CROIX[k], 0.07f, col);
}
