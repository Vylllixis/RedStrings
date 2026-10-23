/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ particules.c ───────────────────────────────────────────────────────
** │ But         Les étincelles : émission, mise à jour (gravité, durée de
** │             vie) et dessin.
** │ Mémoire     un tableau de taille fixe (MAX_PARTICULES) dont les cases
** │             mortes sont réutilisées : jamais d'allocation pendant le
** │             jeu
** │ Contenu     emettre, maj_particules, dessiner_particules
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_particules.h"
#include "gui_types.h"

/** Projette `n` étincelles depuis `pos` dans toutes les directions. */
void emettre(app_t *app, sfVector2f pos, int n, sfColor c, float vitesse,
    float taille)
{
    if (app == NULL)
        return;

    particule_t *p = NULL;
    float angle = 0.f;
    float v = 0.f;

    for (int i = 0; i < MAX_PARTICULES && n > 0; i++) {
        p = &app->particules[i];
        if (p->vie > 0.f)
            continue;
        angle = aleatoire(0.f, 2.f * PI);
        v = vitesse * aleatoire(0.25f, 1.f);
        p->pos = pos;
        p->vit = (sfVector2f){cosf(angle) * v, sinf(angle) * v - vitesse * 0.15f};
        p->vie_max = aleatoire(0.45f, 1.1f);
        p->vie = p->vie_max;
        p->taille = taille * aleatoire(0.5f, 1.2f);
        p->gravite = aleatoire(120.f, 420.f);
        p->couleur = c;
        n--;
    }
}

void maj_particules(app_t *app)
{
    if (app == NULL)
        return;

    particule_t *p = NULL;
    float frein = expf(-3.2f * app->dt_jeu);

    for (int i = 0; i < MAX_PARTICULES; i++) {
        p = &app->particules[i];
        if (p->vie <= 0.f)
            continue;
        p->vit.x *= frein;
        p->vit.y = p->vit.y * frein + p->gravite * app->dt_jeu;
        p->pos.x += p->vit.x * app->dt_jeu;
        p->pos.y += p->vit.y * app->dt_jeu;
        p->vie -= app->dt_jeu;
    }
}

void dessiner_particules(app_t *app)
{
    if (app == NULL)
        return;

    particule_t *p = NULL;
    float t = 0.f;
    float s = 0.f;

    for (int i = 0; i < MAX_PARTICULES; i++) {
        p = &app->particules[i];
        if (p->vie <= 0.f)
            continue;
        t = p->vie / p->vie_max;
        s = p->taille * (0.3f + 0.7f * t);
        lueur(app, p->pos, (sfVector2f){s * 4.f, s * 4.f},
            avec_alpha(p->couleur, 0.55f * t));
        lueur(app, p->pos, (sfVector2f){s * 1.3f, s * 1.3f},
            avec_alpha(melanger(p->couleur, C_BLANC, 0.6f), t));
    }
}
