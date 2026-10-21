/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * L'arène : combat en équipe, mis en scène comme les combats « vus de
 * trois quarts » des anciens jeux de rôle. Ton équipe au premier plan en
 * bas à gauche, l'adversaire plus loin en haut à droite.
 *
 * Séparation stricte :
 *  - bataille.c décide (dégâts, soins, garde, tours, victoire) ;
 *  - ce fichier reçoit les événements et les JOUE dans le temps. Les PV
 *    affichés et le journal ne rattrapent l'état réel qu'au moment de
 *    l'impact ; aucune valeur n'est recalculée ni réappliquée ici.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"

/* Composition, en proportion de la fenêtre */
#define SOL_J_X (LARGEUR * 0.29f)
#define SOL_J_Y (HAUTEUR * 0.80f)
#define SOL_A_X (LARGEUR * 0.70f)
#define SOL_A_Y (HAUTEUR * 0.43f)
#define PLAT_J_RX (LARGEUR * 0.27f)
#define PLAT_J_RY (HAUTEUR * 0.105f)
#define PLAT_A_RX (LARGEUR * 0.185f)
#define PLAT_A_RY (HAUTEUR * 0.058f)
#define ECHELLE_ADVERSE 0.62f

/* Interface */
#define CMD_L 468.f
#define CMD_H 128.f
#define LIGNE_J 36.f
#define LIGNE_A 32.f
#define MARGE 36.f

/* Rythme */
#define DUREE_INTRO 1.1f
#define DELAI_ORDINATEUR 0.75f
#define DUREE_BANNIERE 1.f
#define DUREE_POPUP 1.15f

static const char *NOMS_ACTIONS[NB_ACTIONS] = {"Attaquer", "Soigner", "Défendre"};
static const char *TOUCHES_ACTIONS[NB_ACTIONS] = {"1", "2", "3"};

/* Durée de chaque événement et instant de son impact (secondes). */
static const float DUREES[] = {
    [EV_ATTAQUE] = 0.74f, [EV_SOIN] = 0.85f, [EV_GARDE] = 0.55f,
    [EV_GARDE_FIN] = 0.3f, [EV_KO] = 0.85f, [EV_VICTOIRE] = 0.5f,
};
static const float IMPACTS[] = {
    [EV_ATTAQUE] = 0.30f, [EV_SOIN] = 0.50f, [EV_GARDE] = 0.10f,
    [EV_GARDE_FIN] = 0.f, [EV_KO] = 0.05f, [EV_VICTOIRE] = 0.f,
};

/* Formations : position de chaque membre sur sa plateforme selon la taille
 * de l'équipe (x de -1 à 1, y de -1 = au fond à 1 = devant). */
static const float FORMATIONS[TAILLE_EQUIPE_MAX][TAILLE_EQUIPE_MAX][2] = {
    {{0.f, 0.f}},
    {{-0.48f, 0.3f}, {0.48f, -0.3f}},
    {{-0.64f, 0.35f}, {0.04f, -0.42f}, {0.66f, 0.3f}},
    {{-0.74f, 0.4f}, {-0.27f, -0.45f}, {0.22f, 0.45f}, {0.7f, -0.35f}},
};

static visuel_t *vis(arene_t *ar, ref_t r)
{
    return &ar->vis[r.equipe][r.index];
}

static int meme(ref_t a, ref_t b)
{
    return a.equipe == b.equipe && a.index == b.index;
}

static sfColor couleur_equipe(int e)
{
    return e == 0 ? C_CYAN : C_ROUGE;
}

static sfColor couleur_action(int a)
{
    const sfColor couleurs[NB_ACTIONS] = {C_ROUGE, C_SOIN, C_BLEU};

    return couleurs[a];
}

/* ------------------------------------------------------------------ */
/*  Démarrage                                                          */
/* ------------------------------------------------------------------ */

static void demarrer(app_t *app, arene_t *ar)
{
    bataille_t *b = &ar->bataille;
    const combattant_t *c = NULL;
    visuel_t *v = NULL;

    if (bataille_init(b, ar->equipes[0], ar->tailles[0], ar->equipes[1],
        ar->tailles[1]) == -1) {
        notifier(app, "✗", "Équipes invalides", C_ROUGE);
        changer_scene(app, SCENE_MENU);
        return;
    }
    memset(ar->vis, 0, sizeof(ar->vis));
    memset(ar->popups, 0, sizeof(ar->popups));
    for (int e = 0; e < NB_EQUIPES; e++) {
        for (int i = 0; i < b->equipes[e].taille; i++) {
            c = &b->equipes[e].membres[i];
            v = &ar->vis[e][i];
            v->pv_vis = c->pv;
            v->pv_barre = (float)c->pv;
            v->pv_fantome = (float)c->pv;
        }
    }
    ar->etat = VUE_INTRO;
    ar->t_etat = 0.f;
    ar->nb_file = 0;
    ar->i_file = 0;
    ar->journal_vu = b->nb_journal;
}

void entrer_arene(app_t *app)
{
    arene_t *ar = &app->arene;
    selection_t *s = &app->selection;

    memset(ar, 0, sizeof(arene_t));
    for (int e = 0; e < NB_EQUIPES; e++) {
        ar->tailles[e] = s->nb[e];
        for (int i = 0; i < s->nb[e]; i++)
            ar->equipes[e][i] = app->equipe.data[s->ids[e][i]];
    }
    demarrer(app, ar);
}

/* ------------------------------------------------------------------ */
/*  Popups                                                             */
/* ------------------------------------------------------------------ */

static void popup(arene_t *ar, sfVector2f pos, const char *texte_popup,
    sfColor couleur)
{
    popup_t *p = &ar->popups[0];

    for (int i = 0; i < MAX_POPUPS; i++) {
        if (!ar->popups[i].actif) {
            p = &ar->popups[i];
            break;
        }
    }
    *p = (popup_t){"", couleur, pos, 0.f, 1};
    snprintf(p->texte, sizeof(p->texte), "%s", texte_popup);
}

static void dessiner_popups(app_t *app, arene_t *ar)
{
    popup_t *p = NULL;
    float e = 0.f;
    float a = app->alpha;

    for (int i = 0; i < MAX_POPUPS; i++) {
        p = &ar->popups[i];
        if (!p->actif)
            continue;
        p->t += app->dt;
        if (p->t > DUREE_POPUP) {
            p->actif = 0;
            continue;
        }
        e = p->t < 0.14f ? sortie_rebond(p->t / 0.14f) : 1.f;
        app->alpha = a * (1.f - progression(p->t, DUREE_POPUP - 0.35f, 0.35f));
        app->contour = 3.f;
        texte_echelle(app, POLICE_GRAS, p->texte, 34, (sfVector2f){p->pos.x,
            p->pos.y - 46.f * sortie_cubique(progression(p->t, 0.f, DUREE_POPUP))},
            p->couleur, ALIGNE_CENTRE, 0.5f + 0.5f * e);
        app->contour = 0.f;
    }
    app->alpha = a;
}

/* ------------------------------------------------------------------ */
/*  Placement : formation, profondeur, entrée en scène                 */
/* ------------------------------------------------------------------ */

static float rayon_base(int e, int n)
{
    float r = n <= 2 ? 58.f : (n == 3 ? 52.f : 46.f);

    return e == 0 ? r : r * ECHELLE_ADVERSE;
}

