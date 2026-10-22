/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ scene_creation.c ───────────────────────────────────────────────────
** │ But         Écran de création d'un personnage : formulaire, classes
** │             et aperçu en direct.
** │ Équivaut à  creer_personnage() de la console
** │ Contenu     entrer_creation, apparaitre, bouton_creer, formulaire,
** │             aide_clavier, creation_geometrie, scene_creation
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "personnage.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_etat.h"
#include "gui_hud.h"
#include "gui_scenes.h"
#include "gui_types.h"
#include "gui_ui.h"
#include "gui_yugioh.h"
#include "creation_interne.h"

static const char *LABELS_STATS[NB_STATS] = {"Points de vie", "Attaque", "Défense",
    "Soin"};

void entrer_creation(app_t *app)
{
    creation_t *c = &app->creation;

    memset(c, 0, sizeof(creation_t));
    c->valeurs[0] = 50;
    c->valeurs[1] = 30;
    c->valeurs[2] = 20;
    c->valeurs[3] = 20;
    c->classe = GUERRIER;
    c->classe_vue = GUERRIER;
    c->classe_prec = GUERRIER;
    c->t_classe = 10.f;
    c->cat_main = -1;
    c->sortie = -1.f;
}

/** Fait apparaître l'élément `i` en cascade et renvoie son décalage vertical. */
static float apparaitre(app_t *app, float base, int i)
{
    float d = 0.12f + i * 0.05f;

    app->alpha = base * progression(app->t_scene, d, 0.35f);
    return 18.f * (1.f - sortie_cubique(progression(app->t_scene, d, 0.5f)));
}

static void bouton_creer(app_t *app, creation_t *c, float a, int actif)
{
    const char *raison = creation_raison_invalide(app, c);
    float dy = apparaitre(app, a, 6);
    int fini = c->sortie >= 0.f;
    int pret = actif && raison == NULL;

    if (bouton_plein(app, &c->creer, (sfFloatRect){FX, 628.f + dy, FW, 56.f},
        fini ? "Personnage créé" : "Créer le personnage", fini ? NULL : "Entrée",
        fini ? C_VERT : C_CYAN, pret || fini) && actif)
        creation_creer(app, c);
    if (c->focus == FOCUS_BOUTON)
        crochets(app, (sfFloatRect){FX - 8.f, 620.f + dy, FW + 16.f, 72.f}, 14.f,
            avec_alpha(C_BLANC, 0.6f + 0.3f * sinf(app->temps * 5.f)));
    if (raison && actif) {
        losange(app, (sfVector2f){FX + 4.f, 702.f + dy}, 3.f, C_ROUGE);
        texte(app, POLICE_NORMALE, raison, 13, (sfVector2f){FX + 14.f, 702.f + dy},
            avec_alpha(melanger(C_ROUGE, C_TEXTE, 0.35f), 0.9f), ALIGNE_GAUCHE);
    }
}

static void formulaire(app_t *app, creation_t *c, float a, int actif)
{
    const sfFloatRect cadre = {FX - 16.f, 194.f, FW + 32.f, 418.f};
    float dy = 0.f;

    app->alpha = a * progression(app->t_scene, 0.05f, 0.4f);
    ombre_biseau(app, cadre, 22.f, 1.f);
    panneau(app, cadre, 22.f, (sfColor){22, 26, 48, 210}, (sfColor){12, 14, 28, 225},
        C_BORD);
    crochets(app, (sfFloatRect){cadre.left - 6.f, cadre.top - 6.f, cadre.width + 12.f,
        cadre.height + 12.f}, 18.f, avec_alpha(C_CYAN, 0.45f));
    dy = apparaitre(app, a, 0);
    titre_section(app, "01", "NOM", (sfVector2f){FX, 218.f + dy}, FW, C_CYAN);
    c->nom.focus = c->focus == FOCUS_NOM && actif;
    if (champ_texte(app, &c->nom, (sfFloatRect){FX, 232.f + dy, FW, 50.f},
        "Ex : Aragorn", NOM_LONGUEUR))
        c->focus = 0;
    dy = apparaitre(app, a, 1);
    titre_section(app, "02", "STATISTIQUES DE BASE", (sfVector2f){FX, 304.f + dy}, FW,
        C_CYAN);
    for (int i = 0; i < NB_STATS; i++) {
        dy = apparaitre(app, a, 2 + i);
        if (glissiere(app, &c->stats[i], (sfFloatRect){FX, 314.f + i * 35.f + dy, FW,
            33.f}, LABELS_STATS[i], &c->valeurs[i], couleur_stat((stat_t)i),
            c->focus == FOCUS_STAT + i))
            c->focus = FOCUS_STAT + i;
    }
    dy = apparaitre(app, a, 5);
    titre_section(app, "03", "CLASSE", (sfVector2f){FX, 471.f + dy}, FW, C_CYAN);
    creation_onglets(app, c, 482.f + dy);
    if (yugioh_actif(app))
        creation_main_cartes(app, c, 514.f + dy);
    else
        creation_grille_classes(app, c, 514.f + dy);
    bouton_creer(app, c, a, actif);
}

