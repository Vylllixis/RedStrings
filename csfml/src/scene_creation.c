/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"

/* Colonne du formulaire */
#define FX 64.f
#define FW 600.f
/* Panneau d'aperçu */
#define AX 716.f
#define AY 74.f
#define AW 516.f
#define AH 610.f
#define SCENE_X (AX + AW / 2.f)
#define SCENE_Y 250.f
#define SOCLE_Y 338.f
#define DUREE_SORTIE_CREATION 1.f

static const char *LABELS_STATS[3] = {"Points de vie", "Attaque", "Défense"};
static const char *DESCRIPTIONS[NB_CLASSES] = {
    [GUERRIER] = "Corps à corps",
    [MAGE] = "Magie",
    [ARCHER] = "Distance",
};

void entrer_creation(app_t *app)
{
    creation_t *c = &app->creation;

    memset(c, 0, sizeof(creation_t));
    c->valeurs[0] = 50;
    c->valeurs[1] = 30;
    c->valeurs[2] = 20;
    c->classe = GUERRIER;
    c->classe_vue = GUERRIER;
    c->classe_prec = GUERRIER;
    c->t_classe = 10.f;
    c->sortie = -1.f;
}

/** Fait apparaître l'élément `i` en cascade et renvoie son décalage vertical. */
static float apparaitre(app_t *app, float base, int i)
{
    float d = 0.12f + i * 0.05f;

    app->alpha = base * progression(app->t_scene, d, 0.35f);
    return 18.f * (1.f - sortie_cubique(progression(app->t_scene, d, 0.5f)));
}

/**
 * Pourquoi le personnage ne peut pas être créé, ou NULL s'il peut l'être.
 * Seule source de ces règles à l'écran : le bouton et creer() l'utilisent.
 */
static const char *raison_invalide(app_t *app, const creation_t *c)
{
    if (c->nom.texte[0] == '\0')
        return "Donne un nom à ton personnage";
    if (trouver_personnage(app->equipe.data, app->equipe.len, c->nom.texte) != -1)
        return "Ce nom est déjà pris";
    if (app->equipe.len >= MAX_PERSONNAGES)
        return "Nombre maximum de personnages atteint";
    return NULL;
}