static void placer(app_t *app, arene_t *ar)
{
    const bataille_t *b = &ar->bataille;
    float intro = ar->etat == VUE_INTRO ? ar->t_etat : DUREE_INTRO;
    int n = 0;
    float fx = 0.f;
    float fy = 0.f;
    float e = 0.f;
    visuel_t *v = NULL;

    for (int eq = 0; eq < NB_EQUIPES; eq++) {
        n = b->equipes[eq].taille;
        for (int i = 0; i < n; i++) {
            v = &ar->vis[eq][i];
            fx = FORMATIONS[n - 1][i][0];
            fy = FORMATIONS[n - 1][i][1];
            e = sortie_cubique(progression(intro, 0.15f + i * 0.08f + eq * 0.12f, 0.55f));
            v->sol = eq == 0
                ? (sfVector2f){SOL_J_X + fx * PLAT_J_RX * 0.8f - 320.f * (1.f - e),
                    SOL_J_Y + fy * PLAT_J_RY * 0.75f}
                : (sfVector2f){SOL_A_X + fx * PLAT_A_RX * 0.8f + 260.f * (1.f - e),
                    SOL_A_Y + fy * PLAT_A_RY * 0.75f};
            v->rayon = rayon_base(eq, n) * (1.f + 0.1f * fy);
            v->pos = (sfVector2f){v->sol.x, v->sol.y - v->rayon * 1.35f
                + sinf(app->temps * 1.6f + i + eq * 2.f) * 3.f};
        }
    }
}

/** Ordre de dessin : du fond (petit y au sol) vers l'avant. */
static int ordre_dessin(const arene_t *ar, ref_t ordre[NB_EQUIPES * TAILLE_EQUIPE_MAX])
{
    int n = 0;
    ref_t tmp;

    for (int e = 0; e < NB_EQUIPES; e++)
        for (int i = 0; i < ar->bataille.equipes[e].taille; i++)
            ordre[n++] = (ref_t){e, i};
    for (int i = 1; i < n; i++) {
        for (int j = i; j > 0 && ar->vis[ordre[j].equipe][ordre[j].index].sol.y
            < ar->vis[ordre[j - 1].equipe][ordre[j - 1].index].sol.y; j--) {
            tmp = ordre[j];
            ordre[j] = ordre[j - 1];
            ordre[j - 1] = tmp;
        }
    }
    return n;
}

/* ------------------------------------------------------------------ */
/*  Arène                                                              */
/* ------------------------------------------------------------------ */

static void disque(app_t *app, sfVector2f c, sfVector2f r, sfColor centre,
    sfColor bord)
{
    sfVertex v[34];
    float a = 0.f;

    v[0] = (sfVertex){c, avec_alpha(centre, app->alpha), {0, 0}};
    for (int i = 0; i <= 32; i++) {
        a = i * 2.f * PI / 32.f;
        v[i + 1] = (sfVertex){{c.x + cosf(a) * r.x, c.y + sinf(a) * r.y},
            avec_alpha(bord, app->alpha), {0, 0}};
    }
    sfRenderWindow_drawPrimitives(app->fenetre, v, 34, sfTriangleFan, NULL);
}

/** Anneau d'ellipse en tirets qui tourne (sol des plateformes). */
static void anneau_tirets(app_t *app, sfVector2f c, sfVector2f r, float rot,
    sfColor col)
{
    sfVector2f p[6];
    float a = 0.f;

    for (int k = 0; k < 16; k++) {
        for (int i = 0; i < 6; i++) {
            a = rot + k * 2.f * PI / 16.f + i * 0.045f;
            p[i] = (sfVector2f){c.x + cosf(a) * r.x, c.y + sinf(a) * r.y};
        }
        ligne_epaisse(app, p, 6, 2.f, col, 1);
    }
}

static void plateforme(app_t *app, sfVector2f c, sfVector2f r, sfColor bord,
    float apparition)
{
    sfVector2f rr = {r.x * apparition, r.y * apparition};

    lueur(app, c, (sfVector2f){rr.x * 1.3f, rr.y * 2.6f}, avec_alpha(bord, 0.16f));
    disque(app, (sfVector2f){c.x, c.y + rr.y * 0.22f}, (sfVector2f){rr.x * 1.02f,
        rr.y * 1.1f}, (sfColor){4, 5, 12, 200}, (sfColor){4, 5, 12, 0});
    disque(app, c, rr, (sfColor){26, 32, 64, 235}, (sfColor){12, 15, 32, 235});
    ellipse(app, c, rr, 2.f, avec_alpha(bord, 0.85f));
    ellipse(app, c, (sfVector2f){rr.x * 0.72f, rr.y * 0.72f}, 1.f,
        avec_alpha(bord, 0.3f));
    anneau_tirets(app, c, (sfVector2f){rr.x * 0.86f, rr.y * 0.86f},
        app->temps * 0.12f, avec_alpha(bord, 0.35f));
}

/** Cône de lumière venu du haut, posé sur une plateforme. */
static void faisceau(app_t *app, sfVector2f c, float largeur, sfColor col)
{
    sfVertex v[4] = {
        {{c.x - largeur * 0.35f, 0.f}, avec_alpha(col, 0.f), {0, 0}},
        {{c.x + largeur * 0.35f, 0.f}, avec_alpha(col, 0.f), {0, 0}},
        {{c.x + largeur, c.y}, avec_alpha(col, 0.09f * app->alpha), {0, 0}},
        {{c.x - largeur, c.y}, avec_alpha(col, 0.09f * app->alpha), {0, 0}},
    };

    sfRenderWindow_drawPrimitives(app->fenetre, v, 4, sfQuads, &app->additif);
}

/** Le fil rouge qui relie les deux camps, parcouru d'impulsions. */
static void fil_des_camps(app_t *app, float e)
{
    sfVector2f p[40];
    sfVector2f a = {SOL_J_X + PLAT_J_RX * 0.55f, SOL_J_Y - PLAT_J_RY * 0.3f};
    sfVector2f b = {SOL_A_X - PLAT_A_RX * 0.6f, SOL_A_Y + PLAT_A_RY * 0.4f};
    sfVector2f m = {(a.x + b.x) / 2.f, (a.y + b.y) / 2.f - 40.f};
    float u = 0.f;
    int k = 0;

    for (int i = 0; i < 40; i++) {
        u = i / 39.f * e;
        p[i] = (sfVector2f){(1 - u) * (1 - u) * a.x + 2 * (1 - u) * u * m.x + u * u * b.x,
            (1 - u) * (1 - u) * a.y + 2 * (1 - u) * u * m.y + u * u * b.y
            + sinf(u * 9.f + app->temps * 2.f) * 4.f};
    }
    ligne_epaisse(app, p, 40, 9.f, avec_alpha(C_ROUGE, 0.08f), 1);
    ligne_epaisse(app, p, 40, 1.6f, avec_alpha(C_ROUGE, 0.55f), 1);
    for (int i = 0; i < 2; i++) {
        k = (int)(fmodf(app->temps * 0.45f + i * 0.5f, 1.f) * 39.f);
        lueur(app, p[k], (sfVector2f){22.f, 22.f}, avec_alpha(C_ROUGE, 0.7f * e));
    }
}

/** Gradins lointains : anneau du stade et piliers de lumière. */
static void stade(app_t *app)
{
    sfVector2f c = {LARGEUR * 0.5f, HAUTEUR * 0.33f};
    sfVertex pilier[4];
    float x = 0.f;
    float y = 0.f;
    float a = 0.f;

    ellipse(app, c, (sfVector2f){LARGEUR * 0.62f, HAUTEUR * 0.2f}, 1.5f,
        avec_alpha(C_BLANC, 0.07f));
    ellipse(app, c, (sfVector2f){LARGEUR * 0.56f, HAUTEUR * 0.16f}, 1.f,
        avec_alpha(C_ROUGE, 0.12f));
    for (int k = 0; k < 9; k++) {
        a = PI + (k + 0.5f) * PI / 9.f;
        x = c.x + cosf(a) * LARGEUR * 0.62f;
        y = c.y + sinf(a) * HAUTEUR * 0.2f;
        pilier[0] = (sfVertex){{x - 6.f, y - 120.f}, avec_alpha(C_CYAN, 0.f), {0, 0}};
        pilier[1] = (sfVertex){{x + 6.f, y - 120.f}, avec_alpha(C_CYAN, 0.f), {0, 0}};
        pilier[2] = (sfVertex){{x + 6.f, y}, avec_alpha(C_CYAN, (0.05f + 0.04f
            * sinf(app->temps * 1.3f + k)) * app->alpha), {0, 0}};
        pilier[3] = (sfVertex){{x - 6.f, y}, pilier[2].color, {0, 0}};
        sfRenderWindow_drawPrimitives(app->fenetre, pilier, 4, sfQuads, &app->additif);
    }
}

