/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ menu_personnages.c ─────────────────────────────────────────────────
** │ But         Les actions du menu principal qui gèrent les personnages :
** │             ajouter, afficher, sauvegarder, charger (remplace) et
** │             fusionner (ajoute), avec les garde-fous qui évitent de
** │             perdre du travail.
** │ Contenu     menu_indisponible, demander_personnage, menu_ajouter,
** │             menu_afficher, menu_sauvegarder, avertir_ecrasement,
** │             menu_charger, menu_fusionner
** │ Utilise     liste_console.c (la liste des noms), sauvegarde.c, saisie.c
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include "boite.h"
#include "couleurs.h"
#include "liste_console.h"
#include "menu_personnages.h"
#include "my_string.h"
#include "personnage.h"
#include "sauvegarde.h"
#include "saisie.h"

#define PLURIEL(n) ((n) > 1 ? "s" : "")

/*
** ╭─ menu_indisponible ──────────────────────────────────────────────────
** │ Dit si une entrée du menu a un sens dans l'état actuel.
** │
** │ - Afficher : il faut au moins 1 personnage
** │ - Combat : il faut au moins 2 personnages en forme (non K.O.)
** │ - Sauvegarder : il faut au moins 1 personnage, sinon on écraserait la
** │   sauvegarde existante par un fichier vide
** │ Le menu grise ces entrées, et la boucle principale refuse de les lancer
** │ en affichant cette raison.
** ╰──────────────────────────────────────────────────────────────────────
*/
const char *menu_indisponible(const roster_t *r, int choix)
{
    if (choix == CHOIX_AFFICHER && r->len < 1)
        return "Aucun personnage : crée-en un d'abord (option 1)";
    if (choix == CHOIX_COMBAT && roster_nb_en_forme(r) < 2)
        return "Il faut au moins 2 personnages en forme pour un combat";
    if (choix == CHOIX_SAUVEGARDER && r->len < 1)
        return "Rien à sauvegarder : crée d'abord un personnage (option 1)";
    return NULL;
}

/* Redemande jusqu'à un nom ou un ID connu. Renvoie l'indice, SAISIE_ANNULEE (q) ou SAISIE_FIN. */
static int demander_personnage(const roster_t *r, const char *question)
{
    char saisie[64];
    int idx = 0;

    while (1) {
        printf("  " GRAS "%s" RESET GRIS " (nom ou ID · " MOT_ANNULER " annule)" RESET CYAN " › " RESET,
            question);
        if (lire_ligne(saisie, sizeof(saisie)) == -1)
            return SAISIE_FIN;
        if (my_strcmp(saisie, MOT_ANNULER) == 0)
            return SAISIE_ANNULEE;
        if (saisie[0] == '\0')
            continue;
        idx = roster_trouver(r, saisie);
        if (idx != -1)
            return idx;
        afficher_erreur("Personnage introuvable");
    }
}

void menu_ajouter(roster_t *r)
{
    personnage_t nouveau = {0};

    if (creer_personnage(&nouveau, r->data, r->len) == 0
        && roster_ajouter(r, &nouveau) == -1)
        afficher_erreur("Mémoire insuffisante");
    printf("\n");
}

void menu_afficher(const roster_t *r)
{
    int idx = 0;

    afficher_liste(r, NULL, 0);
    idx = demander_personnage(r, "Quel personnage ?");
    if (idx >= 0)
        afficher_personnage(&r->data[idx]);
    else if (idx == SAISIE_ANNULEE)
        printf("  " GRIS "↩ Annulé." RESET "\n\n");
}

void menu_sauvegarder(const roster_t *r)
{
    sauvegarde_t code = sauvegarder(r, FICHIER_SAUVEGARDE);

    if (code != SAUVEGARDE_OK)
        afficher_erreur(sauvegarde_message(code));
    else
        printf("  " VERT "✔ %d personnage(s) sauvegardé(s) dans %s" RESET "\n\n",
            r->len, FICHIER_SAUVEGARDE);
}

static void avertir_ecrasement(const roster_t *r)
{
    boite_haut(ROUGE);
    boite_ligne(ROUGE, "  " GRAS "◆ Attention : écrasement" RESET);
    boite_separateur(ROUGE);
    boite_ligne(ROUGE, "  Charger REMPLACE tous les");
    boite_ligne(ROUGE, "  personnages actuels par ceux de");
    boite_ligne(ROUGE, "  %s. Ce qui n'est", FICHIER_SAUVEGARDE);
    boite_ligne(ROUGE, "  pas sauvegardé sera perdu.");
    boite_ligne(ROUGE, "  " GRIS "Pour les GARDER : option %d." RESET, CHOIX_FUSIONNER);
    boite_bas(ROUGE);
    printf("  " GRIS "(%d personnage%s en mémoire)" RESET "\n", r->len, PLURIEL(r->len));
}

/*
** ╭─ menu_charger ───────────────────────────────────────────────────────
** │ Remplace tous les personnages par ceux du fichier.
** │
** │ 1. s'il y a déjà des personnages, un avertissement rouge dit qu'ils
** │    seront écrasés, puis confirmer() demande o/n (le défaut est non)
** │ 2. charger() est « tout ou rien » : un fichier invalide ne change rien
** │ 3. après un chargement réussi, la liste des noms est affichée
** │ Avec une liste vide, rien n'est écrasé : pas de question inutile.
** ╰──────────────────────────────────────────────────────────────────────
*/
void menu_charger(roster_t *r)
{
    sauvegarde_t code = SAUVEGARDE_OK;

    if (r->len > 0) {
        avertir_ecrasement(r);
        if (!confirmer("Remplacer quand même ?")) {
            printf("  " GRIS "↩ Chargement annulé : tes personnages sont conservés." RESET "\n\n");
            return;
        }
    }
    code = charger(r, FICHIER_SAUVEGARDE);
    if (code != SAUVEGARDE_OK) {
        afficher_erreur(sauvegarde_message(code));
        return;
    }
    printf("  " VERT "✔ %d personnage%s chargé%s depuis %s" RESET "\n\n", r->len,
        PLURIEL(r->len), PLURIEL(r->len), FICHIER_SAUVEGARDE);
    afficher_liste(r, NULL, 0);
}

void menu_fusionner(roster_t *r)
{
    int ajoutes = 0;
    int ignores = 0;
    sauvegarde_t code = fusionner(r, FICHIER_SAUVEGARDE, &ajoutes, &ignores);

    if (code != SAUVEGARDE_OK) {
        afficher_erreur(sauvegarde_message(code));
        return;
    }
    if (ajoutes > 0)
        printf("  " VERT "✔ %d personnage%s ajouté%s depuis %s" RESET "\n", ajoutes,
            PLURIEL(ajoutes), PLURIEL(ajoutes), FICHIER_SAUVEGARDE);
    else
        printf("  " GRIS "Aucun personnage ajouté." RESET "\n");
    if (ignores > 0)
        printf("  " GRIS "%d ignoré%s : leur nom existe déjà (les tiens sont gardés)."
            RESET "\n", ignores, PLURIEL(ignores));
    printf("\n");
    afficher_liste(r, NULL, 0);
}
