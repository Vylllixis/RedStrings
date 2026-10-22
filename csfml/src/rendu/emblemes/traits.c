/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ traits.c ───────────────────────────────────────────────────────────
** │ But         Primitives communes des emblèmes : points, épaisseurs,
** │             traits, épée, étoile, flamme.
** │ Contenu     emb_pt, emb_ep, emb_trait, emb_epee, emb_etoile,
** │             emb_flamme
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_types.h"
#include "emblemes_interne.h"

sfVector2f emb_pt(sfVector2f c, float t, float x, float y)
{
    return (sfVector2f){c.x + x * t, c.y + y * t};
}

/** Épaisseur proportionnelle, jamais sous 1,5 px pour rester lisible en petit. */
float emb_ep(float t, float f)
{
    return fmaxf(1.5f, f * t);
}

void emb_trait(app_t *app, sfVector2f c, float t, const float xy[4], float e,
    sfColor col)
{
    sfVector2f p[2] = {emb_pt(c, t, xy[0], xy[1]), emb_pt(c, t, xy[2], xy[3])};

    ligne_epaisse(app, p, 2, emb_ep(t, e), col, 0);
}

void emb_epee(app_t *app, sfVector2f c, float t, float angle, sfColor col)
{
    static const float LAME[] = {-0.08f, 0.3f, -0.08f, -0.72f, 0.f, -0.94f,
        0.08f, -0.72f, 0.08f, 0.3f};
    static const float GARDE[] = {-0.34f, 0.3f, 0.34f, 0.3f, 0.34f, 0.42f,
        -0.34f, 0.42f};
    static const float POIGNEE[] = {-0.055f, 0.42f, 0.055f, 0.42f, 0.055f, 0.74f,
        -0.055f, 0.74f};

    polygone(app, LAME, 5, c, t, angle, col);
    polygone(app, GARDE, 4, c, t, angle, col);
    polygone(app, POIGNEE, 4, c, t, angle, col);
    cercle(app, (sfVector2f){c.x - 0.84f * sinf(angle) * t,
        c.y + 0.84f * cosf(angle) * t}, 0.1f * t, col, C_TRANSPARENT, 0.f);
}

void emb_etoile(app_t *app, sfVector2f c, float t, float angle, sfColor col)
{
    float pts[16];
    float a = 0.f;

    for (int k = 0; k < 8; k++) {
        a = k * PI / 4.f - PI / 2.f;
        pts[2 * k] = cosf(a) * (k % 2 ? 0.3f : 1.f);
        pts[2 * k + 1] = sinf(a) * (k % 2 ? 0.3f : 1.f);
    }
    polygone(app, pts, 8, c, t, angle, col);
}

/** Goutte de emb_flamme : pointe en haut, base arrondie. */
void emb_flamme(app_t *app, sfVector2f c, float t, sfColor col)
{
    float pts[24];
    float a = 0.f;

    pts[0] = 0.f;
    pts[1] = -0.95f;
    for (int k = 0; k < 11; k++) {
        a = -0.35f + k * (PI + 0.7f) / 10.f;
        pts[2 + 2 * k] = cosf(a) * 0.55f;
        pts[3 + 2 * k] = 0.25f + sinf(a) * 0.55f;
    }
    polygone(app, pts, 12, c, t, 0.f, col);
}
