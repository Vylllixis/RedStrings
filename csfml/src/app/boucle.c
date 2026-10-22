/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Une image : transition entre scènes, dessin de la scène courante, curseur.
 */

#include <math.h>
#include <string.h>
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_particules.h"
#include "gui_scenes.h"
#include "gui_types.h"
#include "gui_ui.h"

#define DUREE_SORTIE 0.22f

static void entrer_scene(app_t *app, scene_t s)
{
    app->scene = s;
    app->t_scene = 0.f;
    if (s == SCENE_MENU)
        entrer_menu(app);
    if (s == SCENE_CREATION)
        entrer_creation(app);
    if (s == SCENE_LISTE)
        entrer_liste(app);
    if (s == SCENE_SELECTION)
        entrer_selection(app);
    if (s == SCENE_ARENE)
        entrer_arene(app);
}

void changer_scene(app_t *app, scene_t scene)
{
    if (app->t_transition >= 0.f)
        return;
    app->scene_suivante = scene;
    app->t_transition = 0.f;
}

/** Fondu + glissement : la scène sort vers le haut, la suivante arrive du bas. */
static void maj_transition(app_t *app, float *alpha, float *decalage)
{
    float p = 0.f;

    if (app->t_transition >= 0.f) {
        app->t_transition += app->dt;
        p = progression(app->t_transition, 0.f, DUREE_SORTIE);
        if (p < 1.f) {
            *alpha = 1.f - p;
            *decalage = -16.f * entree_cubique(p);
            return;
        }
        entrer_scene(app, app->scene_suivante);
        app->t_transition = -1.f;
    }
    *alpha = progression(app->t_scene, 0.f, 0.25f);
    *decalage = 16.f * (1.f - sortie_cubique(progression(app->t_scene, 0.f, 0.45f)));
}

static void dessiner_scene(app_t *app)
{
    if (app->scene == SCENE_MENU)
        scene_menu(app);
    if (app->scene == SCENE_CREATION)
        scene_creation(app);
    if (app->scene == SCENE_LISTE)
        scene_liste(app);
    if (app->scene == SCENE_SELECTION)
        scene_selection(app);
    if (app->scene == SCENE_ARENE)
        scene_arene(app);
    if (app->scene == SCENE_AU_REVOIR)
        scene_au_revoir(app);
}

static void maj_curseur(app_t *app)
{
    if (app->curseur_voulu == app->curseur_actuel
        || app->curseurs[app->curseur_voulu] == NULL)
        return;
    sfRenderWindow_setMouseCursor(app->fenetre, app->curseurs[app->curseur_voulu]);
    app->curseur_actuel = app->curseur_voulu;
}

void maj_app(app_t *app, float dt)
{
    float alpha = 1.f;
    float decalage = 0.f;

    app->dt = bornerf(dt, 0.f, 1.f / 30.f);
    app->temps += app->dt;
    app->t_scene += app->dt;
    app->dt_jeu = app->hitstop > 0.f ? app->dt * 0.06f : app->dt;
    app->hitstop -= app->dt;
    app->secousse = approcher(app->secousse, 0.f, 7.f, app->dt);
    app->curseur_voulu = CURSEUR_FLECHE;
    maj_transition(app, &alpha, &decalage);
    app->souris_bloquee = app->t_transition >= 0.f;
    if (app->souris_bloquee) {
        app->in.clic = 0;
        app->in.nb_texte = 0;
        memset(app->in.touches, 0, sizeof(app->in.touches));
    }
    sfRenderWindow_clear(app->fenetre, C_FOND);
    sfRenderWindow_setView(app->fenetre, app->vue_fixe);
    app->alpha = 1.f;
    dessiner_fond(app);
    sfView_setCenter(app->vue, (sfVector2f){
        LARGEUR / 2.f + sinf(app->temps * 83.f) * 12.f * app->secousse,
        HAUTEUR / 2.f + decalage + cosf(app->temps * 97.f) * 9.f * app->secousse});
    sfRenderWindow_setView(app->fenetre, app->vue);
    app->alpha = alpha;
    dessiner_scene(app);
    app->alpha = 1.f;
    maj_particules(app);
    dessiner_particules(app);
    sfRenderWindow_setView(app->fenetre, app->vue_fixe);
    dessiner_toasts(app);
    maj_curseur(app);
}
