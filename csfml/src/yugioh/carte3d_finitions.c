/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ carte3d_finitions.c ────────────────────────────────────────────────
** │ But         Finitions des cartes en perspective : lumière qui court
** │             sur le cadre, et nom de la classe écrit en clair sur les
** │             petites cartes.
** │ Contenu     tour, carte3d_bord_lumineux, carte3d_titre
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include "personnage.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_types.h"
#include "gui_yugioh.h"
#include "yugioh_interne.h"

#define TOUR_CADRE 48

/** Point du tour du cadre, de 0 à 1 dans le sens horaire depuis le coin haut gauche. */
static sfVector2f tour(const carte3d_t *k, float t)
{
    const float m = 0.012f;
    float c = fmodf(t, 1.f) * 4.f;
    float f = c - floorf(c);

    if (c < 1.f)
        return carte3d_point(k, lerpf(m, 1.f - m, f), m);
    if (c < 2.f)
        return carte3d_point(k, 1.f - m, lerpf(m, 1.f - m, f));
    if (c < 3.f)
        return carte3d_point(k, lerpf(1.f - m, m, f), 1.f - m);
    return carte3d_point(k, m, lerpf(1.f - m, m, f));
}

/** Liseré lumineux, et un éclat qui fait le tour du cadre. */
void carte3d_bord_lumineux(app_t *app, const carte3d_t *k, float a)
{
    sfVector2f p[TOUR_CADRE + 1];
    sfVector2f eclat[10];
    sfColor col = k->halo.a ? k->halo : C_BLANC;
    float debut = fmodf(app->temps * 0.35f + k->centre.y * 0.002f, 1.f);

    col.a = 255;
    for (int i = 0; i <= TOUR_CADRE; i++)
        p[i] = tour(k, (float)i / TOUR_CADRE);
    ligne_epaisse(app, p, TOUR_CADRE + 1, 2.5f, avec_alpha(col, 0.45f * k->bord * a), 1);
    for (int i = 0; i < 10; i++)
        eclat[i] = tour(k, debut + i * 0.012f);
    ligne_epaisse(app, eclat, 10, 3.5f, avec_alpha(melanger(col, C_BLANC, 0.6f),
        k->bord * a), 1);
}

/**
 * Nom de la classe en clair sur une petite carte : la texture, trop réduite,
 * ne serait plus lisible. Seulement quand la carte est presque de face.
 */
void carte3d_titre(app_t *app, classe_t c, const carte3d_t *k)
{
    const carte_classe_t *carte = carte_de_classe(c);
    sfVector2f coins[4] = {carte3d_point(k, 0.06f, 0.044f), carte3d_point(k, 0.94f, 0.044f),
        carte3d_point(k, 0.94f, 0.122f), carte3d_point(k, 0.06f, 0.122f)};
    sfVector2f centre = carte3d_point(k, 0.5f, 0.083f);
    float hauteur = hypotf(coins[3].x - coins[0].x, coins[3].y - coins[0].y);
    float largeur = hypotf(coins[1].x - coins[0].x, coins[1].y - coins[0].y);
    sfColor fond = avec_alpha(fond_titre(carte->cadre), bornerf(k->alpha, 0.f, 1.f));
    sfVertex v[4];
    unsigned int taille = (unsigned int)fmaxf(7.f, hauteur * 0.7f);
    float rotation = app->rotation;
    char nom[48];

    if (!carte3d_face_visible(k) || fabsf(sinf(k->rot_y)) > 0.45f || k->alpha < 0.05f)
        return;
    fond.a = (sfUint8)(fond.a * app->alpha);
    for (int i = 0; i < 4; i++)
        v[i] = (sfVertex){coins[i], fond, {0, 0}};
    cible_primitives(app, v, 4, sfQuads, NULL);
    while (taille > 7 && largeur_texte(app, POLICE_GRAS, classe_info(c)->nom, taille)
        > largeur - 4.f)
        taille--;
    tronquer(app, POLICE_GRAS, taille, classe_info(c)->nom, largeur - 2.f, nom, sizeof(nom));
    app->rotation = k->rot_z * 180.f / PI;
    texte(app, POLICE_GRAS, nom, taille, centre, avec_alpha(encre_cadre(carte->cadre),
        bornerf(k->alpha, 0.f, 1.f)), ALIGNE_CENTRE);
    app->rotation = rotation;
}
