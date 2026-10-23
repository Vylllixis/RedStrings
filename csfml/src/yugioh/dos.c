/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ dos.c ──────────────────────────────────────────────────────────────
** │ But         Dos commun à toutes les cartes du mode Yu-Gi-Oh! :
** │             tourbillon brun et orangé, ovale noir et logo RedStrings
** │             (pas le logo officiel du jeu).
** │ Contenu     ovale, dessiner_dos
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "gui_yugioh.h"
#include "yugioh_interne.h"

/** Ovale plein, dégradé du centre vers le bord. */
static void ovale(app_t *app, sfVector2f c, sfVector2f r, sfColor centre, sfColor bord)
{
    sfVertex v[42];
    float a = 0.f;

    v[0] = (sfVertex){c, centre, {0, 0}};
    for (int i = 0; i <= 40; i++) {
        a = i * 2.f * PI / 40.f;
        v[i + 1] = (sfVertex){{c.x + cosf(a) * r.x, c.y + sinf(a) * r.y}, bord, {0, 0}};
    }
    cible_primitives(app, v, 42, sfTriangleFan, NULL);
}

/** Dos commun : tourbillon brun et orangé, ovale noir, logo RedStrings. */
void dessiner_dos(app_t *app)
{
    sfVector2f c = {FACE_L / 2.f, FACE_H / 2.f};
    sfVector2f fil[24];
    float u = 0.f;

    if (app == NULL)
        return;

    rect_arrondi(app, (sfFloatRect){0.f, 0.f, FACE_L, FACE_H}, 14.f,
        (sfColor){16, 9, 4, 255});
    yugioh_degrade(app, (sfFloatRect){10.f, 10.f, FACE_L - 20.f, FACE_H - 20.f},
        (sfColor){70, 34, 14, 255}, (sfColor){30, 14, 6, 255});
    for (int k = 0; k < 11; k++)
        ellipse(app, c, (sfVector2f){160.f - k * 12.f, 236.f - k * 17.f}, 3.f,
            k % 2 ? (sfColor){120, 50, 20, 170} : (sfColor){214, 110, 44, 150});
    ovale(app, c, (sfVector2f){92.f, 134.f}, (sfColor){8, 6, 6, 255},
        (sfColor){24, 12, 8, 255});
    ellipse(app, c, (sfVector2f){92.f, 134.f}, 3.f, OR_CADRE);
    for (int i = 0; i < 24; i++) {
        u = i / 23.f;
        fil[i] = (sfVector2f){c.x - 70.f + 140.f * u, c.y + 4.f + sinf(u * 2.f * PI) * 8.f};
    }
    ligne_epaisse(app, fil, 24, 3.f, C_ROUGE, 0);
    texte(app, POLICE_GRAS, "RED", 40, (sfVector2f){c.x, c.y - 34.f}, C_ROUGE,
        ALIGNE_CENTRE);
    texte(app, POLICE_GRAS, "STRINGS", 25, (sfVector2f){c.x, c.y + 38.f}, C_TEXTE,
        ALIGNE_CENTRE);
    for (int i = 0; i < 4; i++)
        losange(app, (sfVector2f){i % 2 ? FACE_L - 28.f : 28.f, i / 2 ? FACE_H - 28.f
            : 28.f}, 6.f, OR_CADRE);
}
