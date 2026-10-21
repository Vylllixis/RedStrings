/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Les 20 emblèmes vectoriels des classes. La classe ne choisit pas son
 * dessin ici : elle porte un `embleme_t` dans la table classes[], et ce
 * fichier associe chaque emblème à une fonction de dessin.
 * Coordonnées unitaires : (0, 0) au centre, y vers le bas, rayon ≈ 1.
 */

#include <math.h>
#include "gui.h"

typedef void (*dessin_embleme_t)(app_t *app, sfVector2f c, float t, sfColor col);

static const sfColor TROU = {12, 14, 28, 255};

static sfVector2f pt(sfVector2f c, float t, float x, float y)
{
    return (sfVector2f){c.x + x * t, c.y + y * t};
}

/** Épaisseur proportionnelle, jamais sous 1,5 px pour rester lisible en petit. */
static float ep(float t, float f)
{
    return fmaxf(1.5f, f * t);
}

static void trait(app_t *app, sfVector2f c, float t, const float xy[4], float e,
    sfColor col)
{
    sfVector2f p[2] = {pt(c, t, xy[0], xy[1]), pt(c, t, xy[2], xy[3])};

    ligne_epaisse(app, p, 2, ep(t, e), col, 0);
}

static void epee(app_t *app, sfVector2f c, float t, float angle, sfColor col)
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

static void etoile(app_t *app, sfVector2f c, float t, float angle, sfColor col)
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

static void d_epees(app_t *app, sfVector2f c, float t, sfColor col)
{
    epee(app, c, t, -0.72f, col);
    epee(app, c, t, 0.72f, col);
}

static void d_lame(app_t *app, sfVector2f c, float t, sfColor col)
{
    epee(app, c, t * 1.1f, 0.62f, col);
}

static void d_bouclier(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float ECU[] = {-0.68f, -0.78f, 0.68f, -0.78f, 0.68f, 0.05f, 0.f, 0.9f,
        -0.68f, 0.05f};
    static const float CREUX[] = {-0.48f, -0.6f, 0.48f, -0.6f, 0.48f, 0.f, 0.f, 0.64f,
        -0.48f, 0.f};
    static const float BARRES[2][4] = {{0.f, -0.6f, 0.f, 0.6f}, {-0.48f, -0.18f,
        0.48f, -0.18f}};

    polygone(app, ECU, 5, c, t, 0.f, col);
    polygone(app, CREUX, 5, c, t, 0.f, melanger(col, TROU, 0.7f));
    for (int k = 0; k < 2; k++)
        trait(app, c, t, BARRES[k], 0.13f, col);
}

static void d_hache(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float MANCHE[4] = {-0.05f, 0.92f, 0.05f, -0.92f};
    static const float FER[] = {0.05f, -0.8f, 0.55f, -0.95f, 0.82f, -0.45f, 0.55f,
        0.05f, 0.05f, -0.15f};
    static const float POINTE[] = {-0.05f, -0.7f, -0.45f, -0.52f, -0.05f, -0.34f};

    trait(app, c, t, MANCHE, 0.13f, col);
    polygone(app, FER, 5, c, t, 0.f, col);
    polygone(app, POINTE, 3, c, t, 0.f, col);
}

static void d_lance(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float HAMPE[4] = {-0.62f, 0.86f, 0.4f, -0.46f};
    static const float FER[] = {0.f, -1.f, 0.36f, 0.f, 0.f, 0.42f, -0.36f, 0.f};

    trait(app, c, t, HAMPE, 0.11f, col);
    polygone(app, FER, 4, pt(c, t, 0.52f, -0.62f), 0.42f * t, 0.66f, col);
}

static void d_etoile(app_t *app, sfVector2f c, float t, sfColor col)
{
    etoile(app, pt(c, t, -0.08f, 0.08f), t * 0.85f, sinf(app->temps * 0.8f) * 0.12f,
        col);
    etoile(app, pt(c, t, 0.62f, -0.6f), t * 0.3f, app->temps * 0.9f, col);
    cercle(app, pt(c, t, -0.62f, -0.56f), 0.08f * t, avec_alpha(col, 0.5f + 0.5f
        * sinf(app->temps * 3.f)), C_TRANSPARENT, 0.f);
}