/** Équivalent graphique de creer_personnage(). */
static void creer(app_t *app, creation_t *c)
{
    const char *raison = raison_invalide(app, c);
    personnage_t p = {0};
    char msg[96];

    if (raison != NULL) {
        notifier(app, "✗", raison, C_ROUGE);
        if (app->equipe.len < MAX_PERSONNAGES) {
            c->nom.secousse = 1.f;
            c->focus = 0;
        }
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
    emettre(app, (sfVector2f){SCENE_X, SCENE_Y}, 90, couleur_classe(p.classe), 640.f, 4.5f);
    emettre(app, (sfVector2f){SCENE_X, SCENE_Y}, 30, C_BLANC, 420.f, 3.f);
    c->flash = 1.f;
    c->sortie = DUREE_SORTIE_CREATION;
    c->focus = -1;
}

static void ajuster(int *v, int delta)
{
    *v += delta;
    if (*v < STAT_MIN)
        *v = STAT_MIN;
    if (*v > STAT_MAX)
        *v = STAT_MAX;
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
        c->classe = (c->classe + NB_CLASSES - 1) % NB_CLASSES;
    if (c->focus == 4 && k[sfKeyRight])
        c->classe = (c->classe + 1) % NB_CLASSES;
    if (k[sfKeyEnter])
        creer(app, c);
}

/* ------------------------------------------------------------------ */
/*  Cartes de classe                                                   */
/* ------------------------------------------------------------------ */

/** Motif de fond propre à chaque classe : lames, constellation, cible. */
static void motif_classe(app_t *app, classe_t i, sfFloatRect r, sfColor col)
{
    static const float ETOILES[10] = {0.62f, 0.30f, 0.74f, 0.18f, 0.86f, 0.36f,
        0.80f, 0.62f, 0.68f, 0.52f};
    sfVector2f pts[5];
    sfVector2f c = {r.left + r.width - 38.f, r.top + 42.f};

    if (i == GUERRIER) {
        for (int k = 0; k < 5; k++) {
            pts[0] = (sfVector2f){r.left + r.width * 0.52f + k * 16.f, r.top + 76.f};
            pts[1] = (sfVector2f){pts[0].x + 30.f, r.top + 12.f};
            ligne_epaisse(app, pts, 2, k == 2 ? 3.f : 1.5f, col, 0);
        }
    } else if (i == MAGE) {
        for (int k = 0; k < 5; k++)
            pts[k] = (sfVector2f){r.left + r.width * ETOILES[2 * k],
                r.top + r.height * ETOILES[2 * k + 1] * 0.8f};
        ligne_epaisse(app, pts, 5, 1.f, col, 0);
        for (int k = 0; k < 5; k++)
            losange(app, pts[k], k == 1 ? 3.5f : 2.2f, col);
    } else {
        for (int k = 1; k <= 3; k++)
            arc_cercle(app, c, 9.f * k, 1.2f, (sfVector2f){0.f, 2.f * PI}, col, 0);
        pts[0] = (sfVector2f){c.x - 34.f, c.y};
        pts[1] = (sfVector2f){c.x + 30.f, c.y};
        ligne_epaisse(app, pts, 2, 1.f, col, 0);
        pts[0] = (sfVector2f){c.x, c.y - 34.f};
        pts[1] = (sfVector2f){c.x, c.y + 34.f};
        ligne_epaisse(app, pts, 2, 1.f, col, 0);
    }
}

static void carte_classe(app_t *app, creation_t *c, classe_t i, sfFloatRect r)
{
    sfColor col = couleur_classe(i);
    float h = c->tuiles[i].survol;
    float sel = c->tuiles[i].appui;
    sfVector2f med = {r.left + 38.f, r.top + 40.f};

    if (sel > 0.01f)
        lueur(app, (sfVector2f){r.left + r.width / 2.f, r.top + r.height / 2.f},
            (sfVector2f){r.width * 0.8f, r.height * 0.9f}, avec_alpha(col, 0.22f * sel));
    ombre_biseau(app, r, 14.f, 0.7f + 0.5f * sel);
    panneau(app, r, 14.f, melanger(C_SURFACE_HAUTE, col, 0.05f * h + 0.16f * sel),
        melanger(C_SURFACE, C_FOND, 0.4f), melanger(melanger(C_BORD,
        avec_alpha(col, 0.5f), h), col, sel));
    motif_classe(app, i, r, avec_alpha(col, 0.10f + 0.10f * h + 0.22f * sel));
    hexagone(app, med, 25.f + 2.f * sel, 0.f, melanger((sfColor){14, 16, 32, 255}, col,
        0.12f + 0.18f * sel), avec_alpha(col, 0.35f + 0.65f * fmaxf(sel, h)));
    embleme(app, i, med, 14.f * (1.f + 0.1f * sel), melanger(melanger(C_TEXTE_FAIBLE,
        col, fmaxf(h, 0.4f)), melanger(col, C_BLANC, 0.45f), sel));
    app->espacement = 1.8f;
    texte(app, POLICE_GRAS, nom_classe_maj(i), 15, (sfVector2f){r.left + 18.f,
        r.top + 82.f}, melanger(C_TEXTE_DOUX, melanger(col, C_BLANC, 0.6f), fmaxf(sel, h)),
        ALIGNE_GAUCHE);
    app->espacement = 0.f;
    texte(app, POLICE_NORMALE, DESCRIPTIONS[i], 12, (sfVector2f){r.left + 18.f,
        r.top + 99.f}, C_TEXTE_FAIBLE, ALIGNE_GAUCHE);
    if (sel > 0.02f) {
        rect_arrondi(app, (sfFloatRect){r.left, r.top + r.height - 3.f,
            (r.width - 14.f) * sortie_cubique(sel), 3.f}, 0.f, col);
        losange(app, (sfVector2f){r.left + r.width - 20.f, r.top + 20.f},
            10.f * sortie_rebond(sel), col);
        texte_echelle(app, POLICE_SYMBOLES, "✔", 10, (sfVector2f){r.left + r.width
            - 20.f, r.top + 20.f}, C_BLANC, ALIGNE_CENTRE, sel);
    }
    if (c->focus == 4 && c->classe == i)
        crochets(app, (sfFloatRect){r.left - 5.f, r.top - 5.f, r.width + 10.f,
            r.height + 10.f}, 14.f, avec_alpha(C_BLANC, 0.6f + 0.3f
            * sinf(app->temps * 5.f)));
}

static void cartes_classes(app_t *app, creation_t *c, float y)
{
    float w = (FW - 12.f * (NB_CLASSES - 1)) / NB_CLASSES;
    sfFloatRect r;
    classe_t i = GUERRIER;
    int s = 0;

    for (int k = 0; k < NB_CLASSES; k++) {
        i = (classe_t)k;
        r = (sfFloatRect){FX + k * (w + 12.f), y, w, 112.f};
        s = survole(app, r);
        if (s)
            app->curseur_voulu = CURSEUR_MAIN;
        if (s && app->in.clic) {
            c->classe = i;
            c->focus = 4;
        }
        c->tuiles[k].survol = approcher(c->tuiles[k].survol, s ? 1.f : 0.f, 14.f, app->dt);
        c->tuiles[k].appui = approcher(c->tuiles[k].appui, c->classe == i ? 1.f : 0.f,
            12.f, app->dt);
        maj_ressort(&c->lift[k], c->classe == i ? -6.f : (s ? -3.f : 0.f), 300.f, 18.f,
            app->dt);
        r.top += c->lift[k].x;
        carte_classe(app, c, i, r);
    }
}

/* ------------------------------------------------------------------ */
/*  Hologramme de l'aperçu                                             */
/* ------------------------------------------------------------------ */

/** Guerrier : éclats de lame en couronne, pointés vers l'extérieur. */
static void aura_guerrier(app_t *app, sfVector2f c, sfColor col, float t)
{
    static const float ECLAT[6] = {0.f, -1.f, 0.28f, 0.6f, -0.28f, 0.6f};
    float a = 0.f;
    float r = 0.f;

    for (int k = 0; k < 8; k++) {
        a = t * 0.18f + k * PI / 4.f;
        r = 104.f + 6.f * sinf(t * 2.f + k);
        polygone(app, ECLAT, 3, (sfVector2f){c.x + cosf(a) * r, c.y + sinf(a) * r},
            k % 2 ? 9.f : 14.f, a + PI / 2.f, avec_alpha(col, k % 2 ? 0.45f : 0.8f));
    }
}

/** Mage : cercle runique en pointillés et astres en orbite inclinée. */
static void aura_mage(app_t *app, sfVector2f c, sfColor col, float t)
{
    float a = 0.f;
    float z = 0.f;
    sfVector2f p;

    for (int k = 0; k < 12; k++) {
        a = -t * 0.3f + k * PI / 6.f;
        arc_cercle(app, c, 122.f, 1.6f, (sfVector2f){a, a + 0.32f},
            avec_alpha(col, 0.55f), 1);
    }
    for (int k = 0; k < 5; k++) {
        a = t * 0.9f + k * 2.f * PI / 5.f;
        z = 0.5f + 0.5f * sinf(a);
        p = (sfVector2f){c.x + cosf(a) * 132.f, c.y + sinf(a) * 34.f + 10.f};
        lueur(app, p, (sfVector2f){18.f + 10.f * z, 18.f + 10.f * z},
            avec_alpha(col, 0.35f + 0.4f * z));
        losange(app, p, 2.5f + 2.5f * z, melanger(col, C_BLANC, 0.5f));
    }
}

/** Archer : réticule qui se resserre et graduations de visée. */
static void aura_archer(app_t *app, sfVector2f c, sfColor col, float t)
{
    float e = 78.f + 8.f * sinf(t * 2.4f);
    float sx = 0.f;
    float sy = 0.f;
    sfVector2f l[3];

    for (int k = 0; k < 4; k++) {
        sx = k % 2 ? 1.f : -1.f;
        sy = k < 2 ? -1.f : 1.f;
        l[0] = (sfVector2f){c.x + sx * e, c.y + sy * (e - 22.f)};
        l[1] = (sfVector2f){c.x + sx * e, c.y + sy * e};
        l[2] = (sfVector2f){c.x + sx * (e - 22.f), c.y + sy * e};
        ligne_epaisse(app, l, 3, 2.f, avec_alpha(col, 0.8f), 0);
    }
    for (int k = 0; k < 4; k++) {
        l[0] = (sfVector2f){c.x + cosf(k * PI / 2.f) * 112.f,
            c.y + sinf(k * PI / 2.f) * 112.f};
        l[1] = (sfVector2f){c.x + cosf(k * PI / 2.f) * 140.f,
            c.y + sinf(k * PI / 2.f) * 140.f};
        ligne_epaisse(app, l, 2, 1.5f, avec_alpha(col, 0.6f), 0);
    }
    graduations(app, c, 122.f, 72, 4.f, -t * 0.12f, avec_alpha(col, 0.35f));
}

/** Socle lumineux et cône de projection de l'hologramme. */
static void projecteur(app_t *app, sfColor col)
{
    sfVertex cone[4] = {
        {{SCENE_X - 110.f, SOCLE_Y}, avec_alpha(col, 0.16f * app->alpha), {0, 0}},
        {{SCENE_X + 110.f, SOCLE_Y}, avec_alpha(col, 0.16f * app->alpha), {0, 0}},
        {{SCENE_X + 60.f, SCENE_Y - 120.f}, avec_alpha(col, 0.f), {0, 0}},
        {{SCENE_X - 60.f, SCENE_Y - 120.f}, avec_alpha(col, 0.f), {0, 0}}
    };
    sfVertex trait[2];
    float y = 0.f;
    float f = 0.f;

    sfRenderWindow_drawPrimitives(app->fenetre, cone, 4, sfQuads, &app->additif);
    for (int k = 0; k < 7; k++) {
        f = fmodf(k / 7.f + app->temps * 0.25f, 1.f);
        y = lerpf(SOCLE_Y - 6.f, SCENE_Y - 110.f, f);
        trait[0] = (sfVertex){{lerpf(SCENE_X - 108.f, SCENE_X - 62.f, f), y},
            avec_alpha(col, 0.22f * (1.f - f) * app->alpha), {0, 0}};
        trait[1] = (sfVertex){{lerpf(SCENE_X + 108.f, SCENE_X + 62.f, f), y},
            trait[0].color, {0, 0}};
        sfRenderWindow_drawPrimitives(app->fenetre, trait, 2, sfLines, &app->additif);
    }
    lueur(app, (sfVector2f){SCENE_X, SOCLE_Y}, (sfVector2f){170.f, 34.f},
        avec_alpha(col, 0.35f));
    ellipse(app, (sfVector2f){SCENE_X, SOCLE_Y}, (sfVector2f){118.f, 18.f}, 2.f,
        avec_alpha(col, 0.7f));
    ellipse(app, (sfVector2f){SCENE_X, SOCLE_Y + 8.f}, (sfVector2f){142.f, 24.f}, 1.f,
        avec_alpha(col, 0.25f));
}

/** Suit les changements de classe pour déclencher la transition. */
static void suivre_classe(app_t *app, creation_t *c)
{
    c->t_classe += app->dt;
    if (c->classe == c->classe_vue)
        return;
    c->classe_prec = c->classe_vue;
    c->classe_vue = c->classe;
    c->t_classe = 0.f;
    emettre(app, (sfVector2f){SCENE_X, SCENE_Y}, 26, couleur_classe(c->classe),
        330.f, 3.f);
}

static void hologramme(app_t *app, creation_t *c)
{
    float t = app->temps;
    float m = progression(c->t_classe, 0.f, 0.35f);
    float pop = 0.75f + 0.25f * sortie_rebond(progression(c->t_classe, 0.f, 0.5f));
    float onde = progression(c->t_classe, 0.f, 0.7f);
    sfColor col = melanger(couleur_classe(c->classe_prec), couleur_classe(c->classe), m);
    sfVector2f centre = {SCENE_X, SCENE_Y + 4.f * sinf(t * 1.4f)};

    projecteur(app, col);
    lueur(app, centre, (sfVector2f){210.f, 210.f}, avec_alpha(col, 0.18f));
    if (onde < 1.f)
        arc_cercle(app, centre, 60.f + 170.f * sortie_cubique(onde), 3.f * (1.f - onde)
            + 1.f, (sfVector2f){0.f, 2.f * PI}, avec_alpha(col, 1.f - onde), 1);
    if (c->classe == GUERRIER)
        aura_guerrier(app, centre, col, t);
    else if (c->classe == MAGE)
        aura_mage(app, centre, col, t);
    else
        aura_archer(app, centre, col, t);
    for (int k = 0; k < 3; k++)
        arc_cercle(app, centre, 88.f, 2.f, (sfVector2f){t * 0.5f + k * 2.f * PI / 3.f,
            t * 0.5f + k * 2.f * PI / 3.f + 1.4f}, avec_alpha(col, 0.75f), 1);
    graduations(app, centre, 94.f, 60, 3.f, t * 0.08f, avec_alpha(C_BLANC, 0.18f));
    hexagone(app, centre, 64.f * pop, 0.f, melanger((sfColor){12, 14, 30, 235}, col,
        0.16f), avec_alpha(col, 0.95f));
    hexagone(app, centre, 54.f * pop, PI / 6.f, C_TRANSPARENT, avec_alpha(col, 0.3f));
    lueur(app, centre, (sfVector2f){90.f, 90.f}, avec_alpha(col, 0.3f));
    embleme(app, c->classe, centre, 34.f * pop, melanger(col, C_BLANC, 0.55f));
}

/* ------------------------------------------------------------------ */
/*  Panneau d'aperçu                                                   */
/* ------------------------------------------------------------------ */

static void stats_apercu(app_t *app, creation_t *c, sfColor col)
{
    static const char *noms[3] = {"POINTS DE VIE", "ATTAQUE", "DÉFENSE"};
    static const char *axes[3] = {"PV", "ATQ", "DEF"};
    const sfColor cs[3] = {C_VERT, C_ROUGE, C_BLEU};
    const sfVector2f radar = {AX + 108.f, 596.f};
    const float x0 = AX + 214.f;
    const float w = AW - 214.f - 26.f;
    float v[3];
    float y = 0.f;
    char buf[16];

    for (int i = 0; i < 3; i++) {
        c->affiche[i] = approcher(c->affiche[i], c->valeurs[i], 10.f, app->dt);
        c->segments[i] = approcher(c->segments[i], (c->valeurs[i] + 4) / 5, 10.f,
            app->dt);
        v[i] = c->affiche[i] / STAT_MAX;
    }
    radar_stats(app, radar, 62.f, v, col);
    for (int i = 0; i < 3; i++) {
        y = 596.f + sinf(-PI / 2.f + i * 2.f * PI / 3.f) * 80.f;
        texte(app, POLICE_GRAS, axes[i], 11, (sfVector2f){radar.x + cosf(-PI / 2.f
            + i * 2.f * PI / 3.f) * 82.f, y}, avec_alpha(cs[i], 0.8f), ALIGNE_CENTRE);
    }
    for (int i = 0; i < 3; i++) {
        y = 528.f + i * 54.f;
        etiquette(app, noms[i], (sfVector2f){x0, y}, C_TEXTE_DOUX, ALIGNE_GAUCHE);
        snprintf(buf, sizeof(buf), "%d%s", (int)roundf(c->affiche[i]), i == 2 ? " %" : "");
        texte(app, POLICE_GRAS, buf, 22, (sfVector2f){x0 + w, y - 1.f},
            melanger(cs[i], C_BLANC, 0.1f), ALIGNE_DROITE);
        barre_segments(app, (sfFloatRect){x0, y + 16.f, w, 10.f}, c->segments[i], cs[i]);
    }
}

static void apercu(app_t *app, creation_t *c, float a)
{
    personnage_t p = {0};
    float e = sortie_cubique(progression(app->t_scene, 0.2f, 0.7f));
    float envol = c->sortie >= 0.f
        ? entree_cubique(1.f - c->sortie / DUREE_SORTIE_CREATION) : 0.f;
    sfColor col = melanger(couleur_classe(c->classe_prec), couleur_classe(c->classe),
        progression(c->t_classe, 0.f, 0.35f));
    sfFloatRect r = {AX + 40.f * (1.f - e), AY - 30.f * envol, AW, AH};
    float cx = r.left + AW / 2.f;
    char buf[64];

    my_strcpy(p.nom, c->nom.texte);
    p.classe = c->classe;
    app->alpha = a * progression(app->t_scene, 0.2f, 0.35f);
    ombre_biseau(app, r, 26.f, 1.4f);
    panneau(app, r, 26.f, (sfColor){24, 29, 52, 235}, (sfColor){12, 14, 28, 245},
        melanger(C_BORD, avec_alpha(col, 0.5f), 0.5f));
    crochets(app, (sfFloatRect){r.left - 6.f, r.top - 6.f, r.width + 12.f,
        r.height + 12.f}, 22.f, avec_alpha(col, 0.7f));
    rect_arrondi(app, (sfFloatRect){r.left + 26.f, r.top, 140.f, 3.f}, 0.f, col);
    cercle(app, (sfVector2f){r.left + 34.f, r.top + 30.f}, 4.f, avec_alpha(C_ROUGE,
        0.5f + 0.5f * sinf(app->temps * 4.f)), C_TRANSPARENT, 0.f);
    etiquette(app, "APERÇU EN DIRECT", (sfVector2f){r.left + 46.f, r.top + 30.f},
        C_TEXTE_DOUX, ALIGNE_GAUCHE);
    etiquette(app, nom_classe_maj(c->classe), (sfVector2f){r.left + AW - 26.f,
        r.top + 30.f}, col, ALIGNE_DROITE);
    hologramme(app, c);
    tronquer(app, POLICE_GRAS, 32, p.nom[0] ? p.nom : "Sans nom", AW - 80.f, buf);
    app->espacement = 0.5f;
    texte(app, POLICE_GRAS, buf, 32, (sfVector2f){cx, 394.f - 30.f * envol},
        p.nom[0] ? C_TEXTE : C_TEXTE_FAIBLE, ALIGNE_CENTRE);
    app->espacement = 0.f;
    pastille(app, nom_classe_maj(c->classe), (sfVector2f){cx, 432.f - 30.f * envol},
        col, 0);
    texte(app, POLICE_NORMALE, DESCRIPTIONS[c->classe], 13, (sfVector2f){cx,
        460.f - 30.f * envol}, C_TEXTE_FAIBLE, ALIGNE_CENTRE);
    titre_section(app, NULL, "STATISTIQUES", (sfVector2f){r.left + 26.f,
        490.f - 30.f * envol}, AW - 52.f, col);
    stats_apercu(app, c, col);
    panneau(app, r, 26.f, avec_alpha(C_BLANC, 0.45f * c->flash),
        avec_alpha(C_BLANC, 0.2f * c->flash), C_TRANSPARENT);
    c->flash = approcher(c->flash, 0.f, 5.f, app->dt);
    app->alpha = a;
}

/* ------------------------------------------------------------------ */
/*  Formulaire                                                         */
/* ------------------------------------------------------------------ */

static void bouton_creer(app_t *app, creation_t *c, float a, int actif)
{
    const char *raison = raison_invalide(app, c);
    float dy = apparaitre(app, a, 6);
    int fini = c->sortie >= 0.f;
    int pret = actif && raison == NULL;

    if (bouton_plein(app, &c->creer, (sfFloatRect){FX, 628.f + dy, FW, 56.f},
        fini ? "Personnage créé" : "Créer le personnage", fini ? NULL : "Entrée",
        fini ? C_VERT : C_CYAN, pret || fini) && actif)
        creer(app, c);
    if (c->focus == 5)
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
    const sfColor couleurs[3] = {C_VERT, C_ROUGE, C_BLEU};
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
    c->nom.focus = c->focus == 0 && actif;
    if (champ_texte(app, &c->nom, (sfFloatRect){FX, 232.f + dy, FW, 50.f},
        "Ex : Aragorn", NOM_LONGUEUR))
        c->focus = 0;
    dy = apparaitre(app, a, 1);
    titre_section(app, "02", "STATISTIQUES", (sfVector2f){FX, 308.f + dy}, FW, C_CYAN);
    for (int i = 0; i < 3; i++) {
        dy = apparaitre(app, a, 2 + i);
        if (glissiere(app, &c->stats[i], (sfFloatRect){FX, 322.f + i * 44.f + dy, FW,
            40.f}, LABELS_STATS[i], &c->valeurs[i], couleurs[i], c->focus == 1 + i))
            c->focus = 1 + i;
    }
    dy = apparaitre(app, a, 5);
    titre_section(app, "03", "CLASSE", (sfVector2f){FX, 470.f + dy}, FW, C_CYAN);
    cartes_classes(app, c, 484.f + dy);
    bouton_creer(app, c, a, actif);
}

static void aide_clavier(app_t *app, float a)
{
    static const char *touches[4] = {"Tab", "‹ ›", "Maj", "Échap"};
    static const char *actions[4] = {"naviguer", "ajuster", "×10", "retour"};
    float x = AX + 26.f;
    float w = 0.f;

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

void scene_creation(app_t *app)
{
    creation_t *c = &app->creation;
    float a = app->alpha;
    int actif = c->sortie < 0.f;
    int bloque = app->souris_bloquee;
    char surtitre[32];

    if (!actif) {
        c->sortie -= app->dt;
        if (c->sortie <= 0.f)
            changer_scene(app, SCENE_MENU);
        app->souris_bloquee = 1;
    } else {
        clavier(app, c);
    }
    suivre_classe(app, c);
    snprintf(surtitre, sizeof(surtitre), "RECRUE %02d / %d",
        app->equipe.len < MAX_PERSONNAGES ? app->equipe.len + 1 : MAX_PERSONNAGES,
        MAX_PERSONNAGES);
    en_tete_surtitre(app, &c->retour, surtitre, "Nouveau personnage",
        "Donne-lui un nom, des statistiques et une classe.");
    formulaire(app, c, a, actif);
    apercu(app, c, a);
    aide_clavier(app, a);
    app->souris_bloquee = bloque;
    app->alpha = a;
}
