/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Arène : panneaux d'équipe, ordre des tours, journal, bandeau, nombres flottants, fin.
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

#define DUREE_BANNIERE 1.f

#define DUREE_POPUP 1.15f

void arene_dessiner_popups(app_t *app, arene_t *ar)
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

/** Une ligne d'équipe : nom, classe, PV ; l'actif est souligné. */
static void ligne_equipe(app_t *app, arene_t *ar, ref_t r, sfFloatRect zone)
{
    const combattant_t *c = bataille_lire(&ar->bataille, r);
    visuel_t *v = arene_vis(ar, r);
    int actif = arene_meme(r, bataille_actif(&ar->bataille)) && ar->etat != VUE_ANIMATION
        && ar->etat != VUE_FIN;
    float cy = zone.top + zone.height / 2.f;
    float bw = r.equipe == 0 ? 170.f : 120.f;
    float a = app->alpha;
    char buf[32];

    app->alpha = a * (v->ko_vis ? 0.45f : 1.f);
    if (actif)
        panneau(app, zone, 6.f, avec_alpha(arene_couleur_equipe(r.equipe), 0.16f),
            avec_alpha(arene_couleur_equipe(r.equipe), 0.04f), C_TRANSPARENT);
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
        arene_barre_pv(app, (sfFloatRect){zone.left + zone.width - bw - 8.f, cy - 3.f, bw,
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

void arene_panneau_equipe(app_t *app, arene_t *ar, int e, sfFloatRect r)
{
    float hl = e == 0 ? LIGNE_J : LIGNE_A;
    char buf[48];

    ombre_biseau(app, r, 14.f, 1.f);
    panneau(app, r, 14.f, (sfColor){24, 29, 52, 225}, (sfColor){12, 14, 28, 235},
        C_BORD);
    rect_arrondi(app, (sfFloatRect){r.left + 14.f, r.top, 90.f, 2.f}, 0.f,
        arene_couleur_equipe(e));
    etiquette(app, e == 0 ? "TON ÉQUIPE" : "ADVERSAIRES", (sfVector2f){r.left + 16.f,
        r.top + 20.f}, arene_couleur_equipe(e), ALIGNE_GAUCHE);
    snprintf(buf, sizeof(buf), "%d / %d en lice", nb_vivants_vus(ar, e),
        ar->bataille.equipes[e].taille);
    texte(app, POLICE_NORMALE, buf, 12, (sfVector2f){r.left + r.width - 16.f,
        r.top + 20.f}, C_TEXTE_FAIBLE, ALIGNE_DROITE);
    for (int i = 0; i < ar->bataille.equipes[e].taille; i++)
        ligne_equipe(app, ar, (ref_t){e, i}, (sfFloatRect){r.left + 10.f, r.top + 36.f
            + i * hl, r.width - 20.f, hl - 4.f});
}

/** Les prochains tours, à partir de l'actif (personnages en lice seulement). */
void arene_ordre_tours(app_t *app, arene_t *ar)
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
            arene_couleur_equipe(r.equipe));
        x += 40.f + 8.f * t;
        vus++;
    }
    app->alpha = a;
}

/** Journal : seulement les lignes déjà jouées à l'écran, les plus récentes en clair. */
void arene_journal(app_t *app, arene_t *ar, float y)
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
void arene_banniere(app_t *app, arene_t *ar)
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
        2.f}, 0.f, arene_couleur_equipe(bataille_actif(&ar->bataille).equipe));
    snprintf(buf, sizeof(buf), "TOUR DE %s", c->perso.nom);
    app->espacement = 3.f;
    texte(app, POLICE_GRAS, buf, 20, (sfVector2f){LARGEUR / 2.f + 30.f * (1.f - e), cy},
        C_TEXTE, ALIGNE_CENTRE);
    app->espacement = 0.f;
    app->alpha = a;
}

void arene_ecran_fin(app_t *app, arene_t *ar)
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
        arene_demarrer(app, ar);
    if (bouton(app, &ar->quitter, (sfFloatRect){LARGEUR / 2.f + 10.f, cy + 110.f,
        240.f, 54.f}, "Menu", "Échap", NULL, C_TEXTE_DOUX, 1))
        changer_scene(app, SCENE_MENU);
    app->alpha = a;
}
