/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"

#define TITRE_Y 112.f
#define TAILLE_TITRE 84

/* Colonne des commandes et panneau d'équipe */
#define OX 150.f
#define OW 500.f
#define OY 290.f
#define EQ_RECT ((sfFloatRect){700.f, 262.f, 430.f, 326.f})

/* Pas de bouton à l'écran : seulement les touches 4 et 5, comme la console. */
#define CHOIX_SAUVEGARDER 4
#define CHOIX_CHARGER 5

/** Action mise en avant : le combat dès qu'il est possible, sinon l'ajout. */
static int option_principale(app_t *app)
{
    return app->equipe.len >= 2 ? 2 : 0;
}

void entrer_menu(app_t *app)
{
    memset(&app->menu, 0, sizeof(menu_t));
    app->menu.selection = option_principale(app);
}

/** Fil rouge qui se déroule depuis le centre, avec une perle qui le parcourt. */
static void fil_rouge(app_t *app, float y, float demi, float p)
{
    sfVector2f pts[64];
    float u = 0.f;
    float x = 0.f;
    int perle = 0;

    if (p <= 0.f)
        return;
    for (int i = 0; i < 64; i++) {
        u = i / 63.f;
        x = LARGEUR / 2.f - demi * p + 2.f * demi * p * u;
        pts[i] = (sfVector2f){x, y + sinf(x * 0.035f + app->temps * 3.f) * 4.f
            * sinf(PI * u)};
    }
    ligne_epaisse(app, pts, 64, 12.f, avec_alpha(C_ROUGE, 0.12f), 1);
    ligne_epaisse(app, pts, 64, 2.5f, C_ROUGE, 0);
    cercle(app, pts[0], 4.f, C_ROUGE, C_TRANSPARENT, 0.f);
    cercle(app, pts[63], 4.f, C_ROUGE, C_TRANSPARENT, 0.f);
    perle = (int)(fmodf(app->temps * 0.35f, 1.f) * 63.f);
    lueur(app, pts[perle], (sfVector2f){26.f, 26.f}, avec_alpha(C_ROUGE, 0.8f * p));
    lueur(app, pts[perle], (sfVector2f){7.f, 7.f}, avec_alpha(C_BLANC, 0.9f * p));
}

/** Surtitre et « ailes » lumineuses qui encadrent le logo. */
static void cadre_logo(app_t *app, float t)
{
    float e = sortie_cubique(progression(t, 0.f, 1.2f));
    float a = app->alpha;
    sfVector2f aile[2];

    app->alpha = a * e;
    losange(app, (sfVector2f){LARGEUR / 2.f - 138.f, 44.f}, 3.f, C_ROUGE);
    losange(app, (sfVector2f){LARGEUR / 2.f + 138.f, 44.f}, 3.f, C_ROUGE);
    etiquette(app, "COMBAT AU TOUR PAR TOUR", (sfVector2f){LARGEUR / 2.f, 44.f},
        melanger(C_ROUGE, C_TEXTE_DOUX, 0.35f), ALIGNE_CENTRE);
    for (int k = 0; k < 2; k++) {
        aile[0] = (sfVector2f){LARGEUR / 2.f + (k ? 1.f : -1.f) * (330.f + 40.f * (1.f - e)),
            TITRE_Y + 4.f};
        aile[1] = (sfVector2f){aile[0].x + (k ? 1.f : -1.f) * 220.f * e, TITRE_Y + 4.f};
        ligne_epaisse(app, aile, 2, 1.f, avec_alpha(C_ROUGE, 0.45f), 0);
        losange(app, aile[0], 4.f, C_ROUGE);
        losange(app, aile[1], 2.5f, avec_alpha(C_ROUGE, 0.6f));
    }
    app->alpha = a;
}

