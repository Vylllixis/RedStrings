/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdio.h>
#include "sauvegarde.h"

static int stat_valide(int valeur)
{
    return valeur >= STAT_MIN && valeur <= STAT_MAX;
}

static int personnage_valide(const personnage_t *p, int classe,
    const roster_t *deja_lus)
{
    if (!stat_valide(p->pv) || !stat_valide(p->atq) || !stat_valide(p->def))
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
        erreur = fprintf(f, "%s %d %d %d %d\n", r->data[i].nom, r->data[i].pv,
            r->data[i].atq, r->data[i].def, (int)r->data[i].classe) < 0;
    }
    if (fclose(f) == EOF)
        erreur = 1;
    return erreur ? SAUVEGARDE_ERR_ECRITURE : SAUVEGARDE_OK;
}

/* Lit tout le fichier dans lus ; renvoie SAUVEGARDE_OK si chaque ligne est
 * valide. */
static sauvegarde_t lire_fichier(FILE *f, roster_t *lus)
{
    personnage_t p = {0};
    int classe = 0;
    int ret = 0;

    while ((ret = fscanf(f, "%" STR(NOM_LONGUEUR) "s %d %d %d %d", p.nom,
        &p.pv, &p.atq, &p.def, &classe)) == 5) {
        if (!personnage_valide(&p, classe, lus))
            return SAUVEGARDE_ERR_FORMAT;
        p.classe = (classe_t)classe;
        if (roster_ajouter(lus, &p) == -1)
            return SAUVEGARDE_ERR_MEMOIRE;
    }
    return ret == EOF && !ferror(f) ? SAUVEGARDE_OK : SAUVEGARDE_ERR_FORMAT;
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
