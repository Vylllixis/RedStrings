/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Illustration de la carte découpée en hexagone : remplace l'emblème de la
 * classe partout où il apparaît (jetons de l'arène, fiches, menu, ordre des
 * tours) quand le mode Yu-Gi-Oh! est actif et que l'image a été chargée.
 */

#include <math.h>
#include "personnage.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "gui_yugioh.h"

/* Part de l'illustration gardée autour de son centre (0.5 = toute l'image). */
#define CADRAGE 0.43f
/* Le sujet des illustrations est souvent un peu au-dessus du centre. */
#define CENTRE_V 0.46f

int yugioh_portrait(app_t *app, classe_t c, sfVector2f centre, float rayon)
{
    const sfTexture *art = NULL;
    sfVector2u t;
    sfVertex v[8];
    sfRenderStates etats = {sfBlendAlpha, sfTransform_Identity, NULL, NULL};
    sfColor col;
    float a = 0.f;

    if (!yugioh_actif(app) || (unsigned)c >= NB_CLASSES || app->yugioh.art[c] == NULL)
        return 0;
    art = app->yugioh.art[c];
    t = sfTexture_getSize(art);
    etats.texture = art;
    col = avec_alpha(melanger(C_BLANC, (sfColor){110, 110, 124, 255}, app->desature),
        app->alpha);
    v[0] = (sfVertex){centre, col, {0.5f * t.x, CENTRE_V * t.y}};
    for (int k = 0; k <= 6; k++) {
        a = -PI / 2.f + k * PI / 3.f;
        v[k + 1] = (sfVertex){{centre.x + cosf(a) * rayon, centre.y + sinf(a) * rayon}, col,
            {(0.5f + cosf(a) * CADRAGE) * t.x, (CENTRE_V + sinf(a) * CADRAGE) * t.y}};
    }
    cible_primitives(app, v, 8, sfTriangleFan, &etats);
    hexagone(app, centre, rayon, 0.f, C_TRANSPARENT, avec_alpha((sfColor){232, 206, 128,
        255}, 0.8f * (1.f - 0.6f * app->desature)));
    return 1;
}
