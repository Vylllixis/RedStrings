/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"

#define TITRE_Y 150.f

void entrerMenu(app_t *app)
{
    memset(&app->menu, 0, sizeof(menu_t));
}

/** Fil rouge qui se déroule depuis le centre, avec une perle qui le parcourt. */
static void filRouge(app_t *app, float y, float demi, float p)
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
    ligneEpaisse(app, pts, 64, 12.f, avecAlpha(C_ROUGE, 0.12f), 1);
    ligneEpaisse(app, pts, 64, 2.5f, C_ROUGE, 0);
    cercle(app, pts[0], 4.f, C_ROUGE, C_TRANSPARENT, 0.f);
    cercle(app, pts[63], 4.f, C_ROUGE, C_TRANSPARENT, 0.f);
    perle = (int)(fmodf(app->temps * 0.35f, 1.f) * 63.f);
    lueur(app, pts[perle], (sfVector2f){26.f, 26.f}, avecAlpha(C_ROUGE, 0.8f * p));
    lueur(app, pts[perle], (sfVector2f){7.f, 7.f}, avecAlpha(C_BLANC, 0.9f * p));
}

/** « REDSTRINGS » : chaque lettre tombe avec un léger rebond puis ondule. */
static void titre(app_t *app, float t)
{
    const char *mot = "REDSTRINGS";
    const unsigned int taille = 92;
    float avances[10];
    float total = 0.f;
    float x = 0.f;
    float y = 0.f;
    float e = 0.f;
    float d = 0.f;
    float a = app->alpha;
    char lettre[2] = {0, 0};

    for (int i = 0; i < 10; i++) {
        avances[i] = sfFont_getGlyph(app->polices[POLICE_GRAS], mot[i], taille,
            sfFalse, 0.f).advance + 4.f;
        total += avances[i];
    }
    x = LARGEUR / 2.f - total / 2.f;
    lueur(app, (sfVector2f){LARGEUR / 2.f, TITRE_Y}, (sfVector2f){520.f, 150.f},
        avecAlpha(C_ROUGE, 0.10f * progression(t, 0.f, 0.8f)));
    for (int i = 0; i < 10; i++) {
        d = 0.05f + i * 0.045f;
        e = sortieRebond(progression(t, d, 0.6f));
        y = TITRE_Y + 46.f * (1.f - e) + sinf(app->temps * 2.2f - i * 0.5f) * 3.f;
        lettre[0] = mot[i];
        app->alpha = a * progression(t, d, 0.25f);
        if (i < 3)
            lueur(app, (sfVector2f){x + avances[i] / 2.f, y}, (sfVector2f){60.f, 60.f},
                avecAlpha(C_ROUGE, 0.22f));
        texteEchelle(app, POLICE_GRAS, lettre, taille, (sfVector2f){x + avances[i] / 2.f, y},
            i < 3 ? C_ROUGE : C_TEXTE, ALIGNE_CENTRE, 0.7f + 0.3f * e);
        x += avances[i];
    }
    app->alpha = a;
    filRouge(app, TITRE_Y + 66.f, 300.f, sortieCubique(progression(t, 0.45f, 0.9f)));
    app->alpha = a * progression(t, 0.7f, 0.4f);
    texte(app, POLICE_NORMALE, "Crée tes personnages, puis fais-les s'affronter.", 18,
        (sfVector2f){LARGEUR / 2.f, TITRE_Y + 108.f + 10.f * (1.f - app->alpha)},
        C_TEXTE_DOUX, ALIGNE_CENTRE);
    app->alpha = a;
}

static void action(app_t *app, int choix)
{
    if (choix == 0 && app->nbPersos >= MAX_PERSONNAGES)
        notifier(app, "✗", "Nombre maximum de personnages atteint", C_ROUGE);
    else if (choix == 0)
        changerScene(app, SCENE_CREATION);
    if (choix == 1 && app->nbPersos == 0)
        notifier(app, "✗", "Aucun personnage : crée-en un d'abord", C_ROUGE);
    else if (choix == 1)
        changerScene(app, SCENE_LISTE);
    if (choix == 2 && app->nbPersos < 2)
        notifier(app, "✗", "Il faut au moins 2 personnages pour un combat", C_ROUGE);
    else if (choix == 2)
        changerScene(app, SCENE_SELECTION);
    if (choix == 3)
        changerScene(app, SCENE_AU_REVOIR);
}

static int toucheMenu(app_t *app)
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
    return -1;
}