static void dessiner_arene(app_t *app, arene_t *ar)
{
    float intro = ar->etat == VUE_INTRO ? ar->t_etat : DUREE_INTRO;
    float e = sortie_cubique(progression(intro, 0.f, 0.7f));
    float a = app->alpha;
    sfVertex voile[4] = {
        {{0.f, 0.f}, (sfColor){4, 5, 12, 170}, {0, 0}},
        {{LARGEUR, 0.f}, (sfColor){4, 5, 12, 170}, {0, 0}},
        {{LARGEUR, HAUTEUR}, (sfColor){4, 5, 12, 60}, {0, 0}},
        {{0.f, HAUTEUR}, (sfColor){4, 5, 12, 60}, {0, 0}},
    };

    sfRenderWindow_drawPrimitives(app->fenetre, voile, 4, sfQuads, NULL);
    app->alpha = a * e;
    stade(app);
    faisceau(app, (sfVector2f){SOL_A_X, SOL_A_Y}, PLAT_A_RX, C_BLANC);
    faisceau(app, (sfVector2f){SOL_J_X, SOL_J_Y}, PLAT_J_RX, C_CYAN);
    plateforme(app, (sfVector2f){SOL_A_X, SOL_A_Y}, (sfVector2f){PLAT_A_RX, PLAT_A_RY},
        C_ROUGE, 0.6f + 0.4f * e);
    plateforme(app, (sfVector2f){SOL_J_X, SOL_J_Y}, (sfVector2f){PLAT_J_RX, PLAT_J_RY},
        C_CYAN, 0.6f + 0.4f * e);
    fil_des_camps(app, sortie_cubique(progression(intro, 0.4f, 0.6f)));
    app->alpha = a;
}

/* ------------------------------------------------------------------ */
/*  Combattants                                                        */
/* ------------------------------------------------------------------ */

static sfColor couleur_pv(float ratio)
{
    if (ratio > 0.5f)
        return melanger(C_OR, C_VERT, (ratio - 0.5f) * 2.f);
    return melanger(C_ROUGE, C_OR, ratio * 2.f);
}

/** Barre de PV animée : piste, trace des PV perdus, PV actuels. */
static void barre_pv(app_t *app, sfFloatRect r, const visuel_t *v, int pv_max)
{
    float f = pv_max > 0 ? bornerf(v->pv_barre / pv_max, 0.f, 1.f) : 0.f;
    float g = pv_max > 0 ? bornerf(v->pv_fantome / pv_max, 0.f, 1.f) : 0.f;

    panneau(app, r, r.height * 0.5f, avec_alpha(C_BLANC, 0.08f),
        avec_alpha(C_BLANC, 0.05f), C_TRANSPARENT);
    if (g > f)
        panneau(app, (sfFloatRect){r.left, r.top, r.width * g, r.height}, 0.f,
            avec_alpha(C_BLANC, 0.6f), avec_alpha(C_ROUGE, 0.6f), C_TRANSPARENT);
    if (f > 0.f)
        panneau(app, (sfFloatRect){r.left, r.top, r.width * f, r.height},
            r.height * 0.5f, melanger(couleur_pv(f), C_BLANC, 0.25f), couleur_pv(f),
            C_TRANSPARENT);
}

/** Ombre portée et colonne de lumière sous le personnage. */
static void socle(app_t *app, const visuel_t *v, sfVector2f sol, sfColor col,
    float actif)
{
    sfVertex colonne[4] = {
        {{sol.x - v->rayon * 0.7f, sol.y}, avec_alpha(col, 0.16f * app->alpha), {0, 0}},
        {{sol.x + v->rayon * 0.7f, sol.y}, avec_alpha(col, 0.16f * app->alpha), {0, 0}},
        {{v->pos.x + v->rayon * 0.4f, v->pos.y}, avec_alpha(col, 0.f), {0, 0}},
        {{v->pos.x - v->rayon * 0.4f, v->pos.y}, avec_alpha(col, 0.f), {0, 0}},
    };

    disque(app, sol, (sfVector2f){v->rayon * 1.05f, v->rayon * 0.3f},
        (sfColor){2, 3, 8, 170}, (sfColor){2, 3, 8, 0});
    sfRenderWindow_drawPrimitives(app->fenetre, colonne, 4, sfQuads, &app->additif);
    ellipse(app, sol, (sfVector2f){v->rayon * (1.f + 0.15f * actif), v->rayon * 0.28f
        * (1.f + 0.15f * actif)}, 1.5f + actif, avec_alpha(col, 0.4f + 0.5f * actif));
}

/** Bouclier hexagonal de la garde : apparaît, tourne, s'illumine sous les coups. */
static void bouclier(app_t *app, sfVector2f p, float r, const visuel_t *v)
{
    float g = sortie_rebond(v->garde_anim);

    if (v->garde_anim < 0.01f)
        return;
    lueur(app, p, (sfVector2f){r * 3.2f, r * 3.2f}, avec_alpha(C_BLEU, 0.18f * g
        + 0.4f * v->garde_choc));
    hexagone(app, p, r * 1.5f * g, app->temps * 0.4f, avec_alpha(C_BLEU,
        0.10f + 0.25f * v->garde_choc), avec_alpha(melanger(C_BLEU, C_BLANC,
        v->garde_choc), 0.85f * v->garde_anim));
    hexagone(app, p, r * 1.32f * g, -app->temps * 0.6f, C_TRANSPARENT,
        avec_alpha(C_BLEU, 0.35f * v->garde_anim));
}

static void dessiner_combattant(app_t *app, arene_t *ar, ref_t r, int actif)
{
    const combattant_t *c = bataille_lire(&ar->bataille, r);
    visuel_t *v = vis(ar, r);
    float mort = v->ko_vis ? sortie_cubique(progression(v->t_ko, 0.f, 0.6f)) : 0.f;
    float souffle = actif ? 1.f + 0.04f * sinf(app->temps * 3.2f) : 1.f;
    sfColor col = griser(couleur_classe(c->perso.classe), mort);
    sfVector2f p = {v->pos.x + v->elan.x + sinf(app->temps * 70.f) * 7.f * v->secousse,
        v->pos.y + v->elan.y + v->rayon * 0.5f * mort};
    float rr = v->rayon * souffle * (1.f - 0.2f * mort);
    float a = app->alpha;

    app->alpha = a * (1.f - 0.55f * mort);
    socle(app, v, (sfVector2f){v->sol.x + v->elan.x * 0.6f, v->sol.y}, col,
        actif ? 0.5f + 0.5f * sinf(app->temps * 4.f) : 0.f);
    lueur(app, p, (sfVector2f){rr * 3.f, rr * 3.f}, avec_alpha(col, 0.2f
        + 0.15f * actif + 0.5f * v->lueur_soin));
    if (v->lueur_soin > 0.01f)
        lueur(app, p, (sfVector2f){rr * 3.6f, rr * 3.6f}, avec_alpha(C_SOIN,
            0.45f * v->lueur_soin));
    hexagone(app, p, rr, 0.f, melanger((sfColor){14, 16, 32, 245}, col, 0.2f), col);
    hexagone(app, p, rr * 0.8f, PI / 6.f + app->temps * 0.2f * (1.f - mort),
        C_TRANSPARENT, avec_alpha(col, 0.35f));
    embleme(app, c->perso.classe, p, rr * 0.52f, melanger(col, C_BLANC, 0.5f));
    if (v->flash > 0.01f)
        hexagone(app, p, rr, 0.f, avec_alpha(C_BLANC, 0.75f * v->flash), C_TRANSPARENT);
    bouclier(app, p, rr, v);
    app->alpha = a;
}

