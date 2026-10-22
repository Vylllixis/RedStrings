/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ menu_commandes.c ───────────────────────────────────────────────────
** │ But         Menu : les quatre commandes (ajouter, afficher, combat,
** │             quitter).
** │ Contenu     menu_option_principale, lire_options, dessiner_option,
** │             menu_commandes
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include <stdio.h>
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_etat.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "gui_ui.h"
#include "menu_interne.h"

/* Colonne des menu_commandes et panneau d'équipe */
#define OX 150.f

#define OW 500.f

#define OY 290.f

/** Action mise en avant : le combat dès qu'il est possible, sinon l'ajout. */
int menu_option_principale(app_t *app)
{
    return app->equipe.len >= 2 ? 2 : 0;
}

typedef struct {
    const char *touche;
    const char *label;
    const char *description;
    const char *indisponible;   /**< Raison affichée quand l'option est grisée. */
    char info[32];
    sfColor accent;
    int actif;
} option_t;

/** Remplit les quatre options à partir des données réelles de l'équipe. */
static void lire_options(app_t *app, option_t o[4])
{
    const int n = app->equipe.len;

    o[0] = (option_t){"1", "Ajouter un personnage", "Nom, statistiques et classe",
        "Équipe complète", "", C_CYAN, n < MAX_PERSONNAGES};
    o[1] = (option_t){"2", "Afficher un personnage", "Fiches et recherche par nom",
        "Aucun personnage pour l'instant", "", C_BLEU, n > 0};
    o[2] = (option_t){"3", "Combat", "Équipes de 1 à 4 personnages, tour par tour",
        "Il faut au moins 2 personnages", "", C_ROUGE, n >= 2};
    o[3] = (option_t){"0", "Quitter", NULL, NULL, "", C_TEXTE_DOUX, 1};
    snprintf(o[0].info, sizeof(o[0].info), "%d / %d", n, MAX_PERSONNAGES);
    snprintf(o[1].info, sizeof(o[1].info), "%d héros", n);
    snprintf(o[2].info, sizeof(o[2].info), "%d en forme", n - menu_nb_ko(app));
}

/** Une option du menu ; renvoie 1 si elle est cliquée. */
static int dessiner_option(app_t *app, int i, sfFloatRect r, const option_t *o,
    int principal)
{
    menu_t *m = &app->menu;
    int s = survole(app, r);
    float sel = 0.f;
    float a = app->alpha;
    float cy = r.top + r.height / 2.f;
    float x = 0.f;
    float wi = 0.f;
    sfColor fond = C_SURFACE_HAUTE;

    if (s && m->souris_bouge && o->actif)
        m->selection = i;
    if (s)
        app->curseur_voulu = CURSEUR_MAIN;
    m->boutons[i].survol = approcher(m->boutons[i].survol, m->selection == i ? 1.f : 0.f,
        14.f, app->dt);
    m->boutons[i].appui = approcher(m->boutons[i].appui, s && app->in.bas ? 1.f : 0.f,
        30.f, app->dt);
    sel = m->boutons[i].survol;
    maj_ressort(&m->decalage[i], m->selection == i ? 10.f : 0.f, 320.f, 20.f, app->dt);
    r.left += m->decalage[i].x;
    r.top += 2.f * m->boutons[i].appui;
    if (!o->actif)
        app->alpha *= 0.5f;
    if (principal && o->actif)
        fond = melanger(C_SURFACE_HAUTE, o->accent, 0.22f);
    lueur(app, (sfVector2f){r.left + r.width * 0.4f, cy}, (sfVector2f){r.width * 0.8f,
        r.height * 1.5f}, avec_alpha(o->accent, (principal && o->actif ? 0.12f : 0.f)
        + 0.16f * sel));
    if (o->description || sel > 0.01f)
        ombre_biseau(app, r, 14.f, 0.8f);
    panneau(app, r, 14.f, melanger(o->description ? fond : avec_alpha(C_SURFACE, 0.f),
        melanger(fond, o->accent, 0.3f), sel), melanger(o->description ? C_SURFACE
        : avec_alpha(C_SURFACE, 0.f), C_SURFACE, sel), melanger(principal && o->actif
        ? avec_alpha(o->accent, 0.5f) : C_BORD, o->accent, sel));
    rect_arrondi(app, (sfFloatRect){r.left, r.top + 14.f, 3.f + 2.f * sel,
        r.height - 14.f}, 0.f, avec_alpha(o->accent, 0.3f + 0.7f * sel));
    x = r.left + 26.f;
    touche_clavier(app, o->touche, (sfVector2f){x + 13.f, cy}, o->accent, sel);
    x += 44.f;
    texte(app, POLICE_GRAS, o->label, o->description ? 20 : 17, (sfVector2f){x,
        o->description ? cy - 10.f : cy}, melanger(C_TEXTE_DOUX, C_TEXTE,
        0.6f + 0.4f * sel), ALIGNE_GAUCHE);
    if (o->description)
        texte(app, POLICE_NORMALE, o->actif ? o->description : o->indisponible, 13,
            (sfVector2f){x, cy + 13.f}, o->actif ? C_TEXTE_FAIBLE
            : melanger(C_ROUGE, C_TEXTE_DOUX, 0.4f), ALIGNE_GAUCHE);
    if (o->info[0]) {
        wi = largeur_texte(app, POLICE_GRAS, o->info, 13) + 20.f;
        panneau(app, (sfFloatRect){r.left + r.width - 50.f - wi, cy - 13.f, wi, 26.f},
            6.f, avec_alpha(o->accent, 0.10f + 0.12f * sel), avec_alpha(o->accent, 0.04f),
            avec_alpha(o->accent, 0.25f + 0.35f * sel));
        texte(app, POLICE_GRAS, o->info, 13, (sfVector2f){r.left + r.width - 50.f
            - wi / 2.f, cy}, melanger(o->accent, C_BLANC, 0.35f), ALIGNE_CENTRE);
    }
    texte(app, POLICE_GRAS, "›", 28, (sfVector2f){r.left + r.width - 26.f + 5.f * sel,
        cy - 2.f}, melanger(C_TEXTE_FAIBLE, o->accent, sel), ALIGNE_CENTRE);
    app->alpha = a;
    return s && app->in.clic;
}