static void boutonsMenu(app_t *app, float t)
{
    static const char *labels[4] = {"Ajouter un personnage",
        "Afficher un personnage", "Combat", "Quitter"};
    static const char *touches[4] = {"1", "2", "3", "0"};
    const sfColor accents[4] = {C_CYAN, C_BLEU, C_ROUGE, C_TEXTE_DOUX};
    const int actifs[4] = {app->nbPersos < MAX_PERSONNAGES, app->nbPersos > 0,
        app->nbPersos >= 2, 1};
    char infos[3][32];
    float a = app->alpha;
    float e = 0.f;
    int choix = toucheMenu(app);

    snprintf(infos[0], sizeof(infos[0]), "%d / %d", app->nbPersos, MAX_PERSONNAGES);
    snprintf(infos[1], sizeof(infos[1]), "%d héros", app->nbPersos);
    snprintf(infos[2], sizeof(infos[2]), "1 contre 1");
    for (int i = 0; i < 4; i++) {
        e = sortieCubique(progression(t, 0.4f + i * 0.07f, 0.55f));
        app->alpha = a * progression(t, 0.4f + i * 0.07f, 0.3f);
        if (bouton(app, &app->menu.boutons[i], (sfFloatRect){LARGEUR / 2.f - 230.f
            - 40.f * (1.f - e), 302.f + i * 74.f, 460.f, 60.f}, labels[i], touches[i],
            i < 3 ? infos[i] : NULL, accents[i], actifs[i]))
            choix = i;
    }
    app->alpha = a;
    if (choix >= 0)
        action(app, choix);
}

static void infobulle(app_t *app, const char *nom, sfVector2f pos)
{
    float w = largeurTexte(app, POLICE_GRAS, nom, 14) + 24.f;

    rectArrondiBord(app, (sfFloatRect){pos.x - w / 2.f, pos.y - 15.f, w, 30.f}, 10.f,
        C_SURFACE_HAUTE, C_BORD);
    texte(app, POLICE_GRAS, nom, 14, pos, C_TEXTE, ALIGNE_CENTRE);
}

/** Les 10 emplacements de l'équipe en bas du menu. */
static void equipe(app_t *app, float t)
{
    float a = app->alpha;
    float e = 0.f;
    float *h = app->menu.survolEquipe;
    sfVector2f c;
    char buf[48];

    app->alpha = a * progression(t, 0.8f, 0.4f);
    snprintf(buf, sizeof(buf), "ÉQUIPE  ·  %d / %d", app->nbPersos, MAX_PERSONNAGES);
    etiquette(app, buf, (sfVector2f){LARGEUR / 2.f, 628.f}, C_TEXTE_FAIBLE, ALIGNE_CENTRE);
    for (int i = 0; i < MAX_PERSONNAGES; i++) {
        e = sortieRebond(progression(t, 0.85f + i * 0.035f, 0.45f));
        c = (sfVector2f){LARGEUR / 2.f - 198.f + i * 44.f, 668.f};
        if (i >= app->nbPersos) {
            cercle(app, c, 15.f * e, C_TRANSPARENT, avecAlpha(C_BLANC, 0.1f), 1.5f);
            continue;
        }
        h[i] = approcher(h[i], survole(app, (sfFloatRect){c.x - 18.f, c.y - 18.f,
            36.f, 36.f}) ? 1.f : 0.f, 14.f, app->dt);
        avatar(app, &app->persos[i], (sfVector2f){c.x, c.y - 4.f * h[i]},
            16.f * e * (1.f + 0.15f * h[i]), app->persos[i].pv == 0);
    }
    for (int i = 0; i < app->nbPersos; i++) {
        if (h[i] < 0.01f)
            continue;
        app->alpha = a * h[i];
        infobulle(app, app->persos[i].name, (sfVector2f){LARGEUR / 2.f - 198.f + i * 44.f,
            624.f - 6.f * h[i]});
    }
    app->alpha = a;
}

void sceneMenu(app_t *app)
{
    titre(app, app->tScene);
    boutonsMenu(app, app->tScene);
    equipe(app, app->tScene);
}

void sceneAuRevoir(app_t *app)
{
    float t = app->tScene;
    float e = sortieRebond(progression(t, 0.05f, 0.6f));
    float a = app->alpha;

    lueur(app, (sfVector2f){LARGEUR / 2.f, 330.f}, (sfVector2f){460.f, 170.f},
        avecAlpha(C_ROUGE, 0.14f * e));
    texteEchelle(app, POLICE_GRAS, "À bientôt !", 68, (sfVector2f){LARGEUR / 2.f,
        330.f + 20.f * (1.f - e)}, C_TEXTE, ALIGNE_CENTRE, 0.85f + 0.15f * e);
    filRouge(app, 392.f, 200.f, sortieCubique(progression(t, 0.2f, 0.7f)));
    app->alpha = a * progression(t, 0.35f, 0.4f);
    texte(app, POLICE_NORMALE, "Merci d'avoir joué à Redstrings", 18,
        (sfVector2f){LARGEUR / 2.f, 432.f}, C_TEXTE_DOUX, ALIGNE_CENTRE);
    app->alpha = 1.f;
    rectArrondi(app, (sfFloatRect){-60.f, -60.f, LARGEUR + 120.f, HAUTEUR + 120.f}, 0.f,
        avecAlpha(C_NOIR, progression(t, 1.1f, 0.5f)));
    app->alpha = a;
    if (t > 1.65f)
        app->ouvert = 0;
}
