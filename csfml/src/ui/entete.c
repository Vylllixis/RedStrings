/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ entete.c ───────────────────────────────────────────────────────────
** │ But         En-tête commun des écrans secondaires : bouton de retour,
** │             surtitre, titre et sous-titre.
** │ Contenu     en_tete_surtitre, en_tete
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_ecran.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "gui_ui.h"

/**
 * Bouton retour, surtitre (facultatif), titre et sous-titre communs aux
 * écrans secondaires. Le surtitre porte une information de contexte réelle
 * (ex. « RECRUE 03 / 10 »).
 */
void en_tete_surtitre(app_t *app, bouton_t *retour, const char *surtitre,
    const char *titre, const char *sous_titre)
{
    float t = app->t_scene;
    float e1 = sortie_cubique(progression(t, 0.f, 0.5f));
    float e2 = sortie_cubique(progression(t, 0.08f, 0.5f));
    float e3 = sortie_cubique(progression(t, 0.16f, 0.5f));
    float a = app->alpha;
    float lt = 0.f;
    float x = 64.f + decal_gauche(app);

    app->alpha = a * e1;
    if (bouton_retour(app, retour, (sfVector2f){x - 20.f * (1.f - e1), 30.f}))
        changer_scene(app, SCENE_MENU);
    app->alpha = a * e2;
    if (surtitre) {
        losange(app, (sfVector2f){x + 4.f, 98.f}, 4.f, C_ROUGE);
        etiquette(app, surtitre, (sfVector2f){x + 18.f, 98.f}, C_ROUGE, ALIGNE_GAUCHE);
    }
    app->espacement = 0.6f;
    lt = texte(app, POLICE_GRAS, titre, 36, (sfVector2f){x, 130.f + 16.f * (1.f - e2)},
        C_TEXTE, ALIGNE_GAUCHE);
    app->espacement = 0.f;
    rect_arrondi(app, (sfFloatRect){x + lt + 16.f, 128.f + 16.f * (1.f - e2),
        120.f * e2, 2.f}, 0.f, avec_alpha(C_ROUGE, 0.6f));
    app->alpha = a * e3;
    texte(app, POLICE_NORMALE, sous_titre, 15, (sfVector2f){x,
        164.f + 16.f * (1.f - e3)}, C_TEXTE_DOUX, ALIGNE_GAUCHE);
    app->alpha = a;
}

void en_tete(app_t *app, bouton_t *retour, const char *titre,
    const char *sous_titre)
{
    en_tete_surtitre(app, retour, NULL, titre, sous_titre);
}
