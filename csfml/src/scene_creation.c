/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"

#define FX 64.f
#define FW 600.f
#define DUREE_SORTIE_CREATION 1.f

static const char *LABELS_STATS[3] = {"Points de vie", "Attaque", "Défense"};
static const char *DESCRIPTIONS[3] = {"Corps à corps", "Magie", "Distance"};

void entrer_creation(app_t *app)
{
    creation_t *c = &app->creation;

    memset(c, 0, sizeof(creation_t));
    c->valeurs[0] = 50;
    c->valeurs[1] = 30;
    c->valeurs[2] = 20;
    c->classe = GUERRIER;
    c->sortie = -1.f;
}

/** Fait apparaître l'élément `i` en cascade et renvoie son décalage vertical. */
static float apparaitre(app_t *app, float base, int i)
{
    float d = 0.12f + i * 0.05f;

    app->alpha = base * progression(app->t_scene, d, 0.35f);
    return 18.f * (1.f - sortie_cubique(progression(app->t_scene, d, 0.5f)));
}

static void erreur_nom(app_t *app, creation_t *c, const char *message)
{
    notifier(app, "✗", message, C_ROUGE);
    c->nom.secousse = 1.f;
    c->focus = 0;
}

/** Équivalent graphique de creer_personnage(). */
static void creer(app_t *app, creation_t *c)
{
    personnage_t p = {0};
    char msg[96];

    if (c->nom.texte[0] == '\0') {
        erreur_nom(app, c, "Donne un nom à ton personnage");
        return;
    }
    if (trouver_personnage(app->equipe.data, app->equipe.len, c->nom.texte) != -1) {
        erreur_nom(app, c, "Ce nom est déjà pris");
        return;
    }
    if (app->equipe.len >= MAX_PERSONNAGES) {
        notifier(app, "✗", "Nombre maximum de personnages atteint", C_ROUGE);
        return;
    }
    my_strcpy(p.nom, c->nom.texte);
    p.pv = c->valeurs[0];
    p.atq = c->valeurs[1];
    p.def = c->valeurs[2];
    p.classe = c->classe;
    if (roster_ajouter(&app->equipe, &p) == -1) {
        notifier(app, "✗", "Mémoire insuffisante", C_ROUGE);
        return;
    }
    snprintf(msg, sizeof(msg), "%s a été créé !", p.nom);
    notifier(app, "✔", msg, C_VERT);
    emettre(app, (sfVector2f){980.f, 205.f}, 80, couleur_classe(p.classe), 640.f, 4.5f);
    emettre(app, (sfVector2f){980.f, 205.f}, 30, C_BLANC, 420.f, 3.f);
    c->flash = 1.f;
    c->sortie = DUREE_SORTIE_CREATION;
    c->focus = -1;
}

static void ajuster(int *v, int delta)
{
    *v += delta;
    if (*v < 0)
        *v = 0;
    if (*v > 99)
        *v = 99;
}

static void clavier(app_t *app, creation_t *c)
{
    const int *k = app->in.touches;
    int pas = app->in.shift ? 10 : 1;

    if (k[sfKeyEscape]) {
        changer_scene(app, SCENE_MENU);
        return;
    }
    if (k[sfKeyTab])
        c->focus = (c->focus + (app->in.shift ? 5 : 1)) % 6;
    if (k[sfKeyDown] && c->focus < 5)
        c->focus++;
    if (k[sfKeyUp] && c->focus > 0)
        c->focus--;
    if (c->focus >= 1 && c->focus <= 3 && (k[sfKeyLeft] || k[sfKeyRight]))
        ajuster(&c->valeurs[c->focus - 1], k[sfKeyLeft] ? -pas : pas);
    if (c->focus == 4 && k[sfKeyLeft])
        c->classe = (c->classe + 2) % 3;
    if (c->focus == 4 && k[sfKeyRight])
        c->classe = (c->classe + 1) % 3;
    if (k[sfKeyEnter])
        creer(app, c);
}