static void aide_clavier(app_t *app, float a)
{
    static const char *touches[4] = {"Tab", "‹ ›", "Maj", "Échap"};
    static const char *actions[4] = {"naviguer", "ajuster", "×10 · catégorie", "retour"};
    float total = 0.f;
    float x = 0.f;
    float w = 0.f;

    for (int i = 0; i < 4; i++)
        total += largeur_touche(app, touches[i]) + 8.f + largeur_texte(app, POLICE_NORMALE,
            actions[i], 13) + 22.f;
    /* Dans un aperçu étroit, la ligne est poussée vers la gauche pour ne pas déborder. */
    x = fminf(AX + 26.f, AX + AW - 10.f - (total - 22.f));
    app->alpha = a * progression(app->t_scene, 0.6f, 0.4f);
    for (int i = 0; i < 4; i++) {
        w = largeur_touche(app, touches[i]);
        touche_clavier(app, touches[i], (sfVector2f){x + w / 2.f, 704.f}, C_CYAN, 0.f);
        x += w + 8.f;
        x += texte(app, POLICE_NORMALE, actions[i], 13, (sfVector2f){x, 704.f},
            C_TEXTE_FAIBLE, ALIGNE_GAUCHE) + 22.f;
    }
    app->alpha = a;
}

/* Composition de référence (1280 de large) : cadre du formulaire, écart, aperçu */
#define CADRE_FORM_L 632.f
#define ECART_PANNEAUX 36.f
#define COMPO_G 48.f
#define COMPO_D 1232.f
#define MARGE_ECRAN 24.f

/*
** ╭─ creation_geometrie ─────────────────────────────────────────────────
** │ Formulaire et aperçu se partagent la largeur visible.
** │
** │ - à 1280 et au-delà : la composition d'origine
** │ - plus étroit : les deux panneaux rétrécissent dans les mêmes
** │   proportions (les textes gardent leur taille)
** ╰──────────────────────────────────────────────────────────────────────
*/
void creation_geometrie(app_t *app, creation_t *c)
{
    float g = fmaxf(COMPO_G, app->ecran.left + MARGE_ECRAN);
    float d = fminf(COMPO_D, app->ecran.left + app->ecran.width - MARGE_ECRAN);
    float k = (d - g) / (COMPO_D - COMPO_G);
    float cadre = CADRE_FORM_L * k;

    c->fx = g + 16.f;
    c->fw = cadre - 32.f;
    c->aw = d - (g + cadre + ECART_PANNEAUX * k);
    c->ax = d - c->aw;
}

void scene_creation(app_t *app)
{
    creation_t *c = &app->creation;
    float a = app->alpha;
    int actif = c->sortie < 0.f;
    int bloque = app->souris_bloquee;
    char surtitre[32];

    creation_geometrie(app, c);
    if (!actif) {
        c->sortie -= app->dt;
        if (c->sortie <= 0.f)
            changer_scene(app, SCENE_MENU);
        app->souris_bloquee = 1;
    } else {
        creation_clavier(app, c);
    }
    creation_suivre_classe(app, c);
    snprintf(surtitre, sizeof(surtitre), "RECRUE %02d / %d",
        app->equipe.len < MAX_PERSONNAGES ? app->equipe.len + 1 : MAX_PERSONNAGES,
        MAX_PERSONNAGES);
    en_tete_surtitre(app, &c->retour, surtitre, "Nouveau personnage",
        "Donne-lui un nom, des statistiques et une classe.");
    formulaire(app, c, a, actif);
    creation_apercu(app, c, a);
    aide_clavier(app, a);
    app->souris_bloquee = bloque;
    app->alpha = a;
}
