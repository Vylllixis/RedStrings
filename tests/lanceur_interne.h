/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Partie privée du lanceur de tests : ce que le fils écrit (mémoire
 * partagée) et que le père lit pour construire le rapport.
 */

#ifndef LANCEUR_INTERNE_H
    #define LANCEUR_INTERNE_H

    #include <stdio.h>
    #include "lanceur.h"

    #define ECHECS_MAX 10           /**< échecs détaillés par test (les autres sont comptés) */
    #define TEXTE_COURT 96
    #define TEXTE_LONG 200

typedef struct {
    char fichier[TEXTE_COURT];
    int ligne;
    char fonction[64];
    char expr[TEXTE_LONG];          /**< la vérification, telle qu'écrite */
    char obtenu[TEXTE_LONG];
    char attendu[TEXTE_LONG];
} echec_verif_t;

/** Écrit par le fils, lu par le père : vit en mémoire partagée. */
typedef struct {
    int nb_verif;
    int nb_echecs;
    int bloque;                     /**< 1 si un échec a arrêté le test */
    char dernier_fichier[TEXTE_COURT];
    int derniere_ligne;
    char derniere_fonction[64];
    echec_verif_t echecs[ECHECS_MAX];
} rapport_t;

extern rapport_t *g_rapport;        /**< rapport du test en cours (NULL hors d'un test) */

/** Résumé de la mort d'un fils, tiré de son code de sortie et de sa sortie d'erreur. */
typedef struct {
    char titre[TEXTE_LONG];         /**< « Plantage : accès mémoire invalide (SIGSEGV) » */
    char cause[TEXTE_LONG];         /**< explication en français */
    char detail[4][TEXTE_LONG];     /**< lignes utiles du rapport des sanitizers */
    int nb_detail;
    char frames[6][TEXTE_LONG];     /**< piles d'appels : « fonction (fichier:ligne) » */
    int nb_frames;
} mort_t;

/** Analyse le statut de waitpid() et la sortie d'erreur (FILE rembobiné par l'appelant). */
void lanceur_analyser_mort(int statut, int duree_ms, FILE *erreurs, mort_t *mort);

/** Affiche une ligne de test réussi ou échoué (avec fichier:ligne de sa définition). */
void rapport_ligne_test(const char *nom, const char *fichier, int ligne, int ok, int nb_verif,
    double ms);
/** Affiche le détail d'un test échoué : vérifications ratées, ou plantage. */
void rapport_echec(const rapport_t *r, const mort_t *mort, const char *fichier, int ligne);
void rapport_entete_groupe(const char *fichier);
void rapport_pied_groupe(int reussis, int total);
int rapport_trouver_definition(const char *fichier, const char *nom, int repli);
const char *rapport_couleur(const char *code);

#define ECHECS_RESUME_MAX 128

/** Une ligne du récapitulatif final : quel test, où, et pourquoi. */
typedef struct {
    char nom[64];
    char lieu[TEXTE_LONG];          /**< « fichier:ligne » de la définition du test */
    char resume[TEXTE_LONG];        /**< première vérification ratée ou cause du plantage */
} resume_echec_t;

/** Le bilan final : barre, totaux, temps, liste des tests à corriger. */
void rapport_bilan(int total, int reussis, int verifs, double ms, const resume_echec_t *liste,
    int nb_liste);

#endif /* LANCEUR_INTERNE_H */