/** Goutte de flamme : pointe en haut, base arrondie. */
static void flamme(app_t *app, sfVector2f c, float t, sfColor col)
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

static void d_flamme(app_t *app, sfVector2f c, float t, sfColor col)
{
    float s = 1.f + 0.05f * sinf(app->temps * 9.f);

    flamme(app, c, t * s, col);
    flamme(app, pt(c, t, 0.f, 0.22f), t * 0.5f, melanger(col, C_BLANC, 0.6f));
}

static void d_crane(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float MACHOIRE[] = {-0.34f, 0.2f, 0.34f, 0.2f, 0.3f, 0.72f, -0.3f,
        0.72f};
    static const float NEZ[] = {0.f, 0.06f, 0.09f, 0.26f, -0.09f, 0.26f};
    static const float DENTS[3][4] = {{-0.12f, 0.46f, -0.12f, 0.72f}, {0.f, 0.46f, 0.f,
        0.72f}, {0.12f, 0.46f, 0.12f, 0.72f}};

    cercle(app, pt(c, t, 0.f, -0.18f), 0.62f * t, col, C_TRANSPARENT, 0.f);
    polygone(app, MACHOIRE, 4, c, t, 0.f, col);
    cercle(app, pt(c, t, -0.25f, -0.14f), 0.17f * t, TROU, C_TRANSPARENT, 0.f);
    cercle(app, pt(c, t, 0.25f, -0.14f), 0.17f * t, TROU, C_TRANSPARENT, 0.f);
    polygone(app, NEZ, 3, c, t, 0.f, TROU);
    for (int k = 0; k < 3; k++)
        trait(app, c, t, DENTS[k], 0.05f, TROU);
}

static void d_portail(app_t *app, sfVector2f c, float t, sfColor col)
{
    hexagone(app, c, 0.88f * t, app->temps * 0.6f, C_TRANSPARENT, col);
    hexagone(app, c, 0.58f * t, -app->temps, C_TRANSPARENT, avec_alpha(col, 0.75f));
    lueur(app, c, (sfVector2f){0.9f * t, 0.9f * t}, avec_alpha(col, 0.5f));
    cercle(app, c, 0.2f * t, melanger(col, C_BLANC, 0.5f), C_TRANSPARENT, 0.f);
}

static void d_feuille(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float NERVURE[4] = {-0.5f, 0.68f, 0.5f, -0.68f};
    float pts[36];
    float u = 0.f;

    for (int k = 0; k < 9; k++) {
        u = k * PI / 8.f;
        pts[2 * k] = 0.42f * sinf(u);
        pts[2 * k + 1] = -0.92f * cosf(u);
        pts[34 - 2 * k] = -0.42f * sinf(u);
        pts[35 - 2 * k] = -0.92f * cosf(u);
    }
    polygone(app, pts, 18, c, t, 0.62f, col);
    trait(app, c, t, NERVURE, 0.06f, melanger(col, TROU, 0.6f));
}

static void d_lune(app_t *app, sfVector2f c, float t, sfColor col)
{
    arc_cercle(app, c, 0.6f * t, ep(t, 0.3f), (sfVector2f){0.95f, 5.33f}, col, 0);
    losange(app, pt(c, t, 0.46f, -0.34f), 0.16f * t, melanger(col, C_BLANC, 0.4f));
}

static void d_sablier(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float HAUT[] = {-0.55f, -0.74f, 0.55f, -0.74f, 0.f, 0.f};
    static const float BAS[] = {0.f, 0.f, 0.55f, 0.74f, -0.55f, 0.74f};
    static const float BARRES[2][4] = {{-0.72f, -0.84f, 0.72f, -0.84f}, {-0.72f, 0.84f,
        0.72f, 0.84f}};

    polygone(app, HAUT, 3, c, t, 0.f, avec_alpha(col, 0.45f));
    polygone(app, BAS, 3, c, t, 0.f, col);
    for (int k = 0; k < 2; k++)
        trait(app, c, t, BARRES[k], 0.14f, col);
}

