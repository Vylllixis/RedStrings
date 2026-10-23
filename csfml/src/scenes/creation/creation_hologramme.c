/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ creation_hologramme.c ──────────────────────────────────────────────
** │ But         Création : hologramme de la classe, avec une aura propre
** │             à chaque catégorie.
** │ Contenu     aura_guerrier, aura_mage, aura_archer, aura_soutien,
** │             projecteur, creation_suivre_classe, creation_hologramme
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include "personnage.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_emblemes.h"
#include "gui_etat.h"
#include "gui_hud.h"
#include "gui_particules.h"
#include "gui_types.h"
#include "creation_interne.h"

#define SOCLE_Y 338.f

/** Corps à corps : éclats de lame en couronne, pointés vers l'extérieur. */
static void aura_guerrier(app_t *app, sfVector2f c, sfColor col, float t)
{
    static const float ECLAT[6] = {0.f, -1.f, 0.28f, 0.6f, -0.28f, 0.6f};
    float a = 0.f;
    float r = 0.f;

    for (int k = 0; k < 8; k++) {
        a = t * 0.18f + k * PI / 4.f;
        r = 104.f + 6.f * sinf(t * 2.f + k);
        polygone(app, ECLAT, 3, (sfVector2f){c.x + cosf(a) * r, c.y + sinf(a) * r},
            k % 2 ? 9.f : 14.f, a + PI / 2.f, avec_alpha(col, k % 2 ? 0.45f : 0.8f));
    }
}

/** Magie : cercle runique en pointillés et astres en orbite inclinée. */
static void aura_mage(app_t *app, sfVector2f c, sfColor col, float t)
{
    float a = 0.f;
    float z = 0.f;
    sfVector2f p;

    for (int k = 0; k < 12; k++) {
        a = -t * 0.3f + k * PI / 6.f;
        arc_cercle(app, c, 122.f, 1.6f, (sfVector2f){a, a + 0.32f},
            avec_alpha(col, 0.55f), 1);
    }
    for (int k = 0; k < 5; k++) {
        a = t * 0.9f + k * 2.f * PI / 5.f;
        z = 0.5f + 0.5f * sinf(a);
        p = (sfVector2f){c.x + cosf(a) * 132.f, c.y + sinf(a) * 34.f + 10.f};
        lueur(app, p, (sfVector2f){18.f + 10.f * z, 18.f + 10.f * z},
            avec_alpha(col, 0.35f + 0.4f * z));
        losange(app, p, 2.5f + 2.5f * z, melanger(col, C_BLANC, 0.5f));
    }
}

/** Distance : réticule qui se resserre et graduations de visée. */
static void aura_archer(app_t *app, sfVector2f c, sfColor col, float t)
{
    float e = 78.f + 8.f * sinf(t * 2.4f);
    float sx = 0.f;
    float sy = 0.f;
    sfVector2f l[3];

    for (int k = 0; k < 4; k++) {
        sx = k % 2 ? 1.f : -1.f;
        sy = k < 2 ? -1.f : 1.f;
        l[0] = (sfVector2f){c.x + sx * e, c.y + sy * (e - 22.f)};
        l[1] = (sfVector2f){c.x + sx * e, c.y + sy * e};
        l[2] = (sfVector2f){c.x + sx * (e - 22.f), c.y + sy * e};
        ligne_epaisse(app, l, 3, 2.f, avec_alpha(col, 0.8f), 0);
    }
    for (int k = 0; k < 4; k++) {
        l[0] = (sfVector2f){c.x + cosf(k * PI / 2.f) * 112.f,
            c.y + sinf(k * PI / 2.f) * 112.f};
        l[1] = (sfVector2f){c.x + cosf(k * PI / 2.f) * 140.f,
            c.y + sinf(k * PI / 2.f) * 140.f};
        ligne_epaisse(app, l, 2, 1.5f, avec_alpha(col, 0.6f), 0);
    }
    graduations(app, c, 122.f, 72, 4.f, -t * 0.12f, avec_alpha(col, 0.35f));
}

/** Soutien : halo de croix bienveillantes et ondes douces. */
static void aura_soutien(app_t *app, sfVector2f c, sfColor col, float t)
{
    static const float CROIX[] = {-0.3f, -1.f, 0.3f, -1.f, 0.3f, 1.f, -0.3f, 1.f};
    float a = 0.f;
    float f = 0.f;

    for (int k = 0; k < 3; k++) {
        f = fmodf(t * 0.4f + k / 3.f, 1.f);
        arc_cercle(app, c, 80.f + 70.f * f, 1.5f, (sfVector2f){0.f, 2.f * PI},
            avec_alpha(col, 0.5f * (1.f - f)), 1);
    }
    for (int k = 0; k < 6; k++) {
        a = t * 0.25f + k * PI / 3.f;
        polygone(app, CROIX, 4, (sfVector2f){c.x + cosf(a) * 116.f, c.y + sinf(a)
            * 116.f}, 7.f, 0.f, avec_alpha(col, 0.75f));
        polygone(app, CROIX, 4, (sfVector2f){c.x + cosf(a) * 116.f, c.y + sinf(a)
            * 116.f}, 7.f, PI / 2.f, avec_alpha(col, 0.75f));
    }
}

