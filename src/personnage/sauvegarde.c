/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdio.h>
#include "my_string.h"
#include "sauvegarde.h"

static int stat_valide(int valeur)
{
    return valeur >= STAT_MIN && valeur <= STAT_MAX;
}

static int personnage_valide(const personnage_t *p, int classe,
    const roster_t *deja_lus)
{
    if (!stat_valide(p->pv) || !stat_valide(p->atq) || !stat_valide(p->def)
        || !stat_valide(p->soin))
        return 0;
    if (!classe_valide(classe))
        return 0;
    return trouver_personnage(deja_lus->data, deja_lus->len, p->nom) == -1;
}

sauvegarde_t sauvegarder(const roster_t *r, const char *chemin)
{
    FILE *f = fopen(chemin, "w");
    int erreur = 0;

    if (f == NULL)
        return SAUVEGARDE_ERR_OUVERTURE;
    for (int i = 0; i < r->len && !erreur; i++) {
        erreur = fprintf(f, "%s %d %d %d %d %d\n", r->data[i].nom, r->data[i].pv,
            r->data[i].atq, r->data[i].def, (int)r->data[i].classe,
            r->data[i].soin) < 0;
    }
    if (fclose(f) == EOF)
        erreur = 1;
    return erreur ? SAUVEGARDE_ERR_ECRITURE : SAUVEGARDE_OK;
}

#define LIGNE_MAX 128

static int ligne_vide(const char *s)
{
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] != ' ' && s[i] != '\t' && s[i] != '\n' && s[i] != '\r')
            return 0;
    }
    return 1;
}

/**
 * Une ligne : « nom pv atq def classe soin ». Une ligne sans le soin vient
 * d'une sauvegarde plus ancienne : le soin vaut alors SOIN_MIGRATION, modifié
 * par la classe. Renvoie 0 si la ligne est valide, -1 sinon.
 */
static int lire_ligne_perso(const char *ligne, personnage_t *p)
{
    int classe = 0;
    int fin = 0;
    int fin_soin = 0;
    int champs = sscanf(ligne, "%" STR(NOM_LONGUEUR) "s %d %d %d %d %n", p->nom,
        &p->pv, &p->atq, &p->def, &classe, &fin);
    const char *reste = ligne + fin;

    if (champs != 5 || !classe_valide(classe))
        return -1;
    p->classe = (classe_t)classe;
    if (ligne_vide(reste)) {
        p->soin = stat_finale(p->classe, STAT_SOIN, SOIN_MIGRATION);
        return 0;
    }
    if (sscanf(reste, "%d %n", &p->soin, &fin_soin) != 1)
        return -1;
    return ligne_vide(reste + fin_soin) ? 0 : -1;
}

/* Lit tout le fichier dans lus ; renvoie SAUVEGARDE_OK si chaque ligne est
 * valide. */
static sauvegarde_t lire_fichier(FILE *f, roster_t *lus)
{
    char ligne[LIGNE_MAX];
    personnage_t p = {0};
    int longueur = 0;

    while (fgets(ligne, sizeof(ligne), f) != NULL) {
        longueur = my_strlen(ligne);
        if (longueur == LIGNE_MAX - 1 && ligne[longueur - 1] != '\n')
            return SAUVEGARDE_ERR_FORMAT;       // ligne trop longue
        if (ligne_vide(ligne))
            continue;
        p = (personnage_t){0};
        if (lire_ligne_perso(ligne, &p) == -1
            || !personnage_valide(&p, (int)p.classe, lus))
            return SAUVEGARDE_ERR_FORMAT;
        if (roster_ajouter(lus, &p) == -1)
            return SAUVEGARDE_ERR_MEMOIRE;
    }
    return ferror(f) ? SAUVEGARDE_ERR_FORMAT : SAUVEGARDE_OK;
}

sauvegarde_t charger(roster_t *r, const char *chemin)
{
    FILE *f = fopen(chemin, "r");
    roster_t lus;
    sauvegarde_t code = SAUVEGARDE_OK;

    if (f == NULL)
        return SAUVEGARDE_ERR_OUVERTURE;
    roster_init(&lus);
    code = lire_fichier(f, &lus);
    fclose(f);
    if (code != SAUVEGARDE_OK) {
        roster_liberer(&lus);
        return code;
    }
    roster_liberer(r);
    *r = lus;
    return SAUVEGARDE_OK;
}

const char *sauvegarde_message(sauvegarde_t code)
{
    static const char *messages[] = {
        [SAUVEGARDE_OK] = "Terminé",
        [SAUVEGARDE_ERR_OUVERTURE] = "Impossible d'ouvrir le fichier",
        [SAUVEGARDE_ERR_ECRITURE] = "Erreur pendant l'écriture",
        [SAUVEGARDE_ERR_FORMAT] = "Fichier corrompu : chargement annulé",
        [SAUVEGARDE_ERR_MEMOIRE] = "Mémoire insuffisante",
    };

    return messages[code];
}
