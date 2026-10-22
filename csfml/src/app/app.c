/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ app.c ──────────────────────────────────────────────────────────────
** │ But         Création et destruction de l'application (la structure
** │             app_t qui contient tout : fenêtre, ressources, état du
** │             jeu).
** │ Contenu     creer_app, detruire_app
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "roster.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_scenes.h"
#include "gui_types.h"
#include "gui_yugioh.h"
#include "app_interne.h"

int creer_app(app_t *app, mode_visuel_t mode_visuel)
{
    sfVector2u taille;

    memset(app, 0, sizeof(app_t));
    app->mode = mode_visuel;
    app->taille_fenetre = (sfVector2u){FENETRE_L_DEPART, FENETRE_H_DEPART};
    app->plein_ecran = AFFICHAGE_SANS_BORDURE;
    app->affichage_demande = -1;
    app->zoom = 1.f;
    roster_init(&app->equipe);
    srand(time(NULL));
    if (ouvrir_fenetre(app, AFFICHAGE_FENETRE) != 0)
        return 1;
    if (app_creer_ressources(app) != 0)
        return 1;
    taille = sfRenderWindow_getSize(app->fenetre);
    appliquer_taille(app, taille.x, taille.y);
    app->additif.blendMode = sfBlendAdd;
    app->additif.transform = sfTransform_Identity;
    app->additif.texture = NULL;
    app->additif.shader = NULL;
    app_init_poussieres(app);
    app->alpha = 1.f;
    if (app->mode == MODE_YUGIOH && yugioh_charger(app) != 0) {
        fprintf(stderr, "redstrings_csfml : mode Yu-Gi-Oh! indisponible, "
            "affichage normal\n");
        yugioh_liberer(app);
        app->mode = MODE_NORMAL;
    }
    app->t_transition = -1.f;
    app->ouvert = 1;
    app->scene = SCENE_MENU;
    entrer_menu(app);
    return 0;
}

void detruire_app(app_t *app)
{
    yugioh_liberer(app);
    if (app->fenetre)
        sfRenderWindow_destroy(app->fenetre);
    for (int i = 0; i < NB_POLICES; i++) {
        if (app->polices[i])
            sfFont_destroy(app->polices[i]);
    }
    for (int i = 0; i < NB_CURSEURS; i++) {
        if (app->curseurs[i])
            sfCursor_destroy(app->curseurs[i]);
    }
    if (app->texte)
        sfText_destroy(app->texte);
    if (app->forme)
        sfConvexShape_destroy(app->forme);
    if (app->cercle)
        sfCircleShape_destroy(app->cercle);
    if (app->sprite)
        sfSprite_destroy(app->sprite);
    if (app->tex_lueur)
        sfTexture_destroy(app->tex_lueur);
    if (app->tex_vignette)
        sfTexture_destroy(app->tex_vignette);
    if (app->horloge)
        sfClock_destroy(app->horloge);
    if (app->vue)
        sfView_destroy(app->vue);
    if (app->vue_fixe)
        sfView_destroy(app->vue_fixe);
    if (app->vue_zoom)
        sfView_destroy(app->vue_zoom);
    roster_liberer(&app->equipe);
}