static void d_arc(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float POINTE[] = {0.88f, -0.88f, 0.739f, -0.541f, 0.541f, -0.739f};
    static const float PLUME_A[] = {-0.378f, 0.378f, -0.236f, 0.236f, -0.215f, 0.399f,
        -0.342f, 0.526f};
    static const float PLUME_B[] = {-0.378f, 0.378f, -0.236f, 0.236f, -0.399f, 0.215f,
        -0.526f, 0.342f};
    sfVector2f bezier[24];
    sfVector2f corde[3] = {pt(c, t, 0.38f, 0.78f), pt(c, t, -0.42f, 0.42f),
        pt(c, t, -0.78f, -0.38f)};
    sfVector2f fleche[2] = {pt(c, t, -0.42f, 0.42f), pt(c, t, 0.66f, -0.66f)};
    float u = 0.f;

    for (int i = 0; i < 24; i++) {
        u = i / 23.f;
        bezier[i] = pt(c, t,
            (1 - u) * (1 - u) * 0.38f + 2 * (1 - u) * u * 0.507f + u * u * -0.78f,
            (1 - u) * (1 - u) * 0.78f + 2 * (1 - u) * u * -0.507f + u * u * -0.38f);
    }
    ligne_epaisse(app, corde, 3, fmaxf(1.f, 0.035f * t), avec_alpha(col, 0.75f), 0);
    ligne_epaisse(app, bezier, 24, ep(t, 0.11f), col, 0);
    ligne_epaisse(app, fleche, 2, ep(t, 0.075f), col, 0);
    polygone(app, POINTE, 3, c, t, 0.f, col);
    polygone(app, PLUME_A, 4, c, t, 0.f, col);
    polygone(app, PLUME_B, 4, c, t, 0.f, col);
}

static void d_cible(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float CROIX[4][4] = {{0.f, -1.f, 0.f, -0.25f}, {0.f, 0.25f, 0.f, 1.f},
        {-1.f, 0.f, -0.25f, 0.f}, {0.25f, 0.f, 1.f, 0.f}};

    arc_cercle(app, c, 0.76f * t, ep(t, 0.1f), (sfVector2f){0.f, 2.f * PI}, col, 0);
    arc_cercle(app, c, 0.44f * t, ep(t, 0.08f), (sfVector2f){0.f, 2.f * PI},
        avec_alpha(col, 0.7f), 0);
    cercle(app, c, 0.13f * t, col, C_TRANSPARENT, 0.f);
    for (int k = 0; k < 4; k++)
        trait(app, c, t, CROIX[k], 0.07f, col);
}

static void d_ancre(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float TIGE[4] = {0.f, -0.56f, 0.f, 0.74f};
    static const float JAS[4] = {-0.4f, -0.4f, 0.4f, -0.4f};
    static const float PATTE_D[] = {0.5f, 0.22f, 0.86f, 0.06f, 0.74f, 0.42f};
    static const float PATTE_G[] = {-0.5f, 0.22f, -0.74f, 0.42f, -0.86f, 0.06f};

    arc_cercle(app, pt(c, t, 0.f, -0.72f), 0.16f * t, ep(t, 0.09f),
        (sfVector2f){0.f, 2.f * PI}, col, 0);
    trait(app, c, t, TIGE, 0.12f, col);
    trait(app, c, t, JAS, 0.11f, col);
    arc_cercle(app, pt(c, t, 0.f, 0.05f), 0.7f * t, ep(t, 0.12f),
        (sfVector2f){0.35f, PI - 0.35f}, col, 0);
    polygone(app, PATTE_D, 3, c, t, 0.f, col);
    polygone(app, PATTE_G, 3, c, t, 0.f, col);
}

static void d_croix(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float VERTICALE[] = {-0.16f, -0.86f, 0.16f, -0.86f, 0.16f, 0.86f,
        -0.16f, 0.86f};
    static const float HORIZONTALE[] = {-0.62f, -0.4f, 0.62f, -0.4f, 0.62f, -0.08f,
        -0.62f, -0.08f};

    lueur(app, pt(c, t, 0.f, -0.24f), (sfVector2f){t, t}, avec_alpha(col, 0.35f));
    polygone(app, VERTICALE, 4, c, t, 0.f, col);
    polygone(app, HORIZONTALE, 4, c, t, 0.f, col);
}