typedef void (*aura_t)(app_t *app, sfVector2f c, sfColor col, float t);

static const aura_t AURAS[NB_CATEGORIES] = {
    [CAT_CORPS_A_CORPS] = aura_guerrier,
    [CAT_MAGIE] = aura_mage,
    [CAT_DISTANCE] = aura_archer,
    [CAT_SOUTIEN] = aura_soutien,
};

/** Socle lumineux et cône de projection de l'creation_hologramme. */
static void projecteur(app_t *app, sfColor col)
{
    sfVertex cone[4] = {
        {{SCENE_X - 110.f, SOCLE_Y}, avec_alpha(col, 0.16f * app->alpha), {0, 0}},
        {{SCENE_X + 110.f, SOCLE_Y}, avec_alpha(col, 0.16f * app->alpha), {0, 0}},
        {{SCENE_X + 60.f, SCENE_Y - 120.f}, avec_alpha(col, 0.f), {0, 0}},
        {{SCENE_X - 60.f, SCENE_Y - 120.f}, avec_alpha(col, 0.f), {0, 0}}
    };
    sfVertex trait[2];
    float y = 0.f;
    float f = 0.f;

    cible_primitives(app, cone, 4, sfQuads, &app->additif);
    for (int k = 0; k < 7; k++) {
        f = fmodf(k / 7.f + app->temps * 0.25f, 1.f);
        y = lerpf(SOCLE_Y - 6.f, SCENE_Y - 110.f, f);
        trait[0] = (sfVertex){{lerpf(SCENE_X - 108.f, SCENE_X - 62.f, f), y},
            avec_alpha(col, 0.22f * (1.f - f) * app->alpha), {0, 0}};
        trait[1] = (sfVertex){{lerpf(SCENE_X + 108.f, SCENE_X + 62.f, f), y},
            trait[0].color, {0, 0}};
        cible_primitives(app, trait, 2, sfLines, &app->additif);
    }
    lueur(app, (sfVector2f){SCENE_X, SOCLE_Y}, (sfVector2f){170.f, 34.f},
        avec_alpha(col, 0.35f));
    ellipse(app, (sfVector2f){SCENE_X, SOCLE_Y}, (sfVector2f){118.f, 18.f}, 2.f,
        avec_alpha(col, 0.7f));
    ellipse(app, (sfVector2f){SCENE_X, SOCLE_Y + 8.f}, (sfVector2f){142.f, 24.f}, 1.f,
        avec_alpha(col, 0.25f));
}

/** Suit les changements de classe pour déclencher la transition. */
void creation_suivre_classe(app_t *app, creation_t *c)
{
    if (app == NULL || c == NULL)
        return;

    c->t_classe += app->dt;
    if (c->classe == c->classe_vue)
        return;
    c->classe_prec = c->classe_vue;
    c->classe_vue = c->classe;
    c->t_classe = 0.f;
    emettre(app, (sfVector2f){SCENE_X, SCENE_Y}, 26, couleur_classe(c->classe),
        330.f, 3.f);
}

void creation_hologramme(app_t *app, creation_t *c)
{
    if (app == NULL || c == NULL)
        return;

    float t = app->temps;
    float m = progression(c->t_classe, 0.f, 0.35f);
    float pop = 0.75f + 0.25f * sortie_rebond(progression(c->t_classe, 0.f, 0.5f));
    float onde = progression(c->t_classe, 0.f, 0.7f);
    sfColor col = melanger(couleur_classe(c->classe_prec), couleur_classe(c->classe), m);
    sfVector2f centre = {SCENE_X, SCENE_Y + 4.f * sinf(t * 1.4f)};

    projecteur(app, col);
    lueur(app, centre, (sfVector2f){210.f, 210.f}, avec_alpha(col, 0.18f));
    if (onde < 1.f)
        arc_cercle(app, centre, 60.f + 170.f * sortie_cubique(onde), 3.f * (1.f - onde)
            + 1.f, (sfVector2f){0.f, 2.f * PI}, avec_alpha(col, 1.f - onde), 1);
    AURAS[classe_info(c->classe)->categorie](app, centre, col, t);
    for (int k = 0; k < 3; k++)
        arc_cercle(app, centre, 88.f, 2.f, (sfVector2f){t * 0.5f + k * 2.f * PI / 3.f,
            t * 0.5f + k * 2.f * PI / 3.f + 1.4f}, avec_alpha(col, 0.75f), 1);
    graduations(app, centre, 94.f, 60, 3.f, t * 0.08f, avec_alpha(C_BLANC, 0.18f));
    hexagone(app, centre, 64.f * pop, 0.f, melanger((sfColor){12, 14, 30, 235}, col,
        0.16f), avec_alpha(col, 0.95f));
    hexagone(app, centre, 54.f * pop, PI / 6.f, C_TRANSPARENT, avec_alpha(col, 0.3f));
    lueur(app, centre, (sfVector2f){90.f, 90.f}, avec_alpha(col, 0.3f));
    embleme(app, c->classe, centre, 34.f * pop, melanger(col, C_BLANC, 0.55f));
}
