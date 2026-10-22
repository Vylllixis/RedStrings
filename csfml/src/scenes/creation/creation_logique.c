/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Création : règles (validation, voisinage des classes, ajustement) et clavier.
 */

#include <stdio.h>
#include "personnage.h"
#include "roster.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_etat.h"
#include "gui_particules.h"
#include "gui_types.h"
#include "gui_ui.h"
#include "creation_interne.h"

#define NB_FOCUS (FOCUS_BOUTON + 1)

/**
 * Pourquoi le personnage ne peut pas être créé, ou NULL s'il peut l'être.
 * Seule source de ces règles à l'écran : le bouton et creation_creer() l'utilisent.
 */
const char *creation_raison_invalide(app_t *app, const creation_t *c)
{
    if (c->nom.texte[0] == '\0')
        return "Donne un nom à ton personnage";
    if (trouver_personnage(app->equipe.data, app->equipe.len, c->nom.texte) != -1)
        return "Ce nom est déjà pris";
    if (app->equipe.len >= MAX_PERSONNAGES)
        return "Nombre maximum de personnages atteint";
    return NULL;
}

/** Équivalent graphique de creer_personnage(). */
void creation_creer(app_t *app, creation_t *c)
{
    const char *raison = creation_raison_invalide(app, c);
    personnage_t p = {0};
    char msg[96];

    if (raison != NULL) {
        notifier(app, "✗", raison, C_ROUGE);
        if (app->equipe.len < MAX_PERSONNAGES) {
            c->nom.secousse = 1.f;
            c->focus = FOCUS_NOM;
        }
        return;
    }
    snprintf(p.nom, sizeof(p.nom), "%s", c->nom.texte);
    p.pv = c->valeurs[0];
    p.atq = c->valeurs[1];
    p.def = c->valeurs[2];
    p.soin = c->valeurs[3];
    p.classe = c->classe;
    appliquer_classe(&p);
    if (roster_ajouter(&app->equipe, &p) == -1) {
        notifier(app, "✗", "Mémoire insuffisante", C_ROUGE);
        return;
    }
    snprintf(msg, sizeof(msg), "%s a été créé !", p.nom);
    notifier(app, "✔", msg, C_VERT);
    emettre(app, (sfVector2f){SCENE_X, SCENE_Y}, 90, couleur_classe(p.classe), 640.f, 4.5f);
    emettre(app, (sfVector2f){SCENE_X, SCENE_Y}, 30, C_BLANC, 420.f, 3.f);
    c->flash = 1.f;
    c->sortie = DUREE_SORTIE_CREATION;
    c->focus = -1;
}

/** Classe suivante (sens = 1) ou précédente dans l'ordre des catégories. */
static classe_t classe_voisine(classe_t c, int sens)
{
    classe_t ordre[NB_CLASSES];
    int i = 0;

    ordre_classes(ordre);
    while (ordre[i] != c)
        i++;
    return ordre[(i + sens + NB_CLASSES) % NB_CLASSES];
}

/** Première classe d'une catégorie (pour les creation_onglets). */
classe_t creation_premiere_classe(categorie_t cat)
{
    for (int i = 0; i < NB_CLASSES; i++) {
        if (classe_info(i)->categorie == cat)
            return (classe_t)i;
    }
    return GUERRIER;
}

static classe_t categorie_voisine(classe_t c, int sens)
{
    int cat = (classe_info(c)->categorie + sens + NB_CATEGORIES) % NB_CATEGORIES;

    return creation_premiere_classe((categorie_t)cat);
}

static void ajuster(int *v, int delta)
{
    *v += delta;
    if (*v < STAT_MIN)
        *v = STAT_MIN;
    if (*v > STAT_MAX)
        *v = STAT_MAX;
}

void creation_clavier(app_t *app, creation_t *c)
{
    const int *k = app->in.touches;
    int pas = app->in.shift ? 10 : 1;

    if (k[sfKeyEscape]) {
        changer_scene(app, SCENE_MENU);
        return;
    }
    if (k[sfKeyTab])
        c->focus = (c->focus + (app->in.shift ? NB_FOCUS - 1 : 1)) % NB_FOCUS;
    if (k[sfKeyDown] && c->focus < FOCUS_BOUTON)
        c->focus++;
    if (k[sfKeyUp] && c->focus > FOCUS_NOM)
        c->focus--;
    if (c->focus >= FOCUS_STAT && c->focus < FOCUS_CLASSE
        && (k[sfKeyLeft] || k[sfKeyRight]))
        ajuster(&c->valeurs[c->focus - FOCUS_STAT], k[sfKeyLeft] ? -pas : pas);
    if (c->focus == FOCUS_CLASSE && (k[sfKeyLeft] || k[sfKeyRight]))
        c->classe = app->in.shift ? categorie_voisine(c->classe, k[sfKeyLeft] ? -1 : 1)
            : classe_voisine(c->classe, k[sfKeyLeft] ? -1 : 1);
    if (k[sfKeyEnter])
        creation_creer(app, c);
}
