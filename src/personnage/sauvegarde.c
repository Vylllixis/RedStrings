/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ sauvegarde.c ───────────────────────────────────────────────────────
** │ But         Écrire et relire la liste des personnages dans un fichier
** │             texte, une ligne par personnage : « nom pv atq def classe
** │             soin ».
** │ Contenu     sauvegarder, charger, fusionner, lire_fichier,
** │             lire_ligne_perso, personnage_valide, sauvegarde_message
** │ Utilise     roster.c ; le fichier est lu avec fgets, jamais avec un
** │             tampon sans limite
** ╰──────────────────────────────────────────────────────────────────────
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
    if (!classe_valide(classe) || nom_est_un_id(p->nom))
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

/*
** ╭─ lire_ligne_perso ───────────────────────────────────────────────────
** │ Lit une ligne « nom pv atq def classe soin ».
** │
** │ - une ligne sans le soin vient d'une ancienne sauvegarde : le soin
** │   vaut alors SOIN_MIGRATION, modifié par la classe
** │ - refusée si le nombre de champs est faux, si la classe n'existe pas,
** │   ou s'il reste du texte à la fin
** │ Renvoie 0 si la ligne est valide, -1 sinon.
** ╰──────────────────────────────────────────────────────────────────────
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

/*
** ╭─ charger ────────────────────────────────────────────────────────────
** │ Charge un fichier en « tout ou rien ».
** │
** │ 1. ouvre le fichier
** │ 2. lit tout dans une liste temporaire (lus)
** │ 3. si une ligne est invalide : libère lus et ne touche PAS à la liste
** │    du joueur
** │ 4. sinon remplace la liste du joueur par lus
** │ Un fichier corrompu ne peut donc jamais abîmer les personnages en
** │ cours.
** ╰──────────────────────────────────────────────────────────────────────
*/
/*
** ╭─ fusionner ──────────────────────────────────────────────────────────
** │ Ajoute les personnages du fichier à la liste, sans rien écraser.
** │
** │ 1. lit tout le fichier dans une liste temporaire (lus), comme charger()
** │ 2. fichier invalide : libère lus, la liste du joueur n'est pas touchée
** │ 3. pour chaque personnage de lus : ignoré si son nom existe déjà dans
** │    la liste (celui du joueur est gardé), ajouté sinon
** │ 4. si la mémoire manque en cours de route, la liste revient à sa
** │    longueur de départ : tout ou rien
** ╰──────────────────────────────────────────────────────────────────────
*/
sauvegarde_t fusionner(roster_t *r, const char *chemin, int *ajoutes, int *ignores)
{
    FILE *f = fopen(chemin, "r");
    roster_t lus;
    sauvegarde_t code = SAUVEGARDE_OK;
    int len_depart = r->len;

    *ajoutes = 0;
    *ignores = 0;
    if (f == NULL)
        return SAUVEGARDE_ERR_OUVERTURE;
    roster_init(&lus);
    code = lire_fichier(f, &lus);
    fclose(f);
    for (int i = 0; code == SAUVEGARDE_OK && i < lus.len; i++) {
        if (trouver_personnage(r->data, r->len, lus.data[i].nom) != -1)
            ++*ignores;
        else if (roster_ajouter(r, &lus.data[i]) == 0)
            ++*ajoutes;
        else
            code = SAUVEGARDE_ERR_MEMOIRE;
    }
    if (code != SAUVEGARDE_OK) {
        r->len = len_depart;
        *ajoutes = 0;
        *ignores = 0;
    }
    roster_liberer(&lus);
    return code;
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
