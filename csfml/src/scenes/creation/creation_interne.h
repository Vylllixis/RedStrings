/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Éléments partagés entre les fichiers de « creation » : à n'inclure que depuis ce dossier.
 */

#ifndef CREATION_INTERNE_H
    #define CREATION_INTERNE_H

    #include "personnage.h"
    #include "gui_etat.h"
    #include "gui_types.h"

/*
 * Formulaire à gauche, aperçu à droite. Leurs bords sont recalculés à
 * chaque image selon la largeur visible (creation_geometrie) ; à 1280 de
 * large : formulaire 64 + 600, aperçu 716 + 516.
 */
#define FX (app->creation.fx)
#define FW (app->creation.fw)
#define AX (app->creation.ax)
#define AW (app->creation.aw)
#define SCENE_X (AX + AW / 2.f)
#define SCENE_Y 250.f
#define DUREE_SORTIE_CREATION 1.f

/* Ordre du focus clavier : nom, une glissière par statistique, classe, bouton. */
#define FOCUS_NOM 0
#define FOCUS_STAT 1
#define FOCUS_CLASSE (FOCUS_STAT + NB_STATS)
#define FOCUS_BOUTON (FOCUS_CLASSE + 1)

void creation_geometrie(app_t *app, creation_t *c);
const char *creation_raison_invalide(app_t *app, const creation_t *c);
void creation_creer(app_t *app, creation_t *c);
classe_t creation_premiere_classe(categorie_t cat);
void creation_clavier(app_t *app, creation_t *c);
void creation_onglets(app_t *app, creation_t *c, float y);
void creation_grille_classes(app_t *app, creation_t *c, float y);
void creation_suivre_classe(app_t *app, creation_t *c);
void creation_hologramme(app_t *app, creation_t *c);
void creation_apercu(app_t *app, creation_t *c, float a);
void creation_main_cartes(app_t *app, creation_t *c, float y);
void creation_carte_apercu(app_t *app, creation_t *c);

#endif /* CREATION_INTERNE_H */
