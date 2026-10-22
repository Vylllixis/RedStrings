/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ fond.c ─────────────────────────────────────────────────────────────
** │ But         Le fond animé commun à tous les écrans : dégradé,
** │             aurores, fils rouges, étoiles, sol, poussière.
** │ Contenu     degrade, aurores, y_fil, fils_rouges, etoiles, sol,
** │             poussieres, dessiner_fond (appelée à chaque image par
** │             maj_app)
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_ecran.h"
#include "gui_types.h"

static void degrade(app_t *app)
{
    float t = app->temps * 0.15f;
    sfColor haut = {(sfUint8)(10 + 3 * sinf(t)), 12, (sfUint8)(28 + 4 * cosf(t)), 255};
    sfColor bas = {(sfUint8)(18 + 4 * cosf(t * 1.3f)), 10, 30, 255};
    sfFloatRect e = ecran_visible(app);
    sfVertex v[4] = {
        {{e.left, e.top}, haut, {0, 0}},
        {{e.left + e.width, e.top}, melanger(haut, bas, 0.3f), {0, 0}},
        {{e.left, e.top + e.height}, bas, {0, 0}},
        {{e.left + e.width, e.top + e.height}, melanger(bas, haut, 0.4f), {0, 0}}
    };

    cible_primitives(app, v, 4, sfTriangleStrip, NULL);
}

static void aurores(app_t *app)
{
    float t = app->temps;

    lueur(app, (sfVector2f){340 + 180 * sinf(t * 0.13f), 220 + 90 * cosf(t * 0.17f)},
        (sfVector2f){620, 420}, avec_alpha(C_CYAN, 0.10f));
    lueur(app, (sfVector2f){980 + 160 * cosf(t * 0.11f), 520 + 80 * sinf(t * 0.19f)},
        (sfVector2f){640, 440}, avec_alpha(C_VIOLET, 0.08f));
    lueur(app, (sfVector2f){700 + 260 * sinf(t * 0.07f), 380 + 60 * sinf(t * 0.23f)},
        (sfVector2f){560, 300}, avec_alpha(C_ROUGE, 0.07f));
}

#define HORIZON 470.f

#define FUITE_Y 250.f

/** Hauteur du fil rouge `f` à l'abscisse `x`. */
static float y_fil(int f, float x, float t)
{
    return 190.f + f * 190.f
        + sinf(x * 0.0042f + t * (0.35f + f * 0.08f) + f * 2.1f) * (46.f + f * 10.f)
        + sinf(x * 0.011f - t * 0.6f + f) * 14.f;
}

/** Les « fils rouges » qui ondulent, parcourus par une impulsion d'énergie. */
static void fils_rouges(app_t *app)
{
    sfVector2f pts[90];
    sfFloatRect e = ecran_visible(app);
    float t = app->temps;
    float x = 0.f;

    for (int f = 0; f < 3; f++) {
        for (int i = 0; i < 90; i++) {
            x = e.left - 40.f + i * (e.width + 80.f) / 89.f;
            pts[i] = (sfVector2f){x, y_fil(f, x, t)};
        }
        ligne_epaisse(app, pts, 90, 14.f, avec_alpha(C_ROUGE, 0.03f), 1);
        ligne_epaisse(app, pts, 90, 4.f, avec_alpha(C_ROUGE, 0.06f), 1);
        ligne_epaisse(app, pts, 90, 1.4f, avec_alpha(C_ROUGE, 0.26f), 1);
        x = e.left + fmodf(t * (90.f + f * 35.f) + f * 430.f, e.width + 400.f) - 200.f;
        lueur(app, (sfVector2f){x, y_fil(f, x, t)}, (sfVector2f){90.f, 26.f},
            avec_alpha(C_ROUGE, 0.30f));
        lueur(app, (sfVector2f){x, y_fil(f, x, t)}, (sfVector2f){12.f, 12.f},
            avec_alpha(C_BLANC, 0.5f));
    }
}

