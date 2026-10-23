/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ arene_effets.c ─────────────────────────────────────────────────────
** │ But         Arène : effets visuels des actions (un impact différent
** │             par catégorie, rayon de soin).
** │ Contenu     impact_corps_a_corps, impact_magie, impact_distance,
** │             impact_soutien, bezier, rayon_soin, arene_dessiner_effet
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include "personnage.h"
#include "bataille.h"
#include "gui_anim.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_etat.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "arene_interne.h"

typedef void (*effet_impact_t)(app_t *app, sfVector2f c, sfColor col, float t,
    float s);

static void impact_corps_a_corps(app_t *app, sfVector2f c, sfColor col, float t,
    float s)
{
    float p = sortie_cubique(progression(t, 0.f, 0.12f)) * s;
    float f = 1.f - progression(t, 0.1f, 0.3f);
    sfVector2f l1[2] = {{c.x - 90.f * p, c.y - 90.f * p}, {c.x + 90.f * p, c.y + 90.f * p}};
    sfVector2f l2[2] = {{c.x + 80.f * p, c.y - 80.f * p}, {c.x - 80.f * p, c.y + 80.f * p}};

    ligne_epaisse(app, l1, 2, 22.f * s, avec_alpha(col, 0.35f * f), 1);
    ligne_epaisse(app, l1, 2, 4.f, avec_alpha(C_BLANC, f), 1);
    ligne_epaisse(app, l2, 2, 18.f * s, avec_alpha(col, 0.3f * f), 1);
    ligne_epaisse(app, l2, 2, 3.f, avec_alpha(C_BLANC, f), 1);
}

static void impact_magie(app_t *app, sfVector2f c, sfColor col, float t, float s)
{
    float p = sortie_cubique(progression(t, 0.f, 0.12f));
    float f = 1.f - progression(t, 0.1f, 0.3f);

    cercle(app, c, (24.f + 110.f * sortie_cubique(progression(t, 0.f, 0.4f))) * s,
        C_TRANSPARENT, avec_alpha(col, f), 5.f * f + 1.f);
    lueur(app, c, (sfVector2f){170.f * p * s, 170.f * p * s}, avec_alpha(col, 0.5f * f));
}

static void impact_distance(app_t *app, sfVector2f c, sfColor col, float t, float s)
{
    float f = 1.f - progression(t, 0.f, 0.3f);

    for (int k = 0; k < 6; k++)
        arc_cercle(app, c, (18.f + 70.f * sortie_cubique(progression(t, 0.f, 0.3f)))
            * s, 3.f * f + 1.f, (sfVector2f){k * PI / 3.f, k * PI / 3.f + 0.5f},
            avec_alpha(col, f), 1);
    lueur(app, c, (sfVector2f){90.f * s, 90.f * s}, avec_alpha(C_BLANC, 0.6f * f));
}

static void impact_soutien(app_t *app, sfVector2f c, sfColor col, float t, float s)
{
    float p = sortie_cubique(progression(t, 0.f, 0.18f)) * s;
    float f = 1.f - progression(t, 0.1f, 0.35f);
    sfVector2f v[2] = {{c.x, c.y - 100.f * p}, {c.x, c.y + 100.f * p}};
    sfVector2f h[2] = {{c.x - 100.f * p, c.y}, {c.x + 100.f * p, c.y}};

    lueur(app, c, (sfVector2f){200.f * p, 200.f * p}, avec_alpha(col, 0.45f * f));
    ligne_epaisse(app, v, 2, 14.f * s, avec_alpha(col, 0.4f * f), 1);
    ligne_epaisse(app, h, 2, 14.f * s, avec_alpha(col, 0.4f * f), 1);
    ligne_epaisse(app, v, 2, 3.f, avec_alpha(C_BLANC, f), 1);
    ligne_epaisse(app, h, 2, 3.f, avec_alpha(C_BLANC, f), 1);
}

static const effet_impact_t EFFETS_IMPACT[NB_CATEGORIES] = {
    [CAT_CORPS_A_CORPS] = impact_corps_a_corps,
    [CAT_MAGIE] = impact_magie,
    [CAT_DISTANCE] = impact_distance,
    [CAT_SOUTIEN] = impact_soutien,
};

/** Point d'une courbe de Bézier quadratique (rayon de soin). */
static sfVector2f bezier(sfVector2f a, sfVector2f m, sfVector2f b, float u)
{
    return (sfVector2f){(1 - u) * (1 - u) * a.x + 2 * (1 - u) * u * m.x + u * u * b.x,
        (1 - u) * (1 - u) * a.y + 2 * (1 - u) * u * m.y + u * u * b.y};
}

static void rayon_soin(app_t *app, sfVector2f a, sfVector2f b, float t)
{
    sfVector2f m = {(a.x + b.x) / 2.f, fminf(a.y, b.y) - 90.f};
    sfVector2f p[32];
    float debut = progression(t, 0.3f, 0.25f);
    float fin = progression(t, 0.1f, 0.3f);
    int n = 0;

    if (fin <= debut)
        return;
    for (int i = 0; i < 32; i++)
        p[n++] = bezier(a, m, b, lerpf(debut, fin, i / 31.f));
    ligne_epaisse(app, p, n, 12.f, avec_alpha(C_SOIN, 0.18f), 1);
    ligne_epaisse(app, p, n, 2.5f, avec_alpha(C_SOIN, 0.9f), 1);
    for (int k = 0; k < 4; k++)
        lueur(app, bezier(a, m, b, lerpf(debut, fin, fmodf(t * 3.f + k * 0.25f, 1.f))),
            (sfVector2f){18.f, 18.f}, avec_alpha(C_SOIN, 0.8f));
}

/** Effets dessinés pendant l'événement en cours (traînée, impact, rayon). */
void arene_dessiner_effet(app_t *app, arene_t *ar)
{
    if (app == NULL || ar == NULL)
        return;

    const evenement_t *ev = NULL;
    visuel_t *a = NULL;
    visuel_t *c = NULL;
    sfVector2f trainee[2];
    classe_t cl = GUERRIER;

    if (ar->etat != VUE_ANIMATION || ar->i_file >= ar->nb_file)
        return;
    ev = &ar->file[ar->i_file];
    a = arene_vis(ar, ev->acteur);
    c = arene_vis(ar, ev->cible);
    cl = bataille_lire(&ar->bataille, ev->acteur)->perso.classe;
    if (ev->type == EV_ATTAQUE && ar->t_evt > 0.12f && ar->t_evt < 0.4f) {
        trainee[0] = a->pos;
        trainee[1] = (sfVector2f){a->pos.x + a->elan.x, a->pos.y + a->elan.y};
        ligne_epaisse(app, trainee, 2, a->rayon * 0.45f, avec_alpha(couleur_classe(cl),
            0.16f), 1);
        ligne_epaisse(app, trainee, 2, 3.f, avec_alpha(C_BLANC, 0.35f), 1);
    }
    if (ev->type == EV_ATTAQUE && ar->t_evt >= ARENE_IMPACTS[EV_ATTAQUE])
        EFFETS_IMPACT[classe_info(cl)->categorie](app, c->pos, couleur_classe(cl),
            ar->t_evt - ARENE_IMPACTS[EV_ATTAQUE], c->rayon / 56.f);
    if (ev->type == EV_SOIN)
        rayon_soin(app, a->pos, c->pos, ar->t_evt);
}
