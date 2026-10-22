/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Emblème de chaque classe : choix du dessin selon la classe.
 */

#include "personnage.h"
#include "gui_emblemes.h"
#include "gui_types.h"
#include "emblemes_interne.h"

typedef void (*dessin_embleme_t)(app_t *app, sfVector2f c, float t, sfColor col);

static const dessin_embleme_t DESSINS[NB_EMBLEMES] = {
    [EMB_EPEES] = emb_d_epees,
    [EMB_BOUCLIER] = emb_d_bouclier,
    [EMB_HACHE] = emb_d_hache,
    [EMB_LAME] = emb_d_lame,
    [EMB_LANCE] = emb_d_lance,
    [EMB_ETOILE] = emb_d_etoile,
    [EMB_FLAMME] = emb_d_flamme,
    [EMB_CRANE] = emb_d_crane,
    [EMB_PORTAIL] = emb_d_portail,
    [EMB_FEUILLE] = emb_d_feuille,
    [EMB_LUNE] = emb_d_lune,
    [EMB_SABLIER] = emb_d_sablier,
    [EMB_ARC] = emb_d_arc,
    [EMB_CIBLE] = emb_d_cible,
    [EMB_ANCRE] = emb_d_ancre,
    [EMB_CROIX] = emb_d_croix,
    [EMB_NOTE] = emb_d_note,
    [EMB_FIOLE] = emb_d_fiole,
    [EMB_ENGRENAGE] = emb_d_engrenage,
    [EMB_ORBE] = emb_d_orbe,
};

/** Emblème vectoriel de la classe, choisi par classes[c].embleme. */
void embleme(app_t *app, classe_t c, sfVector2f centre, float taille,
    sfColor couleur)
{
    DESSINS[classes[c].embleme](app, centre, taille, couleur);
}
