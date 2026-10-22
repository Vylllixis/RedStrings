/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ affichage.c ────────────────────────────────────────────────────────
** │ But         Fenêtre et affichage : trois modes (fenêtré
** │             redimensionnable, plein écran natif, plein écran sans
** │             bordure), changement à chaud, et calcul de l'échelle de
** │             l'interface.
** │ Repère      les scènes dessinent dans un repère 1280 x 720 centré
** │             dans la fenêtre ; app->ecran donne la partie réellement
** │             visible
** │ Contenu     ouvrir_fenetre, appliquer_taille, demander_affichage,
** │             appliquer_demande_affichage, basculer_plein_ecran,
** │             changer_type_plein_ecran
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include <stdio.h>
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_types.h"
#include "gui_ui.h"

static const char *NOMS_AFFICHAGES[NB_AFFICHAGES] = {
    "Fenêtré", "Plein écran", "Plein écran sans bordure"
};

/** Mode vidéo du plein écran natif : celui du bureau s'il est accepté. */
static sfVideoMode mode_plein_ecran(void)
{
    sfVideoMode bureau = sfVideoMode_getDesktopMode();
    size_t n = 0;
    const sfVideoMode *modes = NULL;

    if (sfVideoMode_isValid(bureau))
        return bureau;
    modes = sfVideoMode_getFullscreenModes(&n);
    return n > 0 ? modes[0] : bureau;
}

static sfRenderWindow *creer_fenetre(app_t *app, affichage_t mode)
{
    sfContextSettings reglages = {0, 0, 8, 1, 1, sfContextDefault, sfFalse};
    sfVideoMode bureau = sfVideoMode_getDesktopMode();
    sfVideoMode taille = {app->taille_fenetre.x, app->taille_fenetre.y, 32};
    sfRenderWindow *f = NULL;

    if (mode == AFFICHAGE_PLEIN_ECRAN)
        return sfRenderWindow_create(mode_plein_ecran(), "Redstrings", sfFullscreen,
            &reglages);
    if (mode == AFFICHAGE_SANS_BORDURE) {
        f = sfRenderWindow_create(bureau, "Redstrings", sfNone, &reglages);
        if (f != NULL)
            sfRenderWindow_setPosition(f, (sfVector2i){0, 0});
        return f;
    }
    return sfRenderWindow_create(taille, "Redstrings", sfDefaultStyle, &reglages);
}

/*
** ╭─ ouvrir_fenetre ─────────────────────────────────────────────────────
** │ Crée la fenêtre dans le mode demandé, en remplaçant l'ancienne.
** │
** │ 1. creer_fenetre() essaie le mode demandé
** │ 2. si un plein écran échoue, on retente en mode fenêtré ; si tout
** │    échoue, renvoie -1 et l'ancienne fenêtre reste en place
** │ 3. sinon l'ancienne est détruite, la synchronisation verticale
** │    activée, et appliquer_taille() recalcule l'échelle
** │ Polices, textures et état du jeu ne dépendent pas de la fenêtre :
** │ rien n'est perdu.
** ╰──────────────────────────────────────────────────────────────────────
*/
int ouvrir_fenetre(app_t *app, affichage_t mode)
{
    sfRenderWindow *f = creer_fenetre(app, mode);
    sfVector2u t;

    if (f == NULL && mode != AFFICHAGE_FENETRE) {
        mode = AFFICHAGE_FENETRE;
        f = creer_fenetre(app, mode);
    }
    if (f == NULL)
        return -1;
    if (app->fenetre != NULL)
        sfRenderWindow_destroy(app->fenetre);
    app->fenetre = f;
    app->affichage = mode;
    sfRenderWindow_setVerticalSyncEnabled(f, sfTrue);
    app->curseur_actuel = NB_CURSEURS;
    app->in.bas = 0;
    app->in.dehors = 0;
    t = sfRenderWindow_getSize(f);
    appliquer_taille(app, t.x, t.y);
    return 0;
}

/*
** ╭─ appliquer_taille ───────────────────────────────────────────────────
** │ Nouvelle taille de fenêtre : calcule l'échelle de l'interface.
** │
** │ 1. échelle = min(largeur / ECRAN_L_MIN, hauteur / ECRAN_H_MIN),
** │    bornée entre ECHELLE_MIN et ECHELLE_MAX : au moins ECRAN_L_MIN x
** │    ECRAN_H_MIN unités restent visibles
** │ 2. app->ecran = la zone visible, en unités du repère 1280 x 720,
** │    centrée
** │ 3. les deux vues reçoivent cette taille
** │ La zone visible garde les proportions de la fenêtre : rien n'est
** │ étiré.
** ╰──────────────────────────────────────────────────────────────────────
*/
void appliquer_taille(app_t *app, unsigned int largeur, unsigned int hauteur)
{
    float l = 0.f;
    float h = 0.f;

    if (largeur == 0 || hauteur == 0)
        return;
    app->echelle = bornerf(fminf(largeur / ECRAN_L_MIN, hauteur / ECRAN_H_MIN),
        ECHELLE_MIN, ECHELLE_MAX);
    l = largeur / app->echelle;
    h = hauteur / app->echelle;
    app->ecran = (sfFloatRect){LARGEUR / 2.f - l / 2.f, HAUTEUR / 2.f - h / 2.f, l, h};
    if (app->vue != NULL)
        sfView_setSize(app->vue, (sfVector2f){l, h});
    if (app->vue_fixe != NULL) {
        sfView_setSize(app->vue_fixe, (sfVector2f){l, h});
        sfView_setCenter(app->vue_fixe, (sfVector2f){LARGEUR / 2.f, HAUTEUR / 2.f});
    }
    if (app->affichage == AFFICHAGE_FENETRE)
        app->taille_fenetre = (sfVector2u){largeur, hauteur};
}

/** Le changement est appliqué après la lecture des événements de l'ancienne fenêtre. */
void demander_affichage(app_t *app, affichage_t mode)
{
    if ((unsigned)mode < NB_AFFICHAGES)
        app->affichage_demande = (int)mode;
}

void appliquer_demande_affichage(app_t *app)
{
    char msg[96];
    affichage_t mode = (affichage_t)app->affichage_demande;

    if (app->affichage_demande < 0)
        return;
    app->affichage_demande = -1;
    if (mode == app->affichage || ouvrir_fenetre(app, mode) != 0)
        return;
    snprintf(msg, sizeof(msg), "%s  ·  F11 pour %s", NOMS_AFFICHAGES[app->affichage],
        app->affichage == AFFICHAGE_FENETRE ? "le plein écran" : "revenir");
    notifier(app, "◆", msg, C_CYAN);
}

/** F11 ou Alt+Entrée : fenêtré ↔ plein écran (du type choisi). */
void basculer_plein_ecran(app_t *app)
{
    demander_affichage(app, app->affichage == AFFICHAGE_FENETRE ? app->plein_ecran
        : AFFICHAGE_FENETRE);
}

/** Maj+F11 : choisit l'autre type de plein écran, et l'applique si on y est déjà. */
void changer_type_plein_ecran(app_t *app)
{
    char msg[96];

    app->plein_ecran = app->plein_ecran == AFFICHAGE_SANS_BORDURE
        ? AFFICHAGE_PLEIN_ECRAN : AFFICHAGE_SANS_BORDURE;
    if (app->affichage != AFFICHAGE_FENETRE) {
        demander_affichage(app, app->plein_ecran);
        return;
    }
    snprintf(msg, sizeof(msg), "F11 : %s", NOMS_AFFICHAGES[app->plein_ecran]);
    notifier(app, "◆", msg, C_CYAN);
}