/** « REDSTRINGS » : chaque lettre tombe avec un léger rebond puis ondule. */
static void titre(app_t *app, float t)
{
    const char *mot = "REDSTRINGS";
    float avances[10];
    float total = 0.f;
    float x = 0.f;
    float y = 0.f;
    float e = 0.f;
    float d = 0.f;
    float a = app->alpha;
    char lettre[2] = {0, 0};

    for (int i = 0; i < 10; i++) {
        avances[i] = sfFont_getGlyph(app->polices[POLICE_GRAS], mot[i], TAILLE_TITRE,
            sfFalse, 0.f).advance + 6.f;
        total += avances[i];
    }
    cadre_logo(app, t);
    x = LARGEUR / 2.f - total / 2.f;
    lueur(app, (sfVector2f){LARGEUR / 2.f, TITRE_Y}, (sfVector2f){560.f, 150.f},
        avec_alpha(C_ROUGE, 0.12f * progression(t, 0.f, 0.8f)));
    app->contour = 2.f;
    for (int i = 0; i < 10; i++) {
        d = 0.05f + i * 0.045f;
        e = sortie_rebond(progression(t, d, 0.6f));
        y = TITRE_Y + 46.f * (1.f - e) + sinf(app->temps * 2.2f - i * 0.5f) * 3.f;
        lettre[0] = mot[i];
        app->alpha = a * progression(t, d, 0.25f);
        if (i < 3)
            lueur(app, (sfVector2f){x + avances[i] / 2.f, y}, (sfVector2f){64.f, 64.f},
                avec_alpha(C_ROUGE, 0.26f));
        texte_echelle(app, POLICE_GRAS, lettre, TAILLE_TITRE, (sfVector2f){x
            + avances[i] / 2.f, y}, i < 3 ? C_ROUGE : C_TEXTE, ALIGNE_CENTRE,
            0.7f + 0.3f * e);
        x += avances[i];
    }
    app->contour = 0.f;
    app->alpha = a;
    fil_rouge(app, TITRE_Y + 60.f, 300.f, sortie_cubique(progression(t, 0.45f, 0.9f)));
    app->alpha = a * progression(t, 0.7f, 0.4f);
    texte(app, POLICE_NORMALE, "Crée tes personnages, puis fais-les s'affronter.", 18,
        (sfVector2f){LARGEUR / 2.f, TITRE_Y + 100.f + 10.f * (1.f - app->alpha)},
        C_TEXTE_DOUX, ALIGNE_CENTRE);
    app->alpha = a;
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
    notifier(app, "✔", msg, C_VERT);
}

