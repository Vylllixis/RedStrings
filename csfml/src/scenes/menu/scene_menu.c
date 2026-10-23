/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ scene_menu.c ───────────────────────────────────────────────────────
** │ But         Menu principal : choix d'une commande, sauvegarde,
** │             chargement et écran d'au revoir.
** │ Contenu     entrer_menu, sauvegarder_equipe, charger_equipe,
** │             menu_action, menu_touche_menu, menu_nb_ko, scene_menu,
** │             scene_au_revoir
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include <string.h>
#include "personnage.h"
#include "roster.h"
#include "sauvegarde.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_ecran.h"
#include "gui_etat.h"
#include "gui_scenes.h"
#include "gui_types.h"
#include "gui_ui.h"
#include "menu_interne.h"

/* Pas de bouton à l'écran : seulement les touches 4 et 5, comme la console. */
#define CHOIX_SAUVEGARDER 4

#define CHOIX_CHARGER 5

void entrer_menu(app_t *app)
{
    if (app == NULL)
        return;

    memset(&app->menu, 0, sizeof(menu_t));
    app->menu.selection = menu_option_principale(app);
}

/** Touches 4 et 5 : mêmes actions que le menu de la console, sans bouton. */
static void sauvegarder_equipe(app_t *app)
{
    sauvegarde_t code = sauvegarder(&app->equipe, FICHIER_SAUVEGARDE);
    char msg[96];

    if (code != SAUVEGARDE_OK) {
        notifier(app, "✗", sauvegarde_message(code), C_ROUGE);
        return;
    }
    snprintf(msg, sizeof(msg), "%d personnage%s sauvegardé%s", app->equipe.len,
        app->equipe.len > 1 ? "s" : "", app->equipe.len > 1 ? "s" : "");
    son(app, SON_SAUVEGARDE_OK);
    notifier(app, "✔", msg, C_VERT);
}

static void charger_equipe(app_t *app)
{
    roster_t lus;
    sauvegarde_t code = SAUVEGARDE_OK;
    char msg[96];

    roster_init(&lus);
    code = charger(&lus, FICHIER_SAUVEGARDE);
    if (code != SAUVEGARDE_OK) {
        notifier(app, "✗", sauvegarde_message(code), C_ROUGE);
        return;
    }
    if (lus.len > MAX_PERSONNAGES) {
        snprintf(msg, sizeof(msg), "Trop de personnages pour l'écran (%d / %d)",
            lus.len, MAX_PERSONNAGES);
        notifier(app, "✗", msg, C_ROUGE);
        roster_liberer(&lus);
        return;
    }
    roster_liberer(&app->equipe);
    app->equipe = lus;
    snprintf(msg, sizeof(msg), "%d personnage%s chargé%s", lus.len,
        lus.len > 1 ? "s" : "", lus.len > 1 ? "s" : "");
    son(app, SON_CHARGEMENT_OK);
    notifier(app, "✔", msg, C_VERT);
}

void menu_action(app_t *app, int choix)
{
    if (app == NULL)
        return;

    if (choix == CHOIX_SAUVEGARDER)
        sauvegarder_equipe(app);
    if (choix == CHOIX_CHARGER)
        charger_equipe(app);
    if (choix == 0 && app->equipe.len >= MAX_PERSONNAGES)
        notifier(app, "✗", "Nombre maximum de personnages atteint", C_ROUGE);
    else if (choix == 0)
        changer_scene(app, SCENE_CREATION);
    if (choix == 1 && app->equipe.len == 0)
        notifier(app, "✗", "Aucun personnage : crée-en un d'abord", C_ROUGE);
    else if (choix == 1)
        changer_scene(app, SCENE_LISTE);
    if (choix == 2 && app->equipe.len < 2)
        notifier(app, "✗", "Il faut au moins 2 personnages pour un combat", C_ROUGE);
    else if (choix == 2)
        changer_scene(app, SCENE_SELECTION);
    if (choix == 3)
        changer_scene(app, SCENE_AU_REVOIR);
}

int menu_touche_menu(app_t *app)
{
    if (app == NULL)
        return -1;

    const int *k = app->in.touches;

    if (k[sfKeyNum1] || k[sfKeyNumpad1])
        return 0;
    if (k[sfKeyNum2] || k[sfKeyNumpad2])
        return 1;
    if (k[sfKeyNum3] || k[sfKeyNumpad3])
        return 2;
    if (k[sfKeyNum0] || k[sfKeyNumpad0] || k[sfKeyEscape])
        return 3;
    if (k[sfKeyNum4] || k[sfKeyNumpad4])
        return CHOIX_SAUVEGARDER;
    if (k[sfKeyNum5] || k[sfKeyNumpad5])
        return CHOIX_CHARGER;
    if (k[sfKeyEnter] || k[sfKeySpace])
        return app->menu.selection;
    return -1;
}

int menu_nb_ko(app_t *app)
{
    int n = 0;

    if (app == NULL)
        return 0;

    for (int i = 0; i < app->equipe.len; i++)
        n += est_ko(&app->equipe.data[i]);
    return n;
}

void scene_menu(app_t *app)
{
    if (app == NULL)
        return;

    menu_titre(app, app->t_scene);
    menu_commandes(app, app->t_scene);
    menu_panneau_equipe(app, app->t_scene);
    menu_aide_menu(app, app->t_scene);
    menu_credit_claude(app, app->t_scene);
}

void scene_au_revoir(app_t *app)
{
    if (app == NULL)
        return;

    float t = app->t_scene;
    float e = sortie_rebond(progression(t, 0.05f, 0.6f));
    float a = app->alpha;

    lueur(app, (sfVector2f){LARGEUR / 2.f, 330.f}, (sfVector2f){460.f, 170.f},
        avec_alpha(C_ROUGE, 0.14f * e));
    texte_echelle(app, POLICE_GRAS, "À bientôt !", 68, (sfVector2f){LARGEUR / 2.f,
        330.f + 20.f * (1.f - e)}, C_TEXTE, ALIGNE_CENTRE, 0.85f + 0.15f * e);
    menu_fil_rouge(app, 392.f, 200.f, sortie_cubique(progression(t, 0.2f, 0.7f)));
    app->alpha = a * progression(t, 0.35f, 0.4f);
    texte(app, POLICE_NORMALE, "Merci d'avoir joué à Redstrings", 18,
        (sfVector2f){LARGEUR / 2.f, 432.f}, C_TEXTE_DOUX, ALIGNE_CENTRE);
    app->alpha = 1.f;
    rect_arrondi(app, ecran_couvrant(app), 0.f,
        avec_alpha(C_NOIR, progression(t, 1.1f, 0.5f)));
    app->alpha = a;
    if (t > 1.65f)
        app->ouvert = 0;
}
