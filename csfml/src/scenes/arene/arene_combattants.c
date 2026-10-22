/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Arène : placement, dessin et plaque de chaque combattant, réticules de ciblage.
 */

#include <math.h>
#include <stdio.h>
#include "bataille.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_emblemes.h"
#include "gui_etat.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "gui_ui.h"
#include "arene_interne.h"

#define ECHELLE_ADVERSE 0.62f

/* Formations : position de chaque membre sur sa plateforme selon la taille
 * de l'équipe (x de -1 à 1, y de -1 = au fond à 1 = devant). */
static const float FORMATIONS[TAILLE_EQUIPE_MAX][TAILLE_EQUIPE_MAX][2] = {
    {{0.f, 0.f}},
    {{-0.48f, 0.3f}, {0.48f, -0.3f}},
    {{-0.64f, 0.35f}, {0.04f, -0.42f}, {0.66f, 0.3f}},
    {{-0.74f, 0.4f}, {-0.27f, -0.45f}, {0.22f, 0.45f}, {0.7f, -0.35f}},
};

static float rayon_base(int e, int n)
{
    float r = n <= 2 ? 58.f : (n == 3 ? 52.f : 46.f);

    return e == 0 ? r : r * ECHELLE_ADVERSE;
}

void arene_placer(app_t *app, arene_t *ar)
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
            e = sortie_cubique(progression(intro, ENTREE_DEBUT + i * ENTREE_PAS_RANG
                + eq * ENTREE_PAS_EQUIPE, ENTREE_DUREE));
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
int arene_ordre_dessin(const arene_t *ar, ref_t ordre[NB_EQUIPES * TAILLE_EQUIPE_MAX])
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

static sfColor couleur_pv(float ratio)
{
    if (ratio > 0.5f)
        return melanger(C_OR, C_VERT, (ratio - 0.5f) * 2.f);
    return melanger(C_ROUGE, C_OR, ratio * 2.f);
}

/** Barre de PV animée : piste, trace des PV perdus, PV actuels. */
void arene_barre_pv(app_t *app, sfFloatRect r, const visuel_t *v, int pv_max)
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

    arene_disque(app, sol, (sfVector2f){v->rayon * 1.05f, v->rayon * 0.3f},
        (sfColor){2, 3, 8, 170}, (sfColor){2, 3, 8, 0});
    cible_primitives(app, colonne, 4, sfQuads, &app->additif);
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

void arene_dessiner_combattant(app_t *app, arene_t *ar, ref_t r, int actif)
{
    const combattant_t *c = bataille_lire(&ar->bataille, r);
    visuel_t *v = arene_vis(ar, r);
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
    app->desature = mort;
    embleme(app, c->perso.classe, p, rr * 0.52f, melanger(col, C_BLANC, 0.5f));
    app->desature = 0.f;
    if (v->flash > 0.01f)
        hexagone(app, p, rr, 0.f, avec_alpha(C_BLANC, 0.75f * v->flash), C_TRANSPARENT);
    bouclier(app, p, rr, v);
    app->alpha = a;
}

/** Nom et barre de PV flottant au-dessus du personnage. */
void arene_plaque(app_t *app, arene_t *ar, ref_t r, int actif)
{
    const combattant_t *c = bataille_lire(&ar->bataille, r);
    visuel_t *v = arene_vis(ar, r);
    float w = fmaxf(84.f, v->rayon * 2.4f);
    float y = v->pos.y - v->rayon * 1.35f - (r.equipe == 0 ? 22.f : 16.f);
    unsigned int taille = r.equipe == 0 ? 15 : 12;
    float a = app->alpha;
    char nom[48];

    app->alpha = a * (v->ko_vis ? 0.6f : 1.f);
    tronquer(app, POLICE_GRAS, taille, c->perso.nom, w + 30.f, nom, sizeof(nom));
    app->contour = 2.f;
    texte(app, POLICE_GRAS, nom, taille, (sfVector2f){v->pos.x, y - (r.equipe == 0
        ? 14.f : 11.f)}, v->ko_vis ? C_TEXTE_FAIBLE : C_TEXTE, ALIGNE_CENTRE);
    app->contour = 0.f;
    if (v->ko_vis)
        pastille(app, "K.O.", (sfVector2f){v->pos.x, y + 2.f}, C_ROUGE, 1);
    else
        arene_barre_pv(app, (sfFloatRect){v->pos.x - w / 2.f, y, w, r.equipe == 0 ? 6.f
            : 4.f}, v, c->pv_max);
    if (actif) {
        lueur(app, (sfVector2f){v->pos.x, y - 40.f}, (sfVector2f){40.f, 40.f},
            avec_alpha(arene_couleur_equipe(r.equipe), 0.6f));
        losange(app, (sfVector2f){v->pos.x, y - 38.f + sinf(app->temps * 5.f) * 4.f},
            7.f, arene_couleur_equipe(r.equipe));
    }
    app->alpha = a;
}

/** Réticule autour des cibles valides ; la cible choisie affiche l'effet prévu. */
void arene_reticules(app_t *app, arene_t *ar)
{
    const combattant_t *acteur = bataille_lire(&ar->bataille,
        bataille_actif(&ar->bataille));
    sfColor col = arene_couleur_action(ar->action);
    const combattant_t *c = NULL;
    visuel_t *v = NULL;
    float r = 0.f;
    int choisie = 0;
    char buf[24];

    for (int i = 0; i < ar->nb_cibles; i++) {
        c = bataille_lire(&ar->bataille, ar->cibles[i]);
        v = arene_vis(ar, ar->cibles[i]);
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