/** Nom et barre de PV flottant au-dessus du personnage. */
static void plaque(app_t *app, arene_t *ar, ref_t r, int actif)
{
    const combattant_t *c = bataille_lire(&ar->bataille, r);
    visuel_t *v = vis(ar, r);
    float w = fmaxf(84.f, v->rayon * 2.4f);
    float y = v->pos.y - v->rayon * 1.35f - (r.equipe == 0 ? 22.f : 16.f);
    unsigned int taille = r.equipe == 0 ? 15 : 12;
    float a = app->alpha;
    char nom[48];

    app->alpha = a * (v->ko_vis ? 0.6f : 1.f);
    tronquer(app, POLICE_GRAS, taille, c->perso.nom, w + 30.f, nom);
    app->contour = 2.f;
    texte(app, POLICE_GRAS, nom, taille, (sfVector2f){v->pos.x, y - (r.equipe == 0
        ? 14.f : 11.f)}, v->ko_vis ? C_TEXTE_FAIBLE : C_TEXTE, ALIGNE_CENTRE);
    app->contour = 0.f;
    if (v->ko_vis)
        pastille(app, "K.O.", (sfVector2f){v->pos.x, y + 2.f}, C_ROUGE, 1);
    else
        barre_pv(app, (sfFloatRect){v->pos.x - w / 2.f, y, w, r.equipe == 0 ? 6.f
            : 4.f}, v, c->pv_max);
    if (actif) {
        lueur(app, (sfVector2f){v->pos.x, y - 40.f}, (sfVector2f){40.f, 40.f},
            avec_alpha(couleur_equipe(r.equipe), 0.6f));
        losange(app, (sfVector2f){v->pos.x, y - 38.f + sinf(app->temps * 5.f) * 4.f},
            7.f, couleur_equipe(r.equipe));
    }
    app->alpha = a;
}

/** Réticule autour des cibles valides ; la cible choisie affiche l'effet prévu. */
static void reticules(app_t *app, arene_t *ar)
{
    const combattant_t *acteur = bataille_lire(&ar->bataille,
        bataille_actif(&ar->bataille));
    sfColor col = couleur_action(ar->action);
    const combattant_t *c = NULL;
    visuel_t *v = NULL;
    float r = 0.f;
    int choisie = 0;
    char buf[24];

    for (int i = 0; i < ar->nb_cibles; i++) {
        c = bataille_lire(&ar->bataille, ar->cibles[i]);
        v = vis(ar, ar->cibles[i]);
        choisie = i == ar->cible;
        r = v->rayon * (1.3f + (choisie ? 0.08f * sinf(app->temps * 6.f) : 0.f));
        crochets(app, (sfFloatRect){v->pos.x - r, v->pos.y - r, 2.f * r, 2.f * r},
            r * 0.45f, avec_alpha(col, choisie ? 1.f : 0.3f + 0.15f
            * sinf(app->temps * 4.f + i)));
        if (!choisie)
            continue;
        crochets(app, (sfFloatRect){v->pos.x + r, v->pos.y + r, -2.f * r, -2.f * r},
            -r * 0.45f, col);
        snprintf(buf, sizeof(buf), ar->action == ACTION_ATTAQUER ? "-%d" : "+%d",
            ar->action == ACTION_ATTAQUER ? bataille_degats(acteur, c)
            : bataille_soin(acteur, c));
        app->contour = 2.f;
        texte(app, POLICE_GRAS, buf, 22, (sfVector2f){v->pos.x + r + 12.f, v->pos.y},
            melanger(col, C_BLANC, 0.2f), ALIGNE_GAUCHE);
        app->contour = 0.f;
    }
}

/* ------------------------------------------------------------------ */
/*  Effets des actions                                                 */
/* ------------------------------------------------------------------ */

typedef void (*effet_impact_t)(app_t *app, sfVector2f c, sfColor col, float t,
    float s);

static void impact_corps_a_corps(app_t *app, sfVector2f c, sfColor col, float t,
    float s)
{
    float p = sortie_cubique(progression(t, 0.f, 0.12f)) * s;
    float f = 1.f - progression(t, 0.1f, 0.3f);
    sfVector2f l1[2] = {{c.x - 90.f * p, c.y - 90.f * p}, {c.x + 90.f * p, c.y + 90.f * p}};
    sfVector2f l2[2] = {{c.x + 80.f * p, c.y - 80.f * p}, {c.x - 80.f * p, c.y + 80.f * p}};

    ligne_epaisse(app, l1, 2, 22.f * s, avec_alpha(col, 0.35f * f), 1);
    ligne_epaisse(app, l1, 2, 4.f, avec_alpha(C_BLANC, f), 1);
    ligne_epaisse(app, l2, 2, 18.f * s, avec_alpha(col, 0.3f * f), 1);
    ligne_epaisse(app, l2, 2, 3.f, avec_alpha(C_BLANC, f), 1);
}

static void impact_magie(app_t *app, sfVector2f c, sfColor col, float t, float s)
{
    float p = sortie_cubique(progression(t, 0.f, 0.12f));
    float f = 1.f - progression(t, 0.1f, 0.3f);

    cercle(app, c, (24.f + 110.f * sortie_cubique(progression(t, 0.f, 0.4f))) * s,
        C_TRANSPARENT, avec_alpha(col, f), 5.f * f + 1.f);
    lueur(app, c, (sfVector2f){170.f * p * s, 170.f * p * s}, avec_alpha(col, 0.5f * f));
}

static void impact_distance(app_t *app, sfVector2f c, sfColor col, float t, float s)
{
    float f = 1.f - progression(t, 0.f, 0.3f);

    for (int k = 0; k < 6; k++)
        arc_cercle(app, c, (18.f + 70.f * sortie_cubique(progression(t, 0.f, 0.3f)))
            * s, 3.f * f + 1.f, (sfVector2f){k * PI / 3.f, k * PI / 3.f + 0.5f},
            avec_alpha(col, f), 1);
    lueur(app, c, (sfVector2f){90.f * s, 90.f * s}, avec_alpha(C_BLANC, 0.6f * f));
}

static void impact_soutien(app_t *app, sfVector2f c, sfColor col, float t, float s)
{
    float p = sortie_cubique(progression(t, 0.f, 0.18f)) * s;
    float f = 1.f - progression(t, 0.1f, 0.35f);
    sfVector2f v[2] = {{c.x, c.y - 100.f * p}, {c.x, c.y + 100.f * p}};
    sfVector2f h[2] = {{c.x - 100.f * p, c.y}, {c.x + 100.f * p, c.y}};

    lueur(app, c, (sfVector2f){200.f * p, 200.f * p}, avec_alpha(col, 0.45f * f));
    ligne_epaisse(app, v, 2, 14.f * s, avec_alpha(col, 0.4f * f), 1);
    ligne_epaisse(app, h, 2, 14.f * s, avec_alpha(col, 0.4f * f), 1);
    ligne_epaisse(app, v, 2, 3.f, avec_alpha(C_BLANC, f), 1);
    ligne_epaisse(app, h, 2, 3.f, avec_alpha(C_BLANC, f), 1);
}

static const effet_impact_t EFFETS_IMPACT[NB_CATEGORIES] = {
    [CAT_CORPS_A_CORPS] = impact_corps_a_corps,
    [CAT_MAGIE] = impact_magie,
    [CAT_DISTANCE] = impact_distance,
    [CAT_SOUTIEN] = impact_soutien,
};

/** Point d'une courbe de Bézier quadratique (rayon de soin). */
static sfVector2f bezier(sfVector2f a, sfVector2f m, sfVector2f b, float u)
{
    return (sfVector2f){(1 - u) * (1 - u) * a.x + 2 * (1 - u) * u * m.x + u * u * b.x,
        (1 - u) * (1 - u) * a.y + 2 * (1 - u) * u * m.y + u * u * b.y};
}