static void action(app_t *app, int choix)
{
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

static int touche_menu(app_t *app)
{
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

static int nb_ko(app_t *app)
{
    int n = 0;

    for (int i = 0; i < app->equipe.len; i++)
        n += est_ko(&app->equipe.data[i]);
    return n;
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
    o[2] = (option_t){"3", "Combat", "Un attaquant, un défenseur, tour par tour",
        "Il faut au moins 2 personnages", "", C_ROUGE, n >= 2};
    o[3] = (option_t){"0", "Quitter", NULL, NULL, "", C_TEXTE_DOUX, 1};
    snprintf(o[0].info, sizeof(o[0].info), "%d / %d", n, MAX_PERSONNAGES);
    snprintf(o[1].info, sizeof(o[1].info), "%d héros", n);
    snprintf(o[2].info, sizeof(o[2].info), "%d en forme", n - nb_ko(app));
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

static void commandes(app_t *app, float t)
{
    option_t o[4];
    float a = app->alpha;
    float e = 0.f;
    float y = 0.f;
    int choix = touche_menu(app);
    int principal = option_principale(app);
    const int *k = app->in.touches;

    app->menu.souris_bouge = app->in.souris.x != app->menu.souris_prec.x
        || app->in.souris.y != app->menu.souris_prec.y;
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

            sfRenderWindow_drawPrimitives(app->fenetre, filet, 2, sfLines, NULL);
        }
        if (dessiner_option(app, i, (sfFloatRect){OX - 40.f * (1.f - e), y, OW,
            i < 3 ? 64.f : 46.f}, &o[i], i == principal))
            choix = i;
    }
    app->alpha = a;
    if (choix >= 0)
        action(app, choix);
}

/** Emplacement d'équipe : personnage réel, ou place libre (la suivante pulse). */
static void emplacement(app_t *app, int i, sfVector2f c, float e)
{
    personnage_t *p = NULL;
    float *h = app->menu.survol_equipe;
    sfColor col;
    float mort = 0.f;
    char nom[32];

    if (i >= app->equipe.len) {
        hexagone(app, c, 26.f * e, 0.f, avec_alpha(C_BLANC, 0.02f),
            avec_alpha(C_BLANC, 0.08f));
        if (i == app->equipe.len) {
            hexagone(app, c, 26.f * e, 0.f, C_TRANSPARENT, avec_alpha(C_CYAN,
                0.35f + 0.35f * sinf(app->temps * 3.f)));
            texte(app, POLICE_GRAS, "+", 22, c, avec_alpha(C_CYAN, 0.8f), ALIGNE_CENTRE);
        }
        return;
    }
    p = &app->equipe.data[i];
    mort = est_ko(p) ? 1.f : 0.f;
    col = griser(couleur_classe(p->classe), mort);
    h[i] = approcher(h[i], survole(app, (sfFloatRect){c.x - 32.f, c.y - 32.f, 64.f,
        76.f}) ? 1.f : 0.f, 14.f, app->dt);
    c.y -= 4.f * h[i];
    lueur(app, c, (sfVector2f){70.f + 20.f * h[i], 70.f + 20.f * h[i]},
        avec_alpha(col, (0.18f + 0.2f * h[i]) * (1.f - 0.6f * mort)));
    hexagone(app, c, (28.f + 2.f * h[i]) * e, 0.f, melanger((sfColor){14, 16, 32, 255},
        col, 0.2f), avec_alpha(col, 0.6f + 0.4f * h[i]));
    embleme(app, p->classe, c, 13.f * e, melanger(col, C_BLANC, 0.5f));
    tronquer(app, POLICE_GRAS, 12, p->nom, 72.f, nom);
    texte(app, POLICE_GRAS, nom, 12, (sfVector2f){c.x, c.y + 44.f},
        melanger(C_TEXTE_DOUX, C_TEXTE, h[i]), ALIGNE_CENTRE);
    if (mort > 0.f)
        pastille(app, "K.O.", (sfVector2f){c.x, c.y - 30.f}, C_ROUGE, 1);
}

/** Panneau de l'équipe : compte, état, emplacements et sauvegarde. */
static void panneau_equipe(app_t *app, float t)
{
    sfFloatRect r = EQ_RECT;
    float a = app->alpha;
    float e = sortie_cubique(progression(t, 0.55f, 0.6f));
    float sw = (r.width - 48.f - 9 * 4.f) / MAX_PERSONNAGES;
    float x = 0.f;
    char buf[48];

    r.left += 40.f * (1.f - e);
    app->alpha = a * progression(t, 0.55f, 0.4f);
    ombre_biseau(app, r, 22.f, 1.2f);
    panneau(app, r, 22.f, (sfColor){24, 29, 52, 225}, (sfColor){12, 14, 28, 235},
        C_BORD);
    crochets(app, (sfFloatRect){r.left - 6.f, r.top - 6.f, r.width + 12.f, r.height
        + 12.f}, 18.f, avec_alpha(C_ROUGE, 0.5f));
    etiquette(app, "ÉQUIPE", (sfVector2f){r.left + 24.f, r.top + 30.f}, C_TEXTE_DOUX,
        ALIGNE_GAUCHE);
    snprintf(buf, sizeof(buf), "/ %d", MAX_PERSONNAGES);
    x = r.left + r.width - 24.f - texte(app, POLICE_NORMALE, buf, 15, (sfVector2f){
        r.left + r.width - 24.f, r.top + 32.f}, C_TEXTE_FAIBLE, ALIGNE_DROITE) - 6.f;
    snprintf(buf, sizeof(buf), "%d", app->equipe.len);
    texte(app, POLICE_GRAS, buf, 28, (sfVector2f){x, r.top + 30.f}, C_TEXTE,
        ALIGNE_DROITE);
    snprintf(buf, sizeof(buf), "%d en forme  •  %d K.O.", app->equipe.len - nb_ko(app),
        nb_ko(app));
    texte(app, POLICE_NORMALE, app->equipe.len ? buf : "Aucun personnage recruté", 13,
        (sfVector2f){r.left + 24.f, r.top + 56.f}, C_TEXTE_FAIBLE, ALIGNE_GAUCHE);
    for (int i = 0; i < MAX_PERSONNAGES; i++)
        panneau(app, (sfFloatRect){r.left + 24.f + i * (sw + 4.f), r.top + 74.f, sw, 4.f},
            0.f, i < app->equipe.len ? couleur_classe(app->equipe.data[i].classe)
            : avec_alpha(C_BLANC, 0.08f), i < app->equipe.len
            ? couleur_classe(app->equipe.data[i].classe) : avec_alpha(C_BLANC, 0.08f),
            C_TRANSPARENT);
    for (int i = 0; i < MAX_PERSONNAGES; i++)
        emplacement(app, i, (sfVector2f){r.left + 57.f + (i % 5) * 79.f,
            r.top + 126.f + (i / 5) * 86.f}, sortie_rebond(progression(t, 0.7f + i * 0.04f,
            0.45f)));
    x = r.left + 24.f;
    for (int k = 0; k < 2; k++) {
        touche_clavier(app, k ? "5" : "4", (sfVector2f){x + 13.f, r.top + r.height - 24.f},
            C_VERT, 0.f);
        x += 34.f + texte(app, POLICE_NORMALE, k ? "Charger" : "Sauvegarder", 13,
            (sfVector2f){x + 34.f, r.top + r.height - 24.f}, C_TEXTE_DOUX,
            ALIGNE_GAUCHE) + 22.f;
    }
    texte(app, POLICE_NORMALE, FICHIER_SAUVEGARDE, 12, (sfVector2f){r.left + r.width
        - 24.f, r.top + r.height - 24.f}, C_TEXTE_FAIBLE, ALIGNE_DROITE);
    app->alpha = a;
}

/** Rappel des touches en bas de l'écran. */
static void aide_menu(app_t *app, float t)
{
    static const char *touches[4] = {"↑", "↓", "Entrée", "Échap"};
    static const char *actions[4] = {"", "choisir", "valider", "quitter"};
    float a = app->alpha;
    float total = 0.f;
    float x = 0.f;
    float w = 0.f;

    for (int i = 0; i < 4; i++)
        total += largeur_touche(app, touches[i]) + 8.f + (actions[i][0]
            ? largeur_texte(app, POLICE_NORMALE, actions[i], 13) + 24.f : 0.f);
    x = LARGEUR / 2.f - total / 2.f;
    app->alpha = a * progression(t, 1.f, 0.4f);
    for (int i = 0; i < 4; i++) {
        w = largeur_touche(app, touches[i]);
        touche_clavier(app, touches[i], (sfVector2f){x + w / 2.f, 628.f}, C_CYAN, 0.f);
        x += w + 8.f;
        if (actions[i][0])
            x += texte(app, POLICE_NORMALE, actions[i], 13, (sfVector2f){x, 628.f},
                C_TEXTE_FAIBLE, ALIGNE_GAUCHE) + 24.f;
    }
    app->alpha = a;
}

void scene_menu(app_t *app)
{
    titre(app, app->t_scene);
    commandes(app, app->t_scene);
    panneau_equipe(app, app->t_scene);
    aide_menu(app, app->t_scene);
}

void scene_au_revoir(app_t *app)
{
    float t = app->t_scene;
    float e = sortie_rebond(progression(t, 0.05f, 0.6f));
    float a = app->alpha;

    lueur(app, (sfVector2f){LARGEUR / 2.f, 330.f}, (sfVector2f){460.f, 170.f},
        avec_alpha(C_ROUGE, 0.14f * e));
    texte_echelle(app, POLICE_GRAS, "À bientôt !", 68, (sfVector2f){LARGEUR / 2.f,
        330.f + 20.f * (1.f - e)}, C_TEXTE, ALIGNE_CENTRE, 0.85f + 0.15f * e);
    fil_rouge(app, 392.f, 200.f, sortie_cubique(progression(t, 0.2f, 0.7f)));
    app->alpha = a * progression(t, 0.35f, 0.4f);
    texte(app, POLICE_NORMALE, "Merci d'avoir joué à Redstrings", 18,
        (sfVector2f){LARGEUR / 2.f, 432.f}, C_TEXTE_DOUX, ALIGNE_CENTRE);
    app->alpha = 1.f;
    rect_arrondi(app, (sfFloatRect){-60.f, -60.f, LARGEUR + 120.f, HAUTEUR + 120.f}, 0.f,
        avec_alpha(C_NOIR, progression(t, 1.1f, 0.5f)));
    app->alpha = a;
    if (t > 1.65f)
        app->ouvert = 0;
}
