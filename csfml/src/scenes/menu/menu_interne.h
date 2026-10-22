/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Éléments partagés entre les fichiers de « menu » : à n'inclure que depuis ce dossier.
 */

#ifndef MENU_INTERNE_H
    #define MENU_INTERNE_H

    #include "gui_app.h"
    #include "gui_types.h"

int menu_option_principale(app_t *app);
void menu_fil_rouge(app_t *app, float y, float demi, float p);
void menu_titre(app_t *app, float t);
void menu_action(app_t *app, int choix);
int menu_touche_menu(app_t *app);
int menu_nb_ko(app_t *app);
void menu_commandes(app_t *app, float t);
void menu_panneau_equipe(app_t *app, float t);
void menu_aide_menu(app_t *app, float t);
void menu_credit_claude(app_t *app, float t);

#endif /* MENU_INTERNE_H */
