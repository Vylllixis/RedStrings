/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ creer_personnage.c ─────────────────────────────────────────────────
** │ But         Guider le joueur pour créer un personnage : un nom, des
** │             statistiques de base, puis une classe qui les modifie.
** │ Contenu     lire_stat, ecrire_mod, afficher_choix_classe,
** │             choisir_classe, creer_personnage
** │ Utilise     saisie.c (lire_entier), la table classes[] de
** │             personnage.c
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include "boite.h"
#include "couleurs.h"
#include "personnage.h"
#include "saisie.h"

static int saisie_interrompue(void)
{
    afficher_erreur("Saisie interrompue");
    return -1;
}

static int lire_stat(const char *libelle, int *stat)
{
    char libelle_aligne[64];

    aligner_texte(libelle_aligne, sizeof(libelle_aligne), libelle,
        LARGEUR_LIBELLE);
    return lire_entier(libelle_aligne, STAT_MIN, STAT_MAX, stat);
}

/** « +10 » en vert, « -5 » en rouge, « 0 » en gris. */
static void ecrire_mod(char *dst, size_t taille, const char *stat, int mod)
{
    snprintf(dst, taille, GRIS "%s " RESET "%s%+3d" RESET, stat,
        mod > 0 ? VERT : (mod < 0 ? ROUGE : GRIS), mod);
}

static void afficher_choix_classe(int numero, classe_t c)
{
    static const char *stats[NB_STATS] = {"PV", "ATQ", "DEF", "SOI"};
    char nom[64];
    char mods[NB_STATS][48];

    aligner_texte(nom, sizeof(nom), classes[c].nom, 16);
    for (int s = 0; s < NB_STATS; s++)
        ecrire_mod(mods[s], sizeof(mods[s]), stats[s], classes[c].mods[s]);
    printf("    " GRAS "%2d" RESET "  %s%s" RESET "  %s  %s  %s  %s\n", numero,
        couleur_ansi(c), nom, mods[STAT_PV], mods[STAT_ATQ], mods[STAT_DEF],
        mods[STAT_SOIN]);
}

/** Liste le catalogue par catégorie ; renvoie la classe choisie ou -1. */
static int choisir_classe(classe_t *classe)
{
    classe_t ordre[NB_CLASSES];
    int choix = 1;

    ordre_classes(ordre);
    printf("  " GRIS "── Classe ──" RESET "\n");
    for (int i = 0; i < NB_CLASSES; i++) {
        if (i == 0 || classes[ordre[i]].categorie != classes[ordre[i - 1]].categorie)
            printf("  " GRAS "%s" RESET "\n", nom_categorie(classes[ordre[i]].categorie));
        afficher_choix_classe(i + 1, ordre[i]);
    }
    if (lire_entier("Choix", 1, NB_CLASSES, &choix) == -1)
        return -1;
    *classe = ordre[choix - 1];
    return 0;
}

/*
** ╭─ creer_personnage ───────────────────────────────────────────────────
** │ Crée un personnage en trois temps.
** │
** │ 1. le nom : lu avec une taille limite, refusé s'il existe déjà dans
** │    tab
** │ 2. les statistiques de base (PV, attaque, défense, soin)
** │ 3. la classe, puis appliquer_classe() ajoute ses modificateurs
** │ Renvoie 0 si le personnage est créé, -1 s'il est annulé (nom pris,
** │ Ctrl+D). L'ajout dans la liste est fait par l'appelant.
** ╰──────────────────────────────────────────────────────────────────────
*/
int creer_personnage(personnage_t *p, personnage_t *tab, int len)
{
    //en-tête
    printf("\n");
    boite_haut(CYAN);
    boite_ligne(CYAN, "  " GRAS "✦ Création d'un personnage" RESET);
    boite_bas(CYAN);
    printf("\n");

    //choix du nom
    printf("  " GRAS "Nom" RESET CYAN " › " RESET);
    if (scanf("%" STR(NOM_LONGUEUR) "s", p->nom) != 1)
        return saisie_interrompue();
    vider_buffer();
    if (trouver_personnage(tab, len, p->nom) != -1) {
        afficher_erreur("Ce nom est déjà pris");
        return -1;
    }
    printf("\n");

    //choix des statistiques de base (la classe les modifie ensuite)
    printf("  " GRIS "── Statistiques de base ──" RESET "\n");
    if (lire_stat("Points de vie", &p->pv) == -1
        || lire_stat("Attaque", &p->atq) == -1
        || lire_stat("Défense", &p->def) == -1
        || lire_stat("Soin", &p->soin) == -1)
        return saisie_interrompue();
    printf("\n");

    //choix de la classe, puis ses modificateurs sur les statistiques de base
    if (choisir_classe(&p->classe) == -1)
        return saisie_interrompue();
    appliquer_classe(p);
    printf("\n  " VERT "✔ %s a été créé !" RESET "\n", p->nom);
    afficher_personnage(p);
    return 0;
}
