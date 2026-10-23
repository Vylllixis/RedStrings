/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ creer_personnage.c ─────────────────────────────────────────────────
** │ But         Guider le joueur pour créer un personnage : un nom, des
** │             statistiques de base, puis une classe qui les modifie.
** │ Contenu     fin_saisie, lire_stat, lire_stats, ecrire_mod,
** │             afficher_choix_classe, choisir_classe, creer_personnage
** │ Utilise     saisie.c (lire_entier), la table classes[] de
** │             personnage.c
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include "boite.h"
#include "couleurs.h"
#include "personnage.h"
#include "saisie.h"

/* Dit pourquoi la création s'arrête, puis renvoie le code reçu. */
static int fin_saisie(int code)
{
    if (code == SAISIE_ANNULEE)
        printf("\n  " GRIS "↩ Création annulée : rien n'a été enregistré." RESET "\n");
    else
        afficher_erreur("Saisie interrompue");
    return code;
}

static int lire_stat(const char *libelle, int *stat)
{
    char libelle_aligne[64];

    aligner_texte(libelle_aligne, sizeof(libelle_aligne), libelle,
        LARGEUR_LIBELLE);
    return lire_entier(libelle_aligne, STAT_MIN, STAT_MAX, stat);
}

/* Les quatre statistiques de base, dans l'ordre ; s'arrête au premier refus. */
static int lire_stats(personnage_t *p)
{
    static const char *libelles[NB_STATS] = {"Points de vie", "Attaque", "Défense", "Soin"};
    int *champs[NB_STATS] = {&p->pv, &p->atq, &p->def, &p->soin};
    int code = 0;

    for (int i = 0; i < NB_STATS && code == 0; i++)
        code = lire_stat(libelles[i], champs[i]);
    return code;
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

/** Liste le catalogue par catégorie ; renvoie 0 (classe choisie), SAISIE_FIN ou SAISIE_ANNULEE. */
static int choisir_classe(classe_t *classe)
{
    classe_t ordre[NB_CLASSES];
    int choix = 1;
    int code = 0;

    ordre_classes(ordre);
    printf("  " GRIS "── Classe ──" RESET "\n");
    for (int i = 0; i < NB_CLASSES; i++) {
        if (i == 0 || classes[ordre[i]].categorie != classes[ordre[i - 1]].categorie)
            printf("  " GRAS "%s" RESET "\n", nom_categorie(classes[ordre[i]].categorie));
        afficher_choix_classe(i + 1, ordre[i]);
    }
    code = lire_entier("Choix", 1, NB_CLASSES, &choix);
    if (code != 0)
        return code;
    *classe = ordre[choix - 1];
    return 0;
}

/*
** ╭─ creer_personnage ───────────────────────────────────────────────────
** │ Crée un personnage en trois temps, que l'on peut annuler à chacun.
** │
** │ 1. le nom : lu avec lire_nom() ; un nom déjà pris dans tab est redemandé
** │ 2. les statistiques de base (PV, attaque, défense, soin)
** │ 3. la classe, puis appliquer_classe() ajoute ses modificateurs
** │ À chaque question, `q` annule : le personnage n'existe que dans *p, et
** │ c'est l'appelant qui l'ajoute à la liste, donc rien n'est enregistré.
** │ Renvoie 0 si le personnage est créé, SAISIE_ANNULEE (q) ou SAISIE_FIN
** │ (Ctrl+D) sinon.
** ╰──────────────────────────────────────────────────────────────────────
*/
int creer_personnage(personnage_t *p, personnage_t *tab, int len)
{
    int code = 0;

    //en-tête
    printf("\n");
    boite_haut(CYAN);
    boite_ligne(CYAN, "  " GRAS "✦ Création d'un personnage" RESET);
    boite_bas(CYAN);
    printf("  " GRIS "Tape " MOT_ANNULER " à n'importe quelle question pour annuler." RESET "\n\n");

    //choix du nom
    while (1) {
        code = lire_nom("Nom", p->nom, sizeof(p->nom));
        if (code != 0)
            return fin_saisie(code);
        if (trouver_personnage(tab, len, p->nom) == -1)
            break;
        afficher_erreur("Ce nom est déjà pris");
    }
    printf("\n");

    //choix des statistiques de base (la classe les modifie ensuite)
    printf("  " GRIS "── Statistiques de base ──" RESET "\n");
    code = lire_stats(p);
    if (code != 0)
        return fin_saisie(code);
    printf("\n");

    //choix de la classe, puis ses modificateurs sur les statistiques de base
    code = choisir_classe(&p->classe);
    if (code != 0)
        return fin_saisie(code);
    appliquer_classe(p);
    printf("\n  " VERT "✔ %s a été créé !" RESET "\n", p->nom);
    afficher_personnage(p);
    return 0;
}
