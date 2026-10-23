/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ audio_evenements.c ─────────────────────────────────────────────────
** │ But         La liste des événements sonores et leurs règles communes
** │             à tous les thèmes : nom dans le manifeste, priorité, délai
** │             minimum entre deux lectures, lectures simultanées, variation
** │             de hauteur.
** │ Contenu     EVENEMENTS[], son_info, son_depuis_nom, musique_depuis_nom,
** │             nom_musique, nom_theme, dossier_theme
** │ Règle       aucun SFML ici : testé par csfml/tests/tests_audio.c
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <string.h>
#include "gui_sons.h"

/*
** ╭─ EVENEMENTS[] ───────────────────────────────────────────────────────
** │ Une ligne par événement : {nom, priorité, délai, max, hauteur}.
** │
** │ - priorité 1 : sons discrets et fréquents (survol, saisie) ; ils
** │   cèdent leur voix aux autres
** │ - priorité 3 : moments importants ; la musique baisse le temps du son
** │ - délai : deux déclenchements plus rapprochés ne jouent qu'une fois
** │   (une animation qui se répète ne mitraille pas)
** │ - hauteur : petite variation aléatoire, pour qu'un clic répété ne
** │   lasse pas l'oreille
** ╰──────────────────────────────────────────────────────────────────────
*/
static const info_son_t EVENEMENTS[NB_SONS] = {
    [SON_UI_SURVOL] = {"ui.survol", 1, 0.06f, 2, 0.06f},
    [SON_UI_VALIDER] = {"ui.valider", 2, 0.05f, 2, 0.02f},
    [SON_UI_DEPLACEMENT] = {"ui.deplacement", 1, 0.04f, 3, 0.04f},
    [SON_UI_OUVERTURE] = {"ui.ouverture", 2, 0.10f, 1, 0.02f},
    [SON_UI_FERMETURE] = {"ui.fermeture", 2, 0.10f, 1, 0.02f},
    [SON_UI_ONGLET] = {"ui.onglet", 1, 0.05f, 2, 0.03f},
    [SON_UI_SAISIE] = {"ui.saisie", 1, 0.025f, 4, 0.08f},
    [SON_UI_EFFACEMENT] = {"ui.effacement", 1, 0.03f, 3, 0.05f},
    [SON_UI_REFUS] = {"ui.refus", 1, 0.12f, 1, 0.03f},
    [SON_UI_VALEUR] = {"ui.valeur", 1, 0.035f, 3, 0.f},
    [SON_UI_ERREUR] = {"ui.erreur", 2, 0.25f, 1, 0.02f},
    [SON_UI_SUCCES] = {"ui.succes", 2, 0.20f, 1, 0.02f},
    [SON_UI_INFO] = {"ui.info", 2, 0.20f, 1, 0.f},
    [SON_ECRAN_TRANSITION] = {"ecran.transition", 2, 0.30f, 1, 0.03f},
    [SON_ECRAN_RETOUR] = {"ecran.retour", 2, 0.30f, 1, 0.03f},
    [SON_APP_OUVERTURE] = {"app.ouverture", 3, 1.f, 1, 0.f},
    [SON_APP_FERMETURE] = {"app.fermeture", 3, 1.f, 1, 0.f},
    [SON_SECRET_KONAMI] = {"secret.konami", 3, 1.f, 1, 0.f},
    [SON_CARTE_SELECTION] = {"carte.selection", 2, 0.04f, 3, 0.05f},
    [SON_CARTE_RETRAIT] = {"carte.retrait", 2, 0.04f, 3, 0.05f},
    [SON_CARTE_OUVERTURE] = {"carte.ouverture", 2, 0.10f, 1, 0.02f},
    [SON_CARTE_DEFILEMENT] = {"carte.defilement", 1, 0.06f, 2, 0.05f},
    [SON_CLASSE_CHANGEMENT] = {"classe.changement", 1, 0.05f, 2, 0.04f},
    [SON_EQUIPE_VALIDEE] = {"equipe.validee", 2, 0.30f, 1, 0.f},
    [SON_PERSONNAGE_CREE] = {"personnage.cree", 3, 0.50f, 1, 0.f},
    [SON_RECHERCHE_VIDE] = {"recherche.vide", 1, 0.60f, 1, 0.f},
    [SON_SAUVEGARDE_OK] = {"sauvegarde.ok", 3, 0.50f, 1, 0.f},
    [SON_CHARGEMENT_OK] = {"chargement.ok", 3, 0.50f, 1, 0.f},
    [SON_COMBAT_DEBUT] = {"combat.debut", 3, 0.50f, 1, 0.f},
    [SON_COMBAT_TOUR] = {"combat.tour", 2, 0.30f, 1, 0.f},
    [SON_COMBAT_ACTION] = {"combat.action", 2, 0.08f, 2, 0.05f},
    [SON_COMBAT_IMPACT] = {"combat.impact", 2, 0.03f, 3, 0.06f},
    [SON_COMBAT_IMPACT_GARDE] = {"combat.impact_garde", 2, 0.05f, 2, 0.04f},
    [SON_COMBAT_IMPACT_LOURD] = {"combat.impact_lourd", 3, 0.05f, 2, 0.03f},
    [SON_COMBAT_SOIN] = {"combat.soin", 2, 0.05f, 2, 0.03f},
    [SON_COMBAT_GARDE] = {"combat.garde", 2, 0.05f, 2, 0.02f},
    [SON_COMBAT_GARDE_FIN] = {"combat.garde_fin", 1, 0.05f, 2, 0.02f},
    [SON_COMBAT_KO] = {"combat.ko", 3, 0.05f, 2, 0.f},
    [SON_COMBAT_VICTOIRE] = {"combat.victoire", 3, 1.f, 1, 0.f},
    [SON_COMBAT_DEFAITE] = {"combat.defaite", 3, 1.f, 1, 0.f},
};

static const char *MUSIQUES[NB_MUSIQUES] = {
    [MUSIQUE_MENU] = "musique.menu",
    [MUSIQUE_COMBAT] = "musique.combat",
};

static const char *THEMES[NB_THEMES][2] = {
    [THEME_STANDARD] = {"Standard", "standard"},
    [THEME_YUGIOH] = {"Yu-Gi-Oh!", "yugioh"},
};

const info_son_t *son_info(son_t s)
{
    if ((int)s < 0 || s >= NB_SONS)
        return NULL;
    return &EVENEMENTS[s];
}

/* Indice de l'événement nommé `nom`, ou -1 s'il n'existe pas. */
int son_depuis_nom(const char *nom)
{
    for (int i = 0; i < NB_SONS; i++) {
        if (strcmp(EVENEMENTS[i].nom, nom) == 0)
            return i;
    }
    return -1;
}

int musique_depuis_nom(const char *nom)
{
    for (int i = 0; i < NB_MUSIQUES; i++) {
        if (strcmp(MUSIQUES[i], nom) == 0)
            return i;
    }
    return -1;
}

const char *nom_musique(musique_t m)
{
    return (int)m >= 0 && m < NB_MUSIQUES ? MUSIQUES[m] : "";
}

const char *nom_theme(theme_son_t t)
{
    return (int)t >= 0 && t < NB_THEMES ? THEMES[t][0] : "";
}

/* Sous-dossier de csfml/assets/audio/ qui contient le pack du thème. */
const char *dossier_theme(theme_son_t t)
{
    return (int)t >= 0 && t < NB_THEMES ? THEMES[t][1] : THEMES[THEME_STANDARD][1];
}