static void dessiner_tuile(app_t *app, creation_t *c, classe_t i, sfFloatRect r)
{
    sfColor col = couleur_classe(i);
    float h = c->tuiles[i].survol;
    float sel = c->tuiles[i].appui;
    float cy = r.top + r.height / 2.f;

    lueur(app, (sfVector2f){r.left + r.width / 2.f, cy}, (sfVector2f){r.width * 0.75f,
        80.f}, avec_alpha(col, 0.24f * sel));
    ombre(app, r, 18.f, 0.7f + 0.5f * sel);
    rect_arrondi_bord(app, r, 18.f, melanger(C_SURFACE, melanger(C_SURFACE_HAUTE, col,
        0.16f), fmaxf(sel, h * 0.5f)), melanger(melanger(C_BORD, avec_alpha(col, 0.5f), h),
        avec_alpha(col, 0.95f), sel));
    if (c->focus == 4 && c->classe == i)
        rect_arrondi_bord(app, (sfFloatRect){r.left - 4.f, r.top - 4.f, r.width + 8.f,
            r.height + 8.f}, 22.f, C_TRANSPARENT, avec_alpha(C_BLANC, 0.3f));
    embleme(app, i, (sfVector2f){r.left + 42.f, cy}, 22.f * (1.f + 0.08f * sel),
        melanger(melanger(C_TEXTE_FAIBLE, col, h), melanger(col, C_BLANC, 0.3f), sel));
    texte(app, POLICE_GRAS, nom_classe(i), 19, (sfVector2f){r.left + 76.f, cy - 9.f},
        melanger(C_TEXTE_DOUX, C_TEXTE, fmaxf(sel, h)), ALIGNE_GAUCHE);
    texte(app, POLICE_NORMALE, DESCRIPTIONS[i], 13, (sfVector2f){r.left + 76.f,
        cy + 13.f}, C_TEXTE_FAIBLE, ALIGNE_GAUCHE);
    if (sel > 0.02f) {
        cercle(app, (sfVector2f){r.left + r.width - 20.f, r.top + 20.f}, 10.f
            * sortie_rebond(sel), col, C_TRANSPARENT, 0.f);
        texte_echelle(app, POLICE_SYMBOLES, "✔", 11, (sfVector2f){r.left + r.width
            - 20.f, r.top + 20.f}, C_BLANC, ALIGNE_CENTRE, sel);
    }
}

static void tuiles(app_t *app, creation_t *c, float y)
{
    float w = (FW - 28.f) / 3.f;
    sfFloatRect r;
    classe_t i = GUERRIER;
    int s = 0;

    for (int k = 0; k < 3; k++) {
        i = (classe_t)k;
        r = (sfFloatRect){FX + k * (w + 14.f), y, w, 80.f};
        s = survole(app, r);
        if (s)
            app->curseur_voulu = CURSEUR_MAIN;
        if (s && app->in.clic) {
            if (c->classe != i)
                emettre(app, (sfVector2f){r.left + r.width / 2.f, y + 40.f}, 16,
                    couleur_classe(i), 280.f, 3.f);
            c->classe = i;
            c->focus = 4;
        }
        c->tuiles[k].survol = approcher(c->tuiles[k].survol, s ? 1.f : 0.f, 14.f, app->dt);
        c->tuiles[k].appui = approcher(c->tuiles[k].appui, c->classe == i ? 1.f : 0.f,
            12.f, app->dt);
        maj_ressort(&c->lift[k], c->classe == i ? -6.f : (s ? -3.f : 0.f), 300.f, 18.f,
            app->dt);
        r.top += c->lift[k].x;
        dessiner_tuile(app, c, i, r);
    }
}