/** Plan lointain : étoiles fixes qui scintillent et dérivent lentement. */
static void etoiles(app_t *app)
{
    sfVertex v[NB_ETOILES * 4];
    poussiere_t *e = NULL;
    sfVector2f p;
    float s = 0.f;
    sfColor c;

    for (int i = 0; i < NB_ETOILES; i++) {
        e = &app->etoiles[i];
        e->x -= e->vitesse * app->dt;
        if (e->x < -4.f)
            e->x = LARGEUR + 4.f;
        s = e->taille;
        p = vers_ecran(app, e->x, e->y);
        c = avec_alpha(i % 4 == 0 ? C_CYAN : C_BLANC, (0.18f + 0.3f
            * (0.5f + 0.5f * sinf(app->temps * (0.6f + e->phase * 0.3f) + e->phase * 7.f)))
            * (p.y < HORIZON ? 1.f : 0.25f));
        v[4 * i] = (sfVertex){{p.x, p.y}, c, {0, 0}};
        v[4 * i + 1] = (sfVertex){{p.x + s, p.y}, c, {0, 0}};
        v[4 * i + 2] = (sfVertex){{p.x + s, p.y + s}, c, {0, 0}};
        v[4 * i + 3] = (sfVertex){{p.x, p.y + s}, c, {0, 0}};
    }
    cible_primitives(app, v, NB_ETOILES * 4, sfQuads, NULL);
}

/**
 * Sol quadrillé en perspective sous l'horizon : il donne la profondeur
 * de la scène. Les lignes horizontales avancent vers le joueur.
 */
static void sol(app_t *app)
{
    sfVertex v[2 * 48];
    sfFloatRect e = ecran_visible(app);
    float bas = e.top + e.height;
    float k = (HORIZON - FUITE_Y) / (bas - FUITE_Y);
    float z = 0.f;
    float y = 0.f;
    int n = 0;

    for (int i = -14; i <= 14; i++) {
        v[n++] = (sfVertex){{LARGEUR / 2.f + i * 150.f * k, HORIZON},
            avec_alpha(C_CYAN, 0.f), {0, 0}};
        v[n++] = (sfVertex){{LARGEUR / 2.f + i * 150.f, bas},
            avec_alpha(C_CYAN, 0.09f), {0, 0}};
    }
    for (int i = 0; i < 9; i++) {
        z = fmodf(i / 9.f + app->temps * 0.045f, 1.f);
        y = HORIZON + (bas - HORIZON) * z * z;
        v[n++] = (sfVertex){{e.left, y}, avec_alpha(C_CYAN, 0.10f * z), {0, 0}};
        v[n++] = (sfVertex){{e.left + e.width, y}, avec_alpha(C_CYAN, 0.10f * z), {0, 0}};
    }
    cible_primitives(app, v, n, sfLines, NULL);
    lueur(app, (sfVector2f){LARGEUR / 2.f, HORIZON}, (sfVector2f){760.f, 46.f},
        avec_alpha(C_ROUGE, 0.10f));
    lueur(app, (sfVector2f){LARGEUR / 2.f, HORIZON}, (sfVector2f){420.f, 6.f},
        avec_alpha(C_ROUGE, 0.35f));
}

static void poussieres(app_t *app)
{
    poussiere_t *p = NULL;
    float scintille = 0.f;

    for (int i = 0; i < NB_POUSSIERES; i++) {
        p = &app->poussieres[i];
        p->y -= p->vitesse * app->dt;
        p->x += sinf(app->temps * 0.5f + p->phase) * 8.f * app->dt;
        if (p->y < -10.f) {
            p->y = HAUTEUR + 10.f;
            p->x = aleatoire(0.f, LARGEUR);
        }
        scintille = 0.35f + 0.65f * (0.5f + 0.5f * sinf(app->temps * 1.7f + p->phase * 3.f));
        lueur(app, vers_ecran(app, p->x, p->y), (sfVector2f){p->taille * 3.f,
            p->taille * 3.f}, avec_alpha(i % 3 == 0 ? C_ROUGE : C_CYAN,
            0.35f * scintille));
    }
}

void dessiner_fond(app_t *app)
{
    degrade(app);
    etoiles(app);
    aurores(app);
    sol(app);
    fils_rouges(app);
    poussieres(app);
    sfSprite_setTexture(app->sprite, app->tex_vignette, sfTrue);
    sfSprite_setOrigin(app->sprite, (sfVector2f){0, 0});
    sfSprite_setPosition(app->sprite, (sfVector2f){app->ecran.left, app->ecran.top});
    sfSprite_setScale(app->sprite, (sfVector2f){app->ecran.width / 256.f,
        app->ecran.height / 256.f});
    sfSprite_setColor(app->sprite, C_BLANC);
    cible_sprite(app, app->sprite, NULL);
}
