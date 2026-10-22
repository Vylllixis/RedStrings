/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Panneaux biseautés, crochets, losanges, hexagones, titres de section et touches de clavier.
 */

#include <math.h>
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_hud.h"
#include "gui_types.h"

/** Les 6 sommets d'un rectangle aux coins haut-gauche et bas-droite coupés. */
static void contour_biseau(sfFloatRect r, float coupe, sfVector2f pts[6])
{
    coupe = bornerf(coupe, 0.f, fminf(r.width, r.height) / 2.f);
    pts[0] = (sfVector2f){r.left + coupe, r.top};
    pts[1] = (sfVector2f){r.left + r.width, r.top};
    pts[2] = (sfVector2f){r.left + r.width, r.top + r.height - coupe};
    pts[3] = (sfVector2f){r.left + r.width - coupe, r.top + r.height};
    pts[4] = (sfVector2f){r.left, r.top + r.height};
    pts[5] = (sfVector2f){r.left, r.top + coupe};
}

/** Panneau biseauté : dégradé vertical `haut` → `bas` et liseré d'1 pixel. */
void panneau(app_t *app, sfFloatRect r, float coupe, sfColor haut, sfColor bas,
    sfColor bord)
{
    sfVector2f pts[6];
    sfVertex v[8];
    sfVertex l[7];
    float f = 0.f;

    contour_biseau(r, coupe, pts);
    v[0] = (sfVertex){{r.left + r.width / 2.f, r.top + r.height / 2.f},
        avec_alpha(melanger(haut, bas, 0.5f), app->alpha), {0, 0}};
    for (int i = 0; i < 7; i++) {
        f = (pts[i % 6].y - r.top) / r.height;
        v[i + 1] = (sfVertex){pts[i % 6], avec_alpha(melanger(haut, bas, f), app->alpha),
            {0, 0}};
        l[i] = (sfVertex){{floorf(pts[i % 6].x) + 0.5f, floorf(pts[i % 6].y) + 0.5f},
            avec_alpha(bord, app->alpha), {0, 0}};
    }
    cible_primitives(app, v, 8, sfTriangleFan, NULL);
    if (bord.a > 0)
        cible_primitives(app, l, 7, sfLinesStrip, NULL);
}

/** Ombre portée douce sous un panneau biseauté. */
void ombre_biseau(app_t *app, sfFloatRect r, float coupe, float force)
{
    float e = 0.f;
    sfColor c;

    for (int i = 5; i >= 1; i--) {
        e = i * 3.5f;
        c = avec_alpha(C_NOIR, force * 0.08f * (1.f - i / 6.f));
        panneau(app, (sfFloatRect){r.left - e, r.top - e + 8.f + i, r.width + 2.f * e,
            r.height + 2.f * e}, coupe + e * 0.6f, c, c, C_TRANSPARENT);
    }
}

/** Crochets lumineux sur les deux coins non coupés (haut-droite, bas-gauche). */
void crochets(app_t *app, sfFloatRect r, float longueur, sfColor c)
{
    const float g = r.left;
    const float d = r.left + r.width;
    const float h = r.top;
    const float b = r.top + r.height;
    const sfVector2f hd[3] = {{d - longueur, h}, {d, h}, {d, h + longueur}};
    const sfVector2f bg[3] = {{g, b - longueur}, {g, b}, {g + longueur, b}};

    ligne_epaisse(app, hd, 3, 2.f, c, 0);
    ligne_epaisse(app, bg, 3, 2.f, c, 0);
}

void losange(app_t *app, sfVector2f centre, float r, sfColor c)
{
    static const float pts[8] = {0.f, -1.f, 1.f, 0.f, 0.f, 1.f, -1.f, 0.f};

    polygone(app, pts, 4, centre, r, 0.f, c);
}

/** Hexagone (pointe en haut quand rotation = 0), plein et/ou cerclé. */
void hexagone(app_t *app, sfVector2f centre, float r, float rotation,
    sfColor fond, sfColor bord)
{
    float pts[12];
    sfVector2f contour[7];
    float a = 0.f;

    for (int k = 0; k < 6; k++) {
        a = rotation - PI / 2.f + k * PI / 3.f;
        pts[2 * k] = cosf(a);
        pts[2 * k + 1] = sinf(a);
        contour[k] = (sfVector2f){centre.x + cosf(a) * r, centre.y + sinf(a) * r};
    }
    contour[6] = contour[0];
    if (fond.a > 0)
        polygone(app, pts, 6, centre, r, 0.f, fond);
    if (bord.a > 0)
        ligne_epaisse(app, contour, 7, 1.6f, bord, 0);
}

/**
 * « 01  NOM ─────────◆ » : numéro, libellé, puis filet jusqu'à `largeur`.
 * Sans numéro (`num` à NULL), un losange le remplace.
 */
void titre_section(app_t *app, const char *num, const char *label,
    sfVector2f pos, float largeur, sfColor accent)
{
    float x = pos.x;
    sfVertex filet[2];

    if (num)
        x += etiquette(app, num, pos, accent, ALIGNE_GAUCHE) + 10.f;
    else
        losange(app, (sfVector2f){x + 4.f, pos.y}, 4.f, accent);
    x += num ? 0.f : 18.f;
    x += etiquette(app, label, (sfVector2f){x, pos.y}, C_TEXTE_DOUX, ALIGNE_GAUCHE)
        + 14.f;
    if (x >= pos.x + largeur - 12.f)
        return;
    filet[0] = (sfVertex){{x, pos.y + 0.5f}, avec_alpha(C_BLANC, 0.14f * app->alpha),
        {0, 0}};
    filet[1] = (sfVertex){{pos.x + largeur - 10.f, pos.y + 0.5f},
        avec_alpha(C_BLANC, 0.03f * app->alpha), {0, 0}};
    cible_primitives(app, filet, 2, sfLines, NULL);
    losange(app, (sfVector2f){pos.x + largeur - 4.f, pos.y}, 3.f,
        avec_alpha(accent, 0.7f));
}

/**
 * Touche de clavier biseautée ; `allume` (0..1) la remplit de `c`.
 * Un libellé qui commence par un symbole (flèches…) utilise la police de
 * symboles, la seule qui les contient.
 */
float largeur_touche(app_t *app, const char *s)
{
    police_t p = (unsigned char)s[0] == 0xE2 ? POLICE_SYMBOLES : POLICE_GRAS;

    return fmaxf(26.f, largeur_texte(app, p, s, 12) + 16.f);
}

float touche_clavier(app_t *app, const char *s, sfVector2f centre, sfColor c,
    float allume)
{
    police_t p = (unsigned char)s[0] == 0xE2 ? POLICE_SYMBOLES : POLICE_GRAS;
    float w = largeur_touche(app, s);
    sfFloatRect r = {centre.x - w / 2.f, centre.y - 13.f, w, 26.f};

    panneau(app, r, 6.f, melanger(avec_alpha(C_BLANC, 0.07f), c, allume),
        melanger(avec_alpha(C_BLANC, 0.03f), melanger(c, C_NOIR, 0.25f), allume),
        melanger(avec_alpha(C_BLANC, 0.18f), melanger(c, C_BLANC, 0.3f), allume));
    texte(app, p, s, 12, centre, melanger(C_TEXTE_DOUX, C_FOND, allume),
        ALIGNE_CENTRE);
    return w;
}