static void apercu(app_t *app, creation_t *c, float a)
{
    personnage_t p;
    float e = sortie_rebond(progression(app->t_scene, 0.25f, 0.7f));
    float envol = c->sortie >= 0.f ? entree_cubique(1.f - c->sortie / DUREE_SORTIE_CREATION) : 0.f;
    sfFloatRect r = {744.f, 100.f + sinf(app->temps * 1.6f) * 5.f + 40.f * (1.f - e)
        - 30.f * envol, 472.f, 510.f};

    memset(&p, 0, sizeof(personnage_t));
    my_strcpy(p.nom, c->nom.texte);
    p.pv = c->valeurs[0];
    p.atq = c->valeurs[1];
    p.def = c->valeurs[2];
    p.classe = c->classe;
    for (int i = 0; i < 3; i++)
        c->segments[i] = approcher(c->segments[i], (c->valeurs[i] + 4) / 5, 10.f, app->dt);
    app->alpha = a * progression(app->t_scene, 0.25f, 0.35f);
    etiquette(app, "APERÇU EN DIRECT", (sfVector2f){r.left + r.width / 2.f, r.top - 22.f},
        C_TEXTE_FAIBLE, ALIGNE_CENTRE);
    fiche(app, &p, r, c->segments);
    rect_arrondi(app, r, 28.f, avec_alpha(C_BLANC, 0.45f * c->flash));
    c->flash = approcher(c->flash, 0.f, 5.f, app->dt);
    texte(app, POLICE_NORMALE, "Tab pour naviguer  ·  flèches pour ajuster  ·  Maj = ×10",
        13, (sfVector2f){r.left + r.width / 2.f, 662.f}, C_TEXTE_FAIBLE, ALIGNE_CENTRE);
    app->alpha = a;
}

static void formulaire(app_t *app, creation_t *c, float a, int actif)
{
    const sfColor couleurs[3] = {C_VERT, C_ROUGE, C_BLEU};
    float dy = apparaitre(app, a, 0);

    etiquette(app, "NOM", (sfVector2f){FX, 212.f + dy}, C_TEXTE_FAIBLE, ALIGNE_GAUCHE);
    c->nom.focus = c->focus == 0 && actif;
    if (champ_texte(app, &c->nom, (sfFloatRect){FX, 230.f + dy, FW, 58.f},
        "Ex : Aragorn", 29))
        c->focus = 0;
    dy = apparaitre(app, a, 1);
    etiquette(app, "STATISTIQUES", (sfVector2f){FX, 322.f + dy}, C_TEXTE_FAIBLE,
        ALIGNE_GAUCHE);
    for (int i = 0; i < 3; i++) {
        dy = apparaitre(app, a, 2 + i);
        if (glissiere(app, &c->stats[i], (sfFloatRect){FX, 338.f + i * 52.f + dy, FW,
            48.f}, LABELS_STATS[i], &c->valeurs[i], couleurs[i], c->focus == 1 + i))
            c->focus = 1 + i;
    }
    dy = apparaitre(app, a, 5);
    etiquette(app, "CLASSE", (sfVector2f){FX, 514.f + dy}, C_TEXTE_FAIBLE, ALIGNE_GAUCHE);
    tuiles(app, c, 530.f + dy);
    dy = apparaitre(app, a, 6);
    if (bouton_plein(app, &c->creer, (sfFloatRect){FX, 632.f + dy, FW, 60.f},
        "Créer le personnage", "Entrée", C_CYAN, actif) && actif)
        creer(app, c);
    if (c->focus == 5)
        rect_arrondi_bord(app, (sfFloatRect){FX - 4.f, 628.f + dy, FW + 8.f, 68.f}, 22.f,
            C_TRANSPARENT, avec_alpha(C_BLANC, 0.3f));
}

void scene_creation(app_t *app)
{
    creation_t *c = &app->creation;
    float a = app->alpha;
    int actif = c->sortie < 0.f;
    int bloque = app->souris_bloquee;

    if (!actif) {
        c->sortie -= app->dt;
        if (c->sortie <= 0.f)
            changer_scene(app, SCENE_MENU);
        app->souris_bloquee = 1;
    } else {
        clavier(app, c);
    }
    en_tete(app, &c->retour, "Nouveau personnage",
        "Donne-lui un nom, des statistiques et une classe.");
    formulaire(app, c, a, actif);
    apercu(app, c, a);
    app->souris_bloquee = bloque;
    app->alpha = a;
}