static void rayon_soin(app_t *app, sfVector2f a, sfVector2f b, float t)
{
    sfVector2f m = {(a.x + b.x) / 2.f, fminf(a.y, b.y) - 90.f};
    sfVector2f p[32];
    float debut = progression(t, 0.3f, 0.25f);
    float fin = progression(t, 0.1f, 0.3f);
    int n = 0;

    if (fin <= debut)
        return;
    for (int i = 0; i < 32; i++)
        p[n++] = bezier(a, m, b, lerpf(debut, fin, i / 31.f));
    ligne_epaisse(app, p, n, 12.f, avec_alpha(C_SOIN, 0.18f), 1);
    ligne_epaisse(app, p, n, 2.5f, avec_alpha(C_SOIN, 0.9f), 1);
    for (int k = 0; k < 4; k++)
        lueur(app, bezier(a, m, b, lerpf(debut, fin, fmodf(t * 3.f + k * 0.25f, 1.f))),
            (sfVector2f){18.f, 18.f}, avec_alpha(C_SOIN, 0.8f));
}

/** Effets dessinés pendant l'événement en cours (traînée, impact, rayon). */
static void dessiner_effet(app_t *app, arene_t *ar)
{
    const evenement_t *ev = NULL;
    visuel_t *a = NULL;
    visuel_t *c = NULL;
    sfVector2f trainee[2];
    classe_t cl = GUERRIER;

    if (ar->etat != VUE_ANIMATION || ar->i_file >= ar->nb_file)
        return;
    ev = &ar->file[ar->i_file];
    a = vis(ar, ev->acteur);
    c = vis(ar, ev->cible);
    cl = bataille_lire(&ar->bataille, ev->acteur)->perso.classe;
    if (ev->type == EV_ATTAQUE && ar->t_evt > 0.12f && ar->t_evt < 0.4f) {
        trainee[0] = a->pos;
        trainee[1] = (sfVector2f){a->pos.x + a->elan.x, a->pos.y + a->elan.y};
        ligne_epaisse(app, trainee, 2, a->rayon * 0.45f, avec_alpha(couleur_classe(cl),
            0.16f), 1);
        ligne_epaisse(app, trainee, 2, 3.f, avec_alpha(C_BLANC, 0.35f), 1);
    }
    if (ev->type == EV_ATTAQUE && ar->t_evt >= IMPACTS[EV_ATTAQUE])
        EFFETS_IMPACT[classes[cl].categorie](app, c->pos, couleur_classe(cl),
            ar->t_evt - IMPACTS[EV_ATTAQUE], c->rayon / 56.f);
    if (ev->type == EV_SOIN)
        rayon_soin(app, a->pos, c->pos, ar->t_evt);
}

/* ------------------------------------------------------------------ */
/*  Lecture des événements                                             */
/* ------------------------------------------------------------------ */

static void debut_tour(arene_t *ar)
{
    ar->t_tour = 0.f;
    ar->t_etat = 0.f;
    ar->etat = bataille_actif(&ar->bataille).equipe == 0 ? VUE_CHOIX_ACTION
        : VUE_ORDINATEUR;
    ar->action = ACTION_ATTAQUER;
}

static void lancer(app_t *app, arene_t *ar, action_t a, ref_t cible)
{
    int n = bataille_agir(&ar->bataille, a, cible, ar->file);

    if (n < 0) {
        notifier(app, "✗", "Action impossible", C_ROUGE);
        return;
    }
    ar->nb_file = n;
    ar->i_file = 0;
    ar->t_evt = 0.f;
    ar->impact_fait = 0;
    ar->etat = VUE_ANIMATION;
}

/** Moment de l'impact : l'écran rattrape le résultat déjà calculé par le moteur. */
static void impact(app_t *app, arene_t *ar, const evenement_t *ev)
{
    visuel_t *c = vis(ar, ev->cible);
    char buf[24];

    ar->journal_vu++;
    if (ev->type == EV_ATTAQUE) {
        c->pv_vis = ev->pv_apres;
        c->flash = 1.f;
        c->secousse = 1.f;
        c->delai_fantome = 0.45f;
        c->garde_choc = ev->garde ? 1.f : 0.f;
        snprintf(buf, sizeof(buf), "-%d", ev->valeur);
        popup(ar, (sfVector2f){c->pos.x + c->rayon * 0.9f, c->pos.y - c->rayon * 0.2f}, buf,
            ev->garde ? C_BLEU : C_ROUGE);
        emettre(app, c->pos, 22, couleur_classe(bataille_lire(&ar->bataille,
            ev->acteur)->perso.classe), 380.f, 3.f);
        app->secousse = fmaxf(app->secousse, ev->cible.equipe == 0 ? 0.45f : 0.25f);
    } else if (ev->type == EV_SOIN) {
        c->pv_vis = ev->pv_apres;
        c->lueur_soin = 1.f;
        snprintf(buf, sizeof(buf), "+%d", ev->valeur);
        popup(ar, (sfVector2f){c->pos.x + c->rayon * 0.9f, c->pos.y - c->rayon * 0.2f}, buf, C_VERT);
        emettre(app, c->pos, 18, C_SOIN, 220.f, 2.6f);
    } else if (ev->type == EV_GARDE) {
        c->garde_vis = 1;
        popup(ar, (sfVector2f){c->pos.x + c->rayon * 0.9f, c->pos.y - c->rayon * 0.2f}, "GARDE", C_BLEU);
    } else if (ev->type == EV_GARDE_FIN) {
        c->garde_vis = 0;
    } else if (ev->type == EV_KO) {
        c->ko_vis = 1;
        c->t_ko = 0.f;
        emettre(app, c->pos, 40, C_TEXTE_FAIBLE, 300.f, 3.f);
        app->hitstop = 0.08f;
    }
}

/** Déplacement de l'attaquant : recul, élan vers la cible, retour. */
static void elan_attaque(arene_t *ar, const evenement_t *ev, float t)
{
    visuel_t *a = vis(ar, ev->acteur);
    visuel_t *c = vis(ar, ev->cible);
    sfVector2f d = {c->pos.x - a->pos.x, c->pos.y - a->pos.y};
    float l = sqrtf(d.x * d.x + d.y * d.y);
    float k = 0.f;

    if (l < 1.f)
        return;
    d = (sfVector2f){d.x / l, d.y / l};
    if (t < 0.12f)
        k = -14.f * sortie_cubique(t / 0.12f);
    else if (t < IMPACTS[EV_ATTAQUE])
        k = lerpf(-14.f, l * 0.6f, entree_cubique((t - 0.12f) / 0.18f));
    else
        k = l * 0.6f * (1.f - sortie_cubique(progression(t, IMPACTS[EV_ATTAQUE], 0.32f)));
    a->elan = (sfVector2f){d.x * k, d.y * k};
}

static void jouer_evenements(app_t *app, arene_t *ar)
{
    const evenement_t *ev = NULL;

    if (ar->etat != VUE_ANIMATION)
        return;
    while (ar->i_file < ar->nb_file) {
        ev = &ar->file[ar->i_file];
        ar->t_evt += app->dt_jeu;
        if (ev->type == EV_ATTAQUE)
            elan_attaque(ar, ev, ar->t_evt);
        if (ev->type == EV_SOIN && ar->t_evt < 0.3f)
            vis(ar, ev->acteur)->lueur_soin = 1.f;
        if (!ar->impact_fait && ar->t_evt >= IMPACTS[ev->type]) {
            ar->impact_fait = 1;
            impact(app, ar, ev);
        }
        if (ar->t_evt < DUREES[ev->type])
            return;
        vis(ar, ev->acteur)->elan = (sfVector2f){0.f, 0.f};
        ar->i_file++;
        ar->t_evt = 0.f;
        ar->impact_fait = 0;
    }
    if (ar->bataille.gagnant != -1) {
        ar->etat = VUE_FIN;
        ar->t_etat = 0.f;
    } else {
        debut_tour(ar);
    }
}

