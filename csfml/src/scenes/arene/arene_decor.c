/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Arène : décor (stade, plateformes, faisceaux, fil rouge entre les camps).
 */

#include <math.h>
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_ecran.h"
#include "gui_etat.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "arene_interne.h"

void arene_disque(app_t *app, sfVector2f c, sfVector2f r, sfColor centre,
    sfColor bord)
{
    sfVertex v[34];
    float a = 0.f;

    v[0] = (sfVertex){c, avec_alpha(centre, app->alpha), {0, 0}};
    for (int i = 0; i <= 32; i++) {
        a = i * 2.f * PI / 32.f;
        v[i + 1] = (sfVertex){{c.x + cosf(a) * r.x, c.y + sinf(a) * r.y},
            avec_alpha(bord, app->alpha), {0, 0}};
    }
    cible_primitives(app, v, 34, sfTriangleFan, NULL);
}

/** Anneau d'ellipse en tirets qui tourne (sol des plateformes). */
static void anneau_tirets(app_t *app, sfVector2f c, sfVector2f r, float rot,
    sfColor col)
{
    sfVector2f p[6];
    float a = 0.f;

    for (int k = 0; k < 16; k++) {
        for (int i = 0; i < 6; i++) {
            a = rot + k * 2.f * PI / 16.f + i * 0.045f;
            p[i] = (sfVector2f){c.x + cosf(a) * r.x, c.y + sinf(a) * r.y};
        }
        ligne_epaisse(app, p, 6, 2.f, col, 1);
    }
}

static void plateforme(app_t *app, sfVector2f c, sfVector2f r, sfColor bord,
    float apparition)
{
    sfVector2f rr = {r.x * apparition, r.y * apparition};

    lueur(app, c, (sfVector2f){rr.x * 1.3f, rr.y * 2.6f}, avec_alpha(bord, 0.16f));
    arene_disque(app, (sfVector2f){c.x, c.y + rr.y * 0.22f}, (sfVector2f){rr.x * 1.02f,
        rr.y * 1.1f}, (sfColor){4, 5, 12, 200}, (sfColor){4, 5, 12, 0});
    arene_disque(app, c, rr, (sfColor){26, 32, 64, 235}, (sfColor){12, 15, 32, 235});
    ellipse(app, c, rr, 2.f, avec_alpha(bord, 0.85f));
    ellipse(app, c, (sfVector2f){rr.x * 0.72f, rr.y * 0.72f}, 1.f,
        avec_alpha(bord, 0.3f));
    anneau_tirets(app, c, (sfVector2f){rr.x * 0.86f, rr.y * 0.86f},
        app->temps * 0.12f, avec_alpha(bord, 0.35f));
}

/** Cône de lumière venu du haut, posé sur une plateforme. */
static void faisceau(app_t *app, sfVector2f c, float largeur, sfColor col)
{
    sfVertex v[4] = {
        {{c.x - largeur * 0.35f, 0.f}, avec_alpha(col, 0.f), {0, 0}},
        {{c.x + largeur * 0.35f, 0.f}, avec_alpha(col, 0.f), {0, 0}},
        {{c.x + largeur, c.y}, avec_alpha(col, 0.09f * app->alpha), {0, 0}},
        {{c.x - largeur, c.y}, avec_alpha(col, 0.09f * app->alpha), {0, 0}},
    };

    cible_primitives(app, v, 4, sfQuads, &app->additif);
}

/** Le fil rouge qui relie les deux camps, parcouru d'impulsions. */
static void fil_des_camps(app_t *app, float e)
{
    sfVector2f p[40];
    sfVector2f a = {SOL_J_X + PLAT_J_RX * 0.55f, SOL_J_Y - PLAT_J_RY * 0.3f};
    sfVector2f b = {SOL_A_X - PLAT_A_RX * 0.6f, SOL_A_Y + PLAT_A_RY * 0.4f};
    sfVector2f m = {(a.x + b.x) / 2.f, (a.y + b.y) / 2.f - 40.f};
    float u = 0.f;
    int k = 0;

    for (int i = 0; i < 40; i++) {
        u = i / 39.f * e;
        p[i] = (sfVector2f){(1 - u) * (1 - u) * a.x + 2 * (1 - u) * u * m.x + u * u * b.x,
            (1 - u) * (1 - u) * a.y + 2 * (1 - u) * u * m.y + u * u * b.y
            + sinf(u * 9.f + app->temps * 2.f) * 4.f};
    }
    ligne_epaisse(app, p, 40, 9.f, avec_alpha(C_ROUGE, 0.08f), 1);
    ligne_epaisse(app, p, 40, 1.6f, avec_alpha(C_ROUGE, 0.55f), 1);
    for (int i = 0; i < 2; i++) {
        k = (int)(fmodf(app->temps * 0.45f + i * 0.5f, 1.f) * 39.f);
        lueur(app, p[k], (sfVector2f){22.f, 22.f}, avec_alpha(C_ROUGE, 0.7f * e));
    }
}