void menu_commandes(app_t *app, float t)
{
    option_t o[4];
    float a = app->alpha;
    float e = 0.f;
    float y = 0.f;
    int choix = menu_touche_menu(app);
    int principal = menu_option_principale(app);
    const int *k = app->in.touches;

    app->menu.souris_bouge = fabsf(app->in.souris.x - app->menu.souris_prec.x) > 0.01f
        || fabsf(app->in.souris.y - app->menu.souris_prec.y) > 0.01f;
    app->menu.souris_prec = app->in.souris;
    lire_options(app, o);
    if (!o[app->menu.selection].actif)
        app->menu.selection = principal;
    for (int pas = 0; (k[sfKeyDown] || k[sfKeyUp]) && pas < 4; pas++) {
        app->menu.selection = (app->menu.selection + (k[sfKeyDown] ? 1 : 3)) % 4;
        if (o[app->menu.selection].actif)
            break;
    }
    app->alpha = a * progression(t, 0.35f, 0.4f);
    titre_section(app, NULL, "COMMANDES", (sfVector2f){OX, OY - 22.f}, OW, C_ROUGE);
    for (int i = 0; i < 4; i++) {
        e = sortie_cubique(progression(t, 0.4f + i * 0.07f, 0.55f));
        app->alpha = a * progression(t, 0.4f + i * 0.07f, 0.3f);
        y = i < 3 ? OY + i * 74.f : OY + 3 * 74.f + 16.f;
        if (i == 3) {
            sfVertex filet[2] = {
                {{OX, y - 9.f}, avec_alpha(C_BLANC, 0.08f * app->alpha), {0, 0}},
                {{OX + OW, y - 9.f}, avec_alpha(C_BLANC, 0.f), {0, 0}}};

            cible_primitives(app, filet, 2, sfLines, NULL);
        }
        if (dessiner_option(app, i, (sfFloatRect){OX - 40.f * (1.f - e), y, OW,
            i < 3 ? 64.f : 46.f}, &o[i], i == principal))
            choix = i;
    }
    app->alpha = a;
    if (choix >= 0)
        menu_action(app, choix);
}
