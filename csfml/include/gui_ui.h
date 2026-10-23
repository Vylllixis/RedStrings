/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Composants interactifs : boutons, champ de texte, glissières, notifications.
 */

#ifndef GUI_UI_H
    #define GUI_UI_H

    #include <SFML/Graphics.h>
    #include "gui_types.h"

int survole(app_t *app, sfFloatRect r);
int bouton(app_t *app, bouton_t *b, sfFloatRect r, const char *label,
    const char *touche, const char *info, sfColor accent, int actif);
int bouton_plein(app_t *app, bouton_t *b, sfFloatRect r, const char *label,
    const char *touche, sfColor accent, int actif);
int bouton_retour(app_t *app, bouton_t *b, sfVector2f pos);
int bouton_rond(app_t *app, bouton_t *b, sfVector2f centre, const char *sym);
int champ_texte(app_t *app, champ_t *c, sfFloatRect r, const char *aide,
    int max);
int glissiere(app_t *app, glissiere_t *g, sfFloatRect r, const char *label,
    int *valeur, sfColor couleur, int focus);
int glissiere_bornee(app_t *app, glissiere_t *g, sfFloatRect r, const char *label,
    const int bornes[2], int *valeur, sfColor couleur, int focus);
float pastille(app_t *app, const char *s, sfVector2f pos, sfColor c,
    int plein);
void notifier(app_t *app, const char *icone, const char *message,
    sfColor c);
void dessiner_toasts(app_t *app);
void en_tete_surtitre(app_t *app, bouton_t *retour, const char *surtitre,
    const char *titre, const char *sous_titre);
void en_tete(app_t *app, bouton_t *retour, const char *titre,
    const char *sous_titre);

#endif /* GUI_UI_H */
