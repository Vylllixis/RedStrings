/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ arene_evenements.c ─────────────────────────────────────────────────
** │ But         Arène : lecture des événements du moteur, impacts, élan
** │             des attaques, animations.
** │ Contenu     popup, impact, elan_attaque, arene_jouer_evenements,
** │             arene_maj_visuels
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include <stdio.h>
#include "bataille.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_etat.h"
#include "gui_particules.h"
#include "gui_types.h"
#include "arene_interne.h"

/* Durée de chaque événement et instant de son impact (secondes). */
static const float DUREES[] = {
    [EV_ATTAQUE] = 0.74f, [EV_SOIN] = 0.85f, [EV_GARDE] = 0.55f,
    [EV_GARDE_FIN] = 0.3f, [EV_KO] = 0.85f, [EV_VICTOIRE] = 0.5f,
};

const float ARENE_IMPACTS[] = {
    [EV_ATTAQUE] = 0.30f, [EV_SOIN] = 0.50f, [EV_GARDE] = 0.10f,
    [EV_GARDE_FIN] = 0.f, [EV_KO] = 0.05f, [EV_VICTOIRE] = 0.f,
};

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

/*
** ╭─ impact ─────────────────────────────────────────────────────────────
** │ Le moment de l'impact : l'écran rattrape le résultat déjà calculé par
** │ le moteur.
** │
** │ - ATTAQUE : PV affichés = pv_apres, flash, secousse, nombre rouge
** │   (bleu si garde), étincelles
** │ - SOIN : PV affichés, lueur verte, nombre vert
** │ - GARDE / GARDE_FIN : bouclier affiché ou retiré
** │ - K.O. : le combattant passe en K.O., étincelles grises, et un court
** │   « hitstop » (0,08 s) souligne le coup
** ╰──────────────────────────────────────────────────────────────────────
*/
static void impact(app_t *app, arene_t *ar, const evenement_t *ev)
{
    visuel_t *c = arene_vis(ar, ev->cible);
    char buf[24];

    audio_impact(app, ev, bataille_lire(&ar->bataille, ev->cible)->pv_max);
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
    visuel_t *a = arene_vis(ar, ev->acteur);
    visuel_t *c = arene_vis(ar, ev->cible);
    sfVector2f d = {c->pos.x - a->pos.x, c->pos.y - a->pos.y};
    float l = sqrtf(d.x * d.x + d.y * d.y);
    float k = 0.f;

    if (l < 1.f)
        return;
    d = (sfVector2f){d.x / l, d.y / l};
    if (t < 0.12f)
        k = -14.f * sortie_cubique(t / 0.12f);
    else if (t < ARENE_IMPACTS[EV_ATTAQUE])
        k = lerpf(-14.f, l * 0.6f, entree_cubique((t - 0.12f) / 0.18f));
    else
        k = l * 0.6f * (1.f - sortie_cubique(progression(t, ARENE_IMPACTS[EV_ATTAQUE], 0.32f)));
    a->elan = (sfVector2f){d.x * k, d.y * k};
}

/*
** ╭─ arene_jouer_evenements ─────────────────────────────────────────────
** │ Rejoue, un par un, les événements déjà calculés par le moteur
** │ (ar->file).
** │
** │ 1. t_evt compte le temps de l'événement courant (dt_jeu : ralenti
** │    pendant le hitstop)
** │ 2. à l'instant ARENE_IMPACTS[type], impact() met à jour ce que l'on
** │    voit ; une seule fois par événement (impact_fait)
** │ 3. quand DUREES[type] est écoulé, on passe à l'événement suivant
** │ 4. file épuisée : VUE_FIN s'il y a un gagnant, sinon
** │    arene_debut_tour()
** ╰──────────────────────────────────────────────────────────────────────
*/
void arene_jouer_evenements(app_t *app, arene_t *ar)
{
    if (ar == NULL)
        return;

    if (app == NULL)
        return;

    const evenement_t *ev = NULL;

    if (ar->etat != VUE_ANIMATION)
        return;
    while (ar->i_file < ar->nb_file) {
        ev = &ar->file[ar->i_file];
        ar->t_evt += app->dt_jeu;
        if (ev->type == EV_ATTAQUE)
            elan_attaque(ar, ev, ar->t_evt);
        if (ev->type == EV_SOIN && ar->t_evt < 0.3f)
            arene_vis(ar, ev->acteur)->lueur_soin = 1.f;
        if (!ar->impact_fait && ar->t_evt >= ARENE_IMPACTS[ev->type]) {
            ar->impact_fait = 1;
            impact(app, ar, ev);
        }
        if (ar->t_evt < DUREES[ev->type])
            return;
        arene_vis(ar, ev->acteur)->elan = (sfVector2f){0.f, 0.f};
        ar->i_file++;
        ar->t_evt = 0.f;
        ar->impact_fait = 0;
    }
    if (ar->bataille.gagnant != -1) {
        ar->etat = VUE_FIN;
        ar->t_etat = 0.f;
    } else {
        arene_debut_tour(ar);
    }
}

/** Animations continues : barres, bouclier, éclats, K.O. (pilotées par le temps). */
void arene_maj_visuels(app_t *app, arene_t *ar)
{
    if (app == NULL || ar == NULL)
        return;

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
