/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Éléments partagés entre les fichiers de « emblemes » : à n'inclure que depuis ce dossier.
 */

#ifndef EMBLEMES_INTERNE_H
    #define EMBLEMES_INTERNE_H

    #include "gui_types.h"

extern const sfColor emb_TROU;

sfVector2f emb_pt(sfVector2f c, float t, float x, float y);
float emb_ep(float t, float f);
void emb_trait(app_t *app, sfVector2f c, float t, const float xy[4], float e, sfColor col);
void emb_epee(app_t *app, sfVector2f c, float t, float angle, sfColor col);
void emb_etoile(app_t *app, sfVector2f c, float t, float angle, sfColor col);
void emb_d_epees(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_lame(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_bouclier(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_hache(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_lance(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_etoile(app_t *app, sfVector2f c, float t, sfColor col);
void emb_flamme(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_flamme(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_crane(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_portail(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_feuille(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_lune(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_sablier(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_arc(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_cible(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_ancre(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_croix(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_note(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_fiole(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_engrenage(app_t *app, sfVector2f c, float t, sfColor col);
void emb_d_orbe(app_t *app, sfVector2f c, float t, sfColor col);

#endif /* EMBLEMES_INTERNE_H */
