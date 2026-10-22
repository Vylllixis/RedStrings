/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Éléments partagés entre les fichiers de « arene » : à n'inclure que depuis ce dossier.
 */

#ifndef ARENE_INTERNE_H
    #define ARENE_INTERNE_H

    #include "bataille.h"
    #include "gui_config.h"
    #include "gui_etat.h"
    #include "gui_types.h"

#define SOL_J_X (LARGEUR * 0.29f)
#define SOL_J_Y (HAUTEUR * 0.80f)
#define SOL_A_X (LARGEUR * 0.70f)
#define SOL_A_Y (HAUTEUR * 0.43f)
#define PLAT_J_RX (LARGEUR * 0.27f)
#define PLAT_J_RY (HAUTEUR * 0.105f)
#define PLAT_A_RX (LARGEUR * 0.185f)
#define PLAT_A_RY (HAUTEUR * 0.058f)

/* Interface */
#define CMD_L 468.f
#define CMD_H 128.f
#define LIGNE_J 36.f
#define LIGNE_A 32.f
#define MARGE 36.f

/* Rythme */
#define DUREE_INTRO 1.1f

extern const float ARENE_IMPACTS[];

visuel_t *arene_vis(arene_t *ar, ref_t r);
int arene_meme(ref_t a, ref_t b);
sfColor arene_couleur_equipe(int e);
sfColor arene_couleur_action(int a);
void arene_demarrer(app_t *app, arene_t *ar);
void arene_dessiner_popups(app_t *app, arene_t *ar);
void arene_placer(app_t *app, arene_t *ar);
int arene_ordre_dessin(const arene_t *ar, ref_t ordre[NB_EQUIPES * TAILLE_EQUIPE_MAX]);
void arene_disque(app_t *app, sfVector2f c, sfVector2f r, sfColor centre, sfColor bord);
void arene_dessiner_arene(app_t *app, arene_t *ar);
void arene_barre_pv(app_t *app, sfFloatRect r, const visuel_t *v, int pv_max);
void arene_dessiner_combattant(app_t *app, arene_t *ar, ref_t r, int actif);
void arene_plaque(app_t *app, arene_t *ar, ref_t r, int actif);
void arene_reticules(app_t *app, arene_t *ar);
void arene_dessiner_effet(app_t *app, arene_t *ar);
void arene_debut_tour(arene_t *ar);
void arene_lancer(app_t *app, arene_t *ar, action_t a, ref_t cible);
void arene_jouer_evenements(app_t *app, arene_t *ar);
void arene_maj_visuels(app_t *app, arene_t *ar);
void arene_clavier(app_t *app, arene_t *ar);
void arene_souris_cibles(app_t *app, arene_t *ar);
void arene_panneau_commandes(app_t *app, arene_t *ar, float e);
void arene_panneau_equipe(app_t *app, arene_t *ar, int e, sfFloatRect r);
void arene_ordre_tours(app_t *app, arene_t *ar);
void arene_journal(app_t *app, arene_t *ar, float y);
void arene_banniere(app_t *app, arene_t *ar);
void arene_ecran_fin(app_t *app, arene_t *ar);

#endif /* ARENE_INTERNE_H */