/** Gradins lointains : anneau du stade et piliers de lumière. */
static void stade(app_t *app)
{
    sfVector2f c = {LARGEUR * 0.5f, HAUTEUR * 0.33f};
    sfVertex pilier[4];
    float x = 0.f;
    float y = 0.f;
    float a = 0.f;

    ellipse(app, c, (sfVector2f){LARGEUR * 0.62f, HAUTEUR * 0.2f}, 1.5f,
        avec_alpha(C_BLANC, 0.07f));
    ellipse(app, c, (sfVector2f){LARGEUR * 0.56f, HAUTEUR * 0.16f}, 1.f,
        avec_alpha(C_ROUGE, 0.12f));
    for (int k = 0; k < 9; k++) {
        a = PI + (k + 0.5f) * PI / 9.f;
        x = c.x + cosf(a) * LARGEUR * 0.62f;
        y = c.y + sinf(a) * HAUTEUR * 0.2f;
        pilier[0] = (sfVertex){{x - 6.f, y - 120.f}, avec_alpha(C_CYAN, 0.f), {0, 0}};
        pilier[1] = (sfVertex){{x + 6.f, y - 120.f}, avec_alpha(C_CYAN, 0.f), {0, 0}};
        pilier[2] = (sfVertex){{x + 6.f, y}, avec_alpha(C_CYAN, (0.05f + 0.04f
            * sinf(app->temps * 1.3f + k)) * app->alpha), {0, 0}};
        pilier[3] = (sfVertex){{x - 6.f, y}, pilier[2].color, {0, 0}};
        cible_primitives(app, pilier, 4, sfQuads, &app->additif);
    }
}

/** Voile qui assombrit le fond : il couvre toute la fenêtre, quelle que soit sa forme. */
void arene_voile(app_t *app)
{
    sfFloatRect r = ecran_visible(app);
    sfVertex voile[4] = {
        {{r.left, r.top}, (sfColor){4, 5, 12, 170}, {0, 0}},
        {{r.left + r.width, r.top}, (sfColor){4, 5, 12, 170}, {0, 0}},
        {{r.left + r.width, r.top + r.height}, (sfColor){4, 5, 12, 60}, {0, 0}},
        {{r.left, r.top + r.height}, (sfColor){4, 5, 12, 60}, {0, 0}},
    };

    cible_primitives(app, voile, 4, sfQuads, NULL);
}

void arene_dessiner_arene(app_t *app, arene_t *ar)
{
    float intro = ar->etat == VUE_INTRO ? ar->t_etat : DUREE_INTRO;
    float e = sortie_cubique(progression(intro, 0.f, 0.7f));
    float a = app->alpha;

    app->alpha = a * e;
    stade(app);
    faisceau(app, (sfVector2f){SOL_A_X, SOL_A_Y}, PLAT_A_RX, C_BLANC);
    faisceau(app, (sfVector2f){SOL_J_X, SOL_J_Y}, PLAT_J_RX, C_CYAN);
    plateforme(app, (sfVector2f){SOL_A_X, SOL_A_Y}, (sfVector2f){PLAT_A_RX, PLAT_A_RY},
        C_ROUGE, 0.6f + 0.4f * e);
    plateforme(app, (sfVector2f){SOL_J_X, SOL_J_Y}, (sfVector2f){PLAT_J_RX, PLAT_J_RY},
        C_CYAN, 0.6f + 0.4f * e);
    fil_des_camps(app, sortie_cubique(progression(intro, 0.4f, 0.6f)));
    app->alpha = a;
}