/** Animations continues : barres, bouclier, éclats, K.O. (pilotées par le temps). */
static void maj_visuels(app_t *app, arene_t *ar)
{
    visuel_t *v = NULL;
    float dt = app->dt;

    for (int e = 0; e < NB_EQUIPES; e++) {
        for (int i = 0; i < ar->bataille.equipes[e].taille; i++) {
            v = &ar->vis[e][i];
            v->pv_barre = approcher(v->pv_barre, (float)v->pv_vis, 9.f, dt);
            v->delai_fantome -= dt;
            if (v->pv_fantome < v->pv_barre || v->delai_fantome <= 0.f)
                v->pv_fantome = approcher(v->pv_fantome, v->pv_barre,
                    v->pv_fantome < v->pv_barre ? 30.f : 4.f, dt);
            v->garde_anim = approcher(v->garde_anim, (float)v->garde_vis, 10.f, dt);
            v->garde_choc = approcher(v->garde_choc, 0.f, 5.f, dt);
            v->flash = approcher(v->flash, 0.f, 9.f, dt);
            v->secousse = approcher(v->secousse, 0.f, 7.f, dt);
            v->lueur_soin = approcher(v->lueur_soin, 0.f, 2.5f, dt);
            if (v->ko_vis)
                v->t_ko += dt;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Commandes                                                          */
/* ------------------------------------------------------------------ */

static void choisir_cibles(app_t *app, arene_t *ar)
{
    ar->nb_cibles = bataille_cibles(&ar->bataille, (action_t)ar->action, ar->cibles);
    ar->cible = 0;
    if (ar->action == ACTION_DEFENDRE) {
        lancer(app, ar, ACTION_DEFENDRE, ar->cibles[0]);
        return;
    }
    if (ar->nb_cibles == 0) {
        notifier(app, "✗", "Aucune cible valide", C_ROUGE);
        return;
    }
    ar->etat = VUE_CHOIX_CIBLE;
}

static void valider_action(app_t *app, arene_t *ar, int a)
{
    ar->action = a;
    if (!bataille_action_possible(&ar->bataille, (action_t)a)) {
        notifier(app, "✗", a == ACTION_SOIGNER ? "Aucun allié ne peut être soigné"
            : "Action impossible", C_ROUGE);
        return;
    }
    choisir_cibles(app, ar);
}

static void clavier(app_t *app, arene_t *ar)
{
    const int *k = app->in.touches;
    const int touches[NB_ACTIONS] = {sfKeyNum1, sfKeyNum2, sfKeyNum3};
    int sens = k[sfKeyRight] || k[sfKeyDown] ? 1 : (k[sfKeyLeft] || k[sfKeyUp] ? -1 : 0);

    if (ar->etat == VUE_CHOIX_ACTION) {
        if (k[sfKeyEscape])
            changer_scene(app, SCENE_MENU);
        ar->action = (ar->action + sens + NB_ACTIONS) % NB_ACTIONS;
        for (int i = 0; i < NB_ACTIONS; i++)
            if (k[touches[i]])
                valider_action(app, ar, i);
        if (k[sfKeyEnter] || k[sfKeySpace])
            valider_action(app, ar, ar->action);
    } else if (ar->etat == VUE_CHOIX_CIBLE) {
        ar->cible = (ar->cible + sens + ar->nb_cibles) % ar->nb_cibles;
        if (k[sfKeyEscape])
            ar->etat = VUE_CHOIX_ACTION;
        else if (k[sfKeyEnter] || k[sfKeySpace])
            lancer(app, ar, (action_t)ar->action, ar->cibles[ar->cible]);
    } else if (ar->etat == VUE_FIN) {
        if (k[sfKeyEnter] || k[sfKeyR])
            demarrer(app, ar);
        else if (k[sfKeyEscape])
            changer_scene(app, SCENE_MENU);
    }
}

/** Souris : survoler une cible la choisit, cliquer confirme. */
static void souris_cibles(app_t *app, arene_t *ar)
{
    visuel_t *v = NULL;
    float dx = 0.f;
    float dy = 0.f;

    if (ar->etat != VUE_CHOIX_CIBLE)
        return;
    for (int i = 0; i < ar->nb_cibles; i++) {
        v = vis(ar, ar->cibles[i]);
        dx = app->in.souris.x - v->pos.x;
        dy = app->in.souris.y - v->pos.y;
        if (dx * dx + dy * dy > v->rayon * v->rayon * 1.6f || app->souris_bloquee)
            continue;
        app->curseur_voulu = CURSEUR_MAIN;
        ar->cible = i;
        if (app->in.clic)
            lancer(app, ar, (action_t)ar->action, ar->cibles[i]);
        return;
    }
}

/**
 * Combattant présenté à l'écran : pendant une animation, celui qui agit
 * (le moteur, lui, est déjà passé au tour suivant).
 */
static ref_t acteur_affiche(const arene_t *ar)
{
    if (ar->etat == VUE_ANIMATION && ar->i_file < ar->nb_file)
        return ar->file[ar->i_file].acteur;
    return bataille_actif(&ar->bataille);
}

/** Bouton d'action : touche, nom, et la statistique qui le gouverne. */
static int bouton_action(app_t *app, arene_t *ar, int a, sfFloatRect r, int dispo)
{
    const combattant_t *c = bataille_lire(&ar->bataille, acteur_affiche(ar));
    int choix = ar->action == a && (ar->etat == VUE_CHOIX_ACTION
        || ar->etat == VUE_CHOIX_CIBLE);
    int ouvert = ar->etat == VUE_CHOIX_ACTION;
    int s = survole(app, r) && ouvert;
    sfColor col = couleur_action(a);
    float h = 0.f;
    float al = app->alpha;
    char info[32];

    ar->boutons[a].survol = approcher(ar->boutons[a].survol, s || choix ? 1.f : 0.f,
        14.f, app->dt);
    h = ar->boutons[a].survol;
    if (s)
        app->curseur_voulu = CURSEUR_MAIN;
    if (!dispo || !ouvert)
        app->alpha *= ouvert ? 0.45f : (choix ? 0.9f : 0.4f);
    panneau(app, (sfFloatRect){r.left, r.top - 3.f * h, r.width, r.height}, 10.f,
        melanger(C_SURFACE_HAUTE, col, 0.1f + 0.25f * h), C_SURFACE,
        melanger(C_BORD, col, 0.3f + 0.7f * h));
    if (h > 0.01f)
        rect_arrondi(app, (sfFloatRect){r.left + 10.f, r.top + r.height - 3.f - 3.f * h,
            (r.width - 20.f) * h, 3.f}, 0.f, col);
    touche_clavier(app, TOUCHES_ACTIONS[a], (sfVector2f){r.left + 22.f, r.top + 20.f
        - 3.f * h}, col, h);
    texte(app, POLICE_GRAS, NOMS_ACTIONS[a], 16, (sfVector2f){r.left + 42.f, r.top
        + 20.f - 3.f * h}, melanger(C_TEXTE_DOUX, C_TEXTE, h), ALIGNE_GAUCHE);
    if (a == ACTION_ATTAQUER)
        snprintf(info, sizeof(info), "Attaque %d", c->perso.atq);
    else if (a == ACTION_SOIGNER)
        snprintf(info, sizeof(info), dispo ? "Soin %d" : (c->perso.soin > 0
            ? "Personne à soigner" : "Soin nul"), c->perso.soin);
    else
        snprintf(info, sizeof(info), "-%d %% de dégâts", GARDE_REDUCTION);
    texte(app, POLICE_NORMALE, info, 12, (sfVector2f){r.left + 14.f, r.top + 44.f
        - 3.f * h}, C_TEXTE_FAIBLE, ALIGNE_GAUCHE);
    app->alpha = al;
    return s && app->in.clic && dispo;
}

static void panneau_commandes(app_t *app, arene_t *ar, float e)
{
    sfFloatRect r = {LARGEUR - MARGE - CMD_L + 60.f * (1.f - e), HAUTEUR - MARGE
        - CMD_H + 12.f, CMD_L, CMD_H};
    const combattant_t *c = bataille_lire(&ar->bataille, acteur_affiche(ar));
    float bw = (CMD_L - 40.f - 2 * 10.f) / NB_ACTIONS;
    float x = 0.f;
    char buf[64];

    ombre_biseau(app, r, 18.f, 1.2f);
    panneau(app, r, 18.f, (sfColor){24, 29, 52, 238}, (sfColor){12, 14, 28, 245},
        C_BORD);
    crochets(app, (sfFloatRect){r.left - 5.f, r.top - 5.f, r.width + 10.f,
        r.height + 10.f}, 16.f, avec_alpha(couleur_equipe(acteur_affiche(ar).equipe),
        0.6f));
    x = r.left + 20.f + etiquette(app, "TOUR DE", (sfVector2f){r.left + 20.f, r.top
        + 24.f}, C_TEXTE_FAIBLE, ALIGNE_GAUCHE) + 10.f;
    x += texte(app, POLICE_GRAS, c->perso.nom, 17, (sfVector2f){x, r.top + 23.f},
        couleur_classe(c->perso.classe), ALIGNE_GAUCHE) + 10.f;
    texte(app, POLICE_NORMALE, nom_classe(c->perso.classe), 13, (sfVector2f){x, r.top
        + 24.f}, C_TEXTE_FAIBLE, ALIGNE_GAUCHE);
    if (ar->etat == VUE_ORDINATEUR) {
        snprintf(buf, sizeof(buf), "L'adversaire réfléchit%.*s",
            1 + (int)fmodf(app->temps * 3.f, 3.f), "...");
        texte(app, POLICE_NORMALE, buf, 15, (sfVector2f){r.left + 20.f, r.top + 76.f},
            C_TEXTE_DOUX, ALIGNE_GAUCHE);
        return;
    }
    for (int a = 0; a < NB_ACTIONS; a++)
        if (bouton_action(app, ar, a, (sfFloatRect){r.left + 20.f + a * (bw + 10.f),
            r.top + 44.f, bw, 58.f}, bataille_action_possible(&ar->bataille,
            (action_t)a)))
            valider_action(app, ar, a);
    if (ar->etat == VUE_CHOIX_CIBLE)
        texte(app, POLICE_NORMALE, "Choisis la cible  ·  ‹ ›  ·  Entrée  ·  Échap retour",
            12, (sfVector2f){r.left + r.width / 2.f, r.top + r.height - 12.f},
            melanger(couleur_action(ar->action), C_TEXTE, 0.4f), ALIGNE_CENTRE);
}

/* ------------------------------------------------------------------ */
/*  Informations                                                       */
/* ------------------------------------------------------------------ */

/** Une ligne d'équipe : nom, classe, PV ; l'actif est souligné. */
static void ligne_equipe(app_t *app, arene_t *ar, ref_t r, sfFloatRect zone)
{
    const combattant_t *c = bataille_lire(&ar->bataille, r);
    visuel_t *v = vis(ar, r);
    int actif = meme(r, bataille_actif(&ar->bataille)) && ar->etat != VUE_ANIMATION
        && ar->etat != VUE_FIN;
    float cy = zone.top + zone.height / 2.f;
    float bw = r.equipe == 0 ? 170.f : 120.f;
    float a = app->alpha;
    char buf[32];

    app->alpha = a * (v->ko_vis ? 0.45f : 1.f);
    if (actif)
        panneau(app, zone, 6.f, avec_alpha(couleur_equipe(r.equipe), 0.16f),
            avec_alpha(couleur_equipe(r.equipe), 0.04f), C_TRANSPARENT);
    rect_arrondi(app, (sfFloatRect){zone.left, zone.top + 6.f, 3.f, zone.height
        - 12.f}, 0.f, couleur_classe(c->perso.classe));
    texte(app, POLICE_GRAS, c->perso.nom, r.equipe == 0 ? 15 : 13, (sfVector2f){
        zone.left + 12.f, cy - 7.f}, C_TEXTE, ALIGNE_GAUCHE);
    texte(app, POLICE_NORMALE, nom_classe(c->perso.classe), 11, (sfVector2f){
        zone.left + 12.f, cy + 9.f}, couleur_classe(c->perso.classe), ALIGNE_GAUCHE);
    if (v->garde_vis)
        texte(app, POLICE_SYMBOLES, "◆", 12, (sfVector2f){zone.left + zone.width - bw
            - 22.f, cy}, C_BLEU, ALIGNE_CENTRE);
    if (v->ko_vis) {
        pastille(app, "K.O.", (sfVector2f){zone.left + zone.width - 40.f, cy},
            C_ROUGE, 1);
    } else {
        barre_pv(app, (sfFloatRect){zone.left + zone.width - bw - 8.f, cy - 3.f, bw,
            r.equipe == 0 ? 7.f : 5.f}, v, c->pv_max);
        snprintf(buf, sizeof(buf), "%d/%d", (int)roundf(v->pv_barre), c->pv_max);
        texte(app, POLICE_GRAS, buf, 11, (sfVector2f){zone.left + zone.width - 8.f,
            cy - 13.f}, C_TEXTE_DOUX, ALIGNE_DROITE);
    }
    app->alpha = a;
}

static int nb_vivants_vus(arene_t *ar, int e)
{
    int n = 0;

    for (int i = 0; i < ar->bataille.equipes[e].taille; i++)
        n += !ar->vis[e][i].ko_vis;
    return n;
}

static void panneau_equipe(app_t *app, arene_t *ar, int e, sfFloatRect r)
{
    float hl = e == 0 ? LIGNE_J : LIGNE_A;
    char buf[48];

    ombre_biseau(app, r, 14.f, 1.f);
    panneau(app, r, 14.f, (sfColor){24, 29, 52, 225}, (sfColor){12, 14, 28, 235},
        C_BORD);
    rect_arrondi(app, (sfFloatRect){r.left + 14.f, r.top, 90.f, 2.f}, 0.f,
        couleur_equipe(e));
    etiquette(app, e == 0 ? "TON ÉQUIPE" : "ADVERSAIRES", (sfVector2f){r.left + 16.f,
        r.top + 20.f}, couleur_equipe(e), ALIGNE_GAUCHE);
    snprintf(buf, sizeof(buf), "%d / %d en lice", nb_vivants_vus(ar, e),
        ar->bataille.equipes[e].taille);
    texte(app, POLICE_NORMALE, buf, 12, (sfVector2f){r.left + r.width - 16.f,
        r.top + 20.f}, C_TEXTE_FAIBLE, ALIGNE_DROITE);
    for (int i = 0; i < ar->bataille.equipes[e].taille; i++)
        ligne_equipe(app, ar, (ref_t){e, i}, (sfFloatRect){r.left + 10.f, r.top + 36.f
            + i * hl, r.width - 20.f, hl - 4.f});
}

/** Les prochains tours, à partir de l'actif (personnages en lice seulement). */
static void ordre_tours(app_t *app, arene_t *ar)
{
    const bataille_t *b = &ar->bataille;
    ref_t r;
    const combattant_t *c = NULL;
    float x = LARGEUR / 2.f - 3 * 40.f;
    float a = app->alpha;
    float t = 0.f;
    int vus = 0;

    app->alpha = a * (ar->etat == VUE_ANIMATION ? 0.45f : 1.f);
    etiquette(app, "ORDRE DES TOURS", (sfVector2f){LARGEUR / 2.f, 22.f},
        C_TEXTE_FAIBLE, ALIGNE_CENTRE);
    for (int k = 0; k < b->nb_ordre * 2 && vus < 7; k++) {
        r = b->ordre[(b->tour + k) % b->nb_ordre];
        c = bataille_lire(b, r);
        if (!combattant_vivant(c))
            continue;
        t = vus == 0 ? 1.f : 0.f;
        hexagone(app, (sfVector2f){x, 52.f}, 13.f + 5.f * t, 0.f, melanger(
            (sfColor){14, 16, 32, 240}, couleur_classe(c->perso.classe), 0.25f),
            couleur_classe(c->perso.classe));
        embleme(app, c->perso.classe, (sfVector2f){x, 52.f}, 7.f + 3.f * t,
            melanger(couleur_classe(c->perso.classe), C_BLANC, 0.4f));
        rect_arrondi(app, (sfFloatRect){x - 9.f, 76.f + 2.f * t, 18.f, 2.f}, 0.f,
            couleur_equipe(r.equipe));
        x += 40.f + 8.f * t;
        vus++;
    }
    app->alpha = a;
}

/** Journal : seulement les lignes déjà jouées à l'écran, les plus récentes en clair. */
static void journal(app_t *app, arene_t *ar, float y)
{
    const bataille_t *b = &ar->bataille;
    int cache = b->nb_journal - ar->journal_vu;
    const char *ligne = NULL;
    float a = app->alpha;

    for (int k = 0; k < 4; k++) {
        ligne = bataille_journal(b, cache + k);
        if (ligne == NULL)
            break;
        app->alpha = a * (1.f - 0.22f * k);
        losange(app, (sfVector2f){MARGE + 4.f, y + k * 20.f}, 2.5f, k == 0 ? C_ROUGE
            : C_TEXTE_FAIBLE);
        texte(app, POLICE_NORMALE, ligne, 13, (sfVector2f){MARGE + 14.f, y + k * 20.f},
            k == 0 ? C_TEXTE : C_TEXTE_FAIBLE, ALIGNE_GAUCHE);
    }
    app->alpha = a;
}

/** Bandeau bref au début de chaque tour. */
static void banniere(app_t *app, arene_t *ar)
{
    const combattant_t *c = bataille_lire(&ar->bataille, bataille_actif(&ar->bataille));
    float t = ar->t_tour;
    float e = sortie_cubique(progression(t, 0.f, 0.25f));
    float f = 1.f - progression(t, DUREE_BANNIERE - 0.3f, 0.3f);
    float a = app->alpha;
    float cy = HAUTEUR * 0.47f;
    char buf[64];

    if (t > DUREE_BANNIERE || (ar->etat != VUE_CHOIX_ACTION && ar->etat != VUE_ORDINATEUR))
        return;
    app->alpha = a * fminf(e, f);
    panneau(app, (sfFloatRect){LARGEUR / 2.f - 260.f * e, cy - 26.f, 520.f * e, 52.f},
        0.f, (sfColor){8, 10, 22, 200}, (sfColor){8, 10, 22, 200}, C_TRANSPARENT);
    rect_arrondi(app, (sfFloatRect){LARGEUR / 2.f - 260.f * e, cy - 26.f, 520.f * e,
        2.f}, 0.f, couleur_classe(c->perso.classe));
    rect_arrondi(app, (sfFloatRect){LARGEUR / 2.f - 260.f * e, cy + 24.f, 520.f * e,
        2.f}, 0.f, couleur_equipe(bataille_actif(&ar->bataille).equipe));
    snprintf(buf, sizeof(buf), "TOUR DE %s", c->perso.nom);
    app->espacement = 3.f;
    texte(app, POLICE_GRAS, buf, 20, (sfVector2f){LARGEUR / 2.f + 30.f * (1.f - e), cy},
        C_TEXTE, ALIGNE_CENTRE);
    app->espacement = 0.f;
    app->alpha = a;
}

static void ecran_fin(app_t *app, arene_t *ar)
{
    int gagne = ar->bataille.gagnant == 0;
    sfColor col = gagne ? C_CYAN : C_ROUGE;
    float e = sortie_rebond(progression(ar->t_etat, 0.1f, 0.6f));
    float a = app->alpha;
    float cy = HAUTEUR * 0.42f;

    app->alpha = a * progression(ar->t_etat, 0.f, 0.4f);
    panneau(app, (sfFloatRect){-40.f, -40.f, LARGEUR + 80.f, HAUTEUR + 80.f}, 0.f,
        (sfColor){4, 5, 12, 150}, (sfColor){4, 5, 12, 190}, C_TRANSPARENT);
    lueur(app, (sfVector2f){LARGEUR / 2.f, cy}, (sfVector2f){560.f, 180.f},
        avec_alpha(col, 0.3f));
    app->espacement = 2.4f;
    app->contour = 3.f;
    texte_echelle(app, POLICE_GRAS, gagne ? "VICTOIRE" : "DÉFAITE", 76,
        (sfVector2f){LARGEUR / 2.f, cy}, col, ALIGNE_CENTRE, 0.7f + 0.3f * e);
    app->contour = 0.f;
    app->espacement = 0.f;
    texte(app, POLICE_NORMALE, bataille_journal(&ar->bataille, 0), 18,
        (sfVector2f){LARGEUR / 2.f, cy + 66.f}, C_TEXTE_DOUX, ALIGNE_CENTRE);
    if (bouton_plein(app, &ar->rejouer, (sfFloatRect){LARGEUR / 2.f - 250.f, cy + 110.f,
        240.f, 54.f}, "Rejouer", "Entrée", col, 1))
        demarrer(app, ar);
    if (bouton(app, &ar->quitter, (sfFloatRect){LARGEUR / 2.f + 10.f, cy + 110.f,
        240.f, 54.f}, "Menu", "Échap", NULL, C_TEXTE_DOUX, 1))
        changer_scene(app, SCENE_MENU);
    app->alpha = a;
}

/* ------------------------------------------------------------------ */
/*  Scène                                                              */
/* ------------------------------------------------------------------ */

static void maj_etat(app_t *app, arene_t *ar)
{
    action_t a = ACTION_ATTAQUER;
    ref_t cible = {0, 0};

    ar->t_etat += app->dt;
    ar->t_tour += app->dt;
    if (ar->etat == VUE_INTRO && ar->t_etat >= DUREE_INTRO)
        debut_tour(ar);
    if (ar->etat == VUE_ORDINATEUR && ar->t_etat >= DELAI_ORDINATEUR) {
        bataille_ia(&ar->bataille, &a, &cible);
        lancer(app, ar, a, cible);
    }
    jouer_evenements(app, ar);
}

void scene_arene(app_t *app)
{
    arene_t *ar = &app->arene;
    ref_t ordre[NB_EQUIPES * TAILLE_EQUIPE_MAX];
    ref_t actif = bataille_actif(&ar->bataille);
    int montre_actif = ar->etat == VUE_CHOIX_ACTION || ar->etat == VUE_CHOIX_CIBLE
        || ar->etat == VUE_ORDINATEUR;
    float intro = ar->etat == VUE_INTRO ? ar->t_etat : DUREE_INTRO;
    float e = sortie_cubique(progression(intro, 0.5f, 0.5f));
    float a = app->alpha;
    int n = 0;

    clavier(app, ar);
    souris_cibles(app, ar);
    maj_etat(app, ar);
    maj_visuels(app, ar);
    placer(app, ar);
    dessiner_arene(app, ar);
    n = ordre_dessin(ar, ordre);
    for (int i = 0; i < n; i++)
        dessiner_combattant(app, ar, ordre[i], montre_actif && meme(ordre[i], actif));
    dessiner_effet(app, ar);
    for (int i = 0; i < n; i++)
        plaque(app, ar, ordre[i], montre_actif && meme(ordre[i], actif));
    if (ar->etat == VUE_CHOIX_CIBLE)
        reticules(app, ar);
    app->alpha = a * e;
    ordre_tours(app, ar);
    panneau_equipe(app, ar, 1, (sfFloatRect){MARGE - 40.f * (1.f - e), 24.f, 360.f,
        44.f + ar->bataille.equipes[1].taille * LIGNE_A});
    journal(app, ar, 90.f + ar->bataille.equipes[1].taille * LIGNE_A);
    panneau_equipe(app, ar, 0, (sfFloatRect){LARGEUR - MARGE - CMD_L + 40.f * (1.f - e),
        HAUTEUR - MARGE - CMD_H - 14.f - (44.f + ar->bataille.equipes[0].taille * LIGNE_J),
        CMD_L, 44.f + ar->bataille.equipes[0].taille * LIGNE_J});
    if (ar->etat != VUE_FIN)
        panneau_commandes(app, ar, e);
    app->alpha = a;
    banniere(app, ar);
    dessiner_popups(app, ar);
    if (ar->etat == VUE_FIN)
        ecran_fin(app, ar);
}
