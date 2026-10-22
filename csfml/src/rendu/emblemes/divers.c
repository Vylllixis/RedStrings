/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Emblèmes des classes de soutien et d'ingénierie : ancre, croix, note, fiole, engrenage.
 */

#include <math.h>
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "emblemes_interne.h"

void emb_d_ancre(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float TIGE[4] = {0.f, -0.56f, 0.f, 0.74f};
    static const float JAS[4] = {-0.4f, -0.4f, 0.4f, -0.4f};
    static const float PATTE_D[] = {0.5f, 0.22f, 0.86f, 0.06f, 0.74f, 0.42f};
    static const float PATTE_G[] = {-0.5f, 0.22f, -0.74f, 0.42f, -0.86f, 0.06f};

    arc_cercle(app, emb_pt(c, t, 0.f, -0.72f), 0.16f * t, emb_ep(t, 0.09f),
        (sfVector2f){0.f, 2.f * PI}, col, 0);
    emb_trait(app, c, t, TIGE, 0.12f, col);
    emb_trait(app, c, t, JAS, 0.11f, col);
    arc_cercle(app, emb_pt(c, t, 0.f, 0.05f), 0.7f * t, emb_ep(t, 0.12f),
        (sfVector2f){0.35f, PI - 0.35f}, col, 0);
    polygone(app, PATTE_D, 3, c, t, 0.f, col);
    polygone(app, PATTE_G, 3, c, t, 0.f, col);
}

void emb_d_croix(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float VERTICALE[] = {-0.16f, -0.86f, 0.16f, -0.86f, 0.16f, 0.86f,
        -0.16f, 0.86f};
    static const float HORIZONTALE[] = {-0.62f, -0.4f, 0.62f, -0.4f, 0.62f, -0.08f,
        -0.62f, -0.08f};

    lueur(app, emb_pt(c, t, 0.f, -0.24f), (sfVector2f){t, t}, avec_alpha(col, 0.35f));
    polygone(app, VERTICALE, 4, c, t, 0.f, col);
    polygone(app, HORIZONTALE, 4, c, t, 0.f, col);
}

void emb_d_note(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float HAMPE[4] = {0.f, 0.5f, 0.f, -0.84f};
    static const float FANION[] = {0.f, -0.86f, 0.52f, -0.56f, 0.46f, -0.3f, 0.f,
        -0.56f};

    cercle(app, emb_pt(c, t, -0.24f, 0.52f), 0.27f * t, col, C_TRANSPARENT, 0.f);
    emb_trait(app, c, t, HAMPE, 0.1f, col);
    polygone(app, FANION, 4, c, t, 0.f, col);
}

void emb_d_fiole(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float COL[] = {-0.15f, -0.74f, 0.15f, -0.74f, 0.15f, -0.18f, -0.15f,
        -0.18f};
    static const float BOUCHON[] = {-0.22f, -0.92f, 0.22f, -0.92f, 0.22f, -0.74f, -0.22f,
        -0.74f};

    cercle(app, emb_pt(c, t, 0.f, 0.3f), 0.56f * t, avec_alpha(col, 0.22f), col,
        emb_ep(t, 0.08f));
    cercle(app, emb_pt(c, t, 0.f, 0.42f), 0.36f * t, col, C_TRANSPARENT, 0.f);
    polygone(app, COL, 4, c, t, 0.f, avec_alpha(col, 0.55f));
    polygone(app, BOUCHON, 4, c, t, 0.f, col);
    cercle(app, emb_pt(c, t, 0.14f, 0.16f + 0.06f * sinf(app->temps * 3.f)), 0.06f * t,
        melanger(col, C_BLANC, 0.6f), C_TRANSPARENT, 0.f);
}

void emb_d_engrenage(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float DENT[] = {-0.13f, -0.9f, 0.13f, -0.9f, 0.17f, -0.55f, -0.17f,
        -0.55f};

    for (int k = 0; k < 8; k++)
        polygone(app, DENT, 4, c, t, app->temps * 0.4f + k * PI / 4.f, col);
    cercle(app, c, 0.62f * t, col, C_TRANSPARENT, 0.f);
    cercle(app, c, 0.24f * t, emb_TROU, C_TRANSPARENT, 0.f);
}