static void d_note(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float HAMPE[4] = {0.f, 0.5f, 0.f, -0.84f};
    static const float FANION[] = {0.f, -0.86f, 0.52f, -0.56f, 0.46f, -0.3f, 0.f,
        -0.56f};

    cercle(app, pt(c, t, -0.24f, 0.52f), 0.27f * t, col, C_TRANSPARENT, 0.f);
    trait(app, c, t, HAMPE, 0.1f, col);
    polygone(app, FANION, 4, c, t, 0.f, col);
}

static void d_fiole(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float COL[] = {-0.15f, -0.74f, 0.15f, -0.74f, 0.15f, -0.18f, -0.15f,
        -0.18f};
    static const float BOUCHON[] = {-0.22f, -0.92f, 0.22f, -0.92f, 0.22f, -0.74f, -0.22f,
        -0.74f};

    cercle(app, pt(c, t, 0.f, 0.3f), 0.56f * t, avec_alpha(col, 0.22f), col,
        ep(t, 0.08f));
    cercle(app, pt(c, t, 0.f, 0.42f), 0.36f * t, col, C_TRANSPARENT, 0.f);
    polygone(app, COL, 4, c, t, 0.f, avec_alpha(col, 0.55f));
    polygone(app, BOUCHON, 4, c, t, 0.f, col);
    cercle(app, pt(c, t, 0.14f, 0.16f + 0.06f * sinf(app->temps * 3.f)), 0.06f * t,
        melanger(col, C_BLANC, 0.6f), C_TRANSPARENT, 0.f);
}

static void d_engrenage(app_t *app, sfVector2f c, float t, sfColor col)
{
    static const float DENT[] = {-0.13f, -0.9f, 0.13f, -0.9f, 0.17f, -0.55f, -0.17f,
        -0.55f};

    for (int k = 0; k < 8; k++)
        polygone(app, DENT, 4, c, t, app->temps * 0.4f + k * PI / 4.f, col);
    cercle(app, c, 0.62f * t, col, C_TRANSPARENT, 0.f);
    cercle(app, c, 0.24f * t, TROU, C_TRANSPARENT, 0.f);
}

static void d_orbe(app_t *app, sfVector2f c, float t, sfColor col)
{
    float a = 0.f;

    lueur(app, c, (sfVector2f){1.2f * t, 1.2f * t}, avec_alpha(col, 0.5f));
    cercle(app, c, 0.38f * t, col, C_TRANSPARENT, 0.f);
    ellipse(app, c, (sfVector2f){0.88f * t, 0.32f * t}, ep(t, 0.06f),
        avec_alpha(col, 0.8f));
    for (int k = 0; k < 3; k++) {
        a = app->temps * 1.4f + k * 2.f * PI / 3.f;
        losange(app, pt(c, t, cosf(a) * 0.88f, sinf(a) * 0.32f), 0.1f * t,
            melanger(col, C_BLANC, 0.5f));
    }
}

static const dessin_embleme_t DESSINS[NB_EMBLEMES] = {
    [EMB_EPEES] = d_epees,
    [EMB_BOUCLIER] = d_bouclier,
    [EMB_HACHE] = d_hache,
    [EMB_LAME] = d_lame,
    [EMB_LANCE] = d_lance,
    [EMB_ETOILE] = d_etoile,
    [EMB_FLAMME] = d_flamme,
    [EMB_CRANE] = d_crane,
    [EMB_PORTAIL] = d_portail,
    [EMB_FEUILLE] = d_feuille,
    [EMB_LUNE] = d_lune,
    [EMB_SABLIER] = d_sablier,
    [EMB_ARC] = d_arc,
    [EMB_CIBLE] = d_cible,
    [EMB_ANCRE] = d_ancre,
    [EMB_CROIX] = d_croix,
    [EMB_NOTE] = d_note,
    [EMB_FIOLE] = d_fiole,
    [EMB_ENGRENAGE] = d_engrenage,
    [EMB_ORBE] = d_orbe,
};

/** Emblème vectoriel de la classe, choisi par classes[c].embleme. */
void embleme(app_t *app, classe_t c, sfVector2f centre, float taille,
    sfColor couleur)
{
    DESSINS[classes[c].embleme](app, centre, taille, couleur);
}
