/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ audio_manifeste.c ──────────────────────────────────────────────────
** │ But         Lire le manifeste d'un pack de sons (sons.txt) : quel
** │             fichier jouer pour chaque événement, à quel volume, avec
** │             quelles variantes.
** │ Format      une ligne par événement :
** │             « evenement [volume=0.8] [hauteur=0.05]
** │             [rare=fichier.ogg@0.05] fichier.ogg [fichier2.ogg…] »
** │ Contenu     pack_vider, pack_lire_ligne, pack_charger, pack_chemin
** │ Règle       aucun SFML ici ; une ligne fausse est ignorée et signalée,
** │             le reste du pack reste utilisable
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui_sons.h"

#define LIGNE_MAX 512
#define MOTS_MAX 12

void pack_vider(pack_t *p, const char *dossier)
{
    if (p == NULL)
        return;
    memset(p, 0, sizeof(*p));
    snprintf(p->dossier, sizeof(p->dossier), "%s", dossier != NULL ? dossier : "");
    for (int i = 0; i < NB_SONS; i++) {
        p->sons[i].volume = 1.f;
        p->sons[i].hauteur = -1.f;
    }
    for (int i = 0; i < NB_MUSIQUES; i++)
        p->musiques[i].volume = 1.f;
}

/* Découpe `ligne` en mots (séparés par des espaces), sans la partie après '#'. */
static int decouper(char *ligne, char *mots[MOTS_MAX])
{
    int n = 0;
    char *c = strchr(ligne, '#');

    if (c != NULL)
        *c = '\0';
    c = ligne;
    while (*c != '\0' && n < MOTS_MAX) {
        while (*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n')
            *c++ = '\0';
        if (*c == '\0')
            break;
        mots[n++] = c;
        while (*c != '\0' && *c != ' ' && *c != '\t' && *c != '\r' && *c != '\n')
            c++;
    }
    while (*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n')
        c++;
    return *c != '\0' ? -1 : n;
}

/* Un nom de fichier sûr : lettres, chiffres, '_', '-', '.', jamais de dossier. */
static int fichier_valide(const char *f)
{
    size_t n = strlen(f);

    if (n < 5 || n >= SON_FICHIER_MAX || strstr(f, "..") != NULL)
        return 0;
    for (size_t i = 0; i < n; i++) {
        if (!((f[i] >= 'a' && f[i] <= 'z') || (f[i] >= 'A' && f[i] <= 'Z')
            || (f[i] >= '0' && f[i] <= '9') || f[i] == '_' || f[i] == '-'
            || f[i] == '.'))
            return 0;
    }
    return strcmp(f + n - 4, ".ogg") == 0 || strcmp(f + n - 4, ".wav") == 0
        || (n > 5 && strcmp(f + n - 5, ".flac") == 0);
}

/* Nombre réel compris entre min et max ; -1 si le texte n'en est pas un. */
static int lire_reel(const char *s, float min, float max, float *v)
{
    char *fin = NULL;
    float x = strtof(s, &fin);

    if (fin == s || *fin != '\0' || !(x >= min && x <= max))
        return -1;
    *v = x;
    return 0;
}

static int erreur(char *msg, size_t taille, const char *texte, const char *mot)
{
    if (msg != NULL && taille > 0)
        snprintf(msg, taille, "%s « %s »", texte, mot != NULL ? mot : "");
    return -1;
}

/* « rare=fichier.ogg@0.05 » */
static int lire_rare(entree_son_t *e, const char *valeur, char *msg, size_t taille)
{
    const char *at = strchr(valeur, '@');
    char fichier[SON_FICHIER_MAX];
    size_t n = at != NULL ? (size_t)(at - valeur) : 0;

    if (at == NULL || n >= sizeof(fichier))
        return erreur(msg, taille, "variante rare mal écrite (fichier@proba)", valeur);
    memcpy(fichier, valeur, n);
    fichier[n] = '\0';
    if (!fichier_valide(fichier))
        return erreur(msg, taille, "nom de fichier refusé", fichier);
    if (lire_reel(at + 1, 0.f, 1.f, &e->proba_rare) == -1)
        return erreur(msg, taille, "probabilité hors de 0..1", at + 1);
    snprintf(e->rare, sizeof(e->rare), "%s", fichier);
    return 0;
}

static int lire_option(entree_son_t *e, const char *mot, char *msg, size_t taille)
{
    if (strncmp(mot, "volume=", 7) == 0)
        return lire_reel(mot + 7, 0.f, 1.f, &e->volume) == -1
            ? erreur(msg, taille, "volume hors de 0..1", mot) : 0;
    if (strncmp(mot, "hauteur=", 8) == 0)
        return lire_reel(mot + 8, 0.f, 0.5f, &e->hauteur) == -1
            ? erreur(msg, taille, "hauteur hors de 0..0.5", mot) : 0;
    if (strncmp(mot, "rare=", 5) == 0)
        return lire_rare(e, mot + 5, msg, taille);
    return erreur(msg, taille, "option inconnue", mot);
}

/* Lit les options et les fichiers d'une ligne dans `e` (une copie, validée d'un bloc). */
static int lire_entree(entree_son_t *e, char **mots, int n, char *msg, size_t taille)
{
    for (int i = 0; i < n; i++) {
        if (strchr(mots[i], '=') != NULL) {
            if (lire_option(e, mots[i], msg, taille) == -1)
                return -1;
            continue;
        }
        if (!fichier_valide(mots[i]))
            return erreur(msg, taille, "nom de fichier refusé", mots[i]);
        if (e->nb >= SON_VARIANTES_MAX)
            return erreur(msg, taille, "trop de variantes (4 au plus) à partir de", mots[i]);
        snprintf(e->fichiers[e->nb++], SON_FICHIER_MAX, "%s", mots[i]);
    }
    return e->nb > 0 ? 0 : erreur(msg, taille, "aucun fichier pour", "l'événement");
}

static int lire_musique(pack_t *p, int m, char **mots, int n, char *msg, size_t taille)
{
    entree_son_t e = {0};

    e.volume = 1.f;
    if (lire_entree(&e, mots, n, msg, taille) == -1)
        return -1;
    if (e.nb != 1 || e.rare[0] != '\0')
        return erreur(msg, taille, "une musique prend un seul fichier", nom_musique(m));
    snprintf(p->musiques[m].fichier, SON_FICHIER_MAX, "%s", e.fichiers[0]);
    p->musiques[m].volume = e.volume;
    return 0;
}

/*
** ╭─ pack_lire_ligne ────────────────────────────────────────────────────
** │ Lit une ligne du manifeste dans le pack. Une ligne vide ou un
** │ commentaire (#) ne change rien.
** │
** │ 1. découpe la ligne en mots (copie bornée à LIGNE_MAX)
** │ 2. le premier mot doit être un événement ou une musique connus
** │ 3. les mots « nom=valeur » sont des options, les autres des fichiers
** │ 4. l'entrée n'est enregistrée que si TOUTE la ligne est valide
** │ Renvoie 0 si la ligne est acceptée, -1 sinon (message dans `msg`).
** ╰──────────────────────────────────────────────────────────────────────
*/
int pack_lire_ligne(pack_t *p, const char *ligne, char *msg, size_t taille)
{
    char copie[LIGNE_MAX];
    char *mots[MOTS_MAX];
    entree_son_t e = {0};
    int n = 0;
    int id = 0;

    if (p == NULL || ligne == NULL)
        return erreur(msg, taille, "ligne absente", "");
    if (strlen(ligne) >= sizeof(copie))
        return erreur(msg, taille, "ligne trop longue", "…");
    snprintf(copie, sizeof(copie), "%s", ligne);
    n = decouper(copie, mots);
    if (n <= 0)
        return n == 0 ? 0 : erreur(msg, taille, "trop de mots sur la ligne", ligne);
    if (musique_depuis_nom(mots[0]) != -1)
        return lire_musique(p, musique_depuis_nom(mots[0]), mots + 1, n - 1, msg, taille);
    id = son_depuis_nom(mots[0]);
    if (id == -1)
        return erreur(msg, taille, "événement inconnu", mots[0]);
    if (p->sons[id].nb > 0)
        return erreur(msg, taille, "événement défini deux fois", mots[0]);
    e.volume = 1.f;
    e.hauteur = -1.f;
    if (lire_entree(&e, mots + 1, n - 1, msg, taille) == -1)
        return -1;
    p->sons[id] = e;
    return 0;
}

/*
** ╭─ pack_charger ───────────────────────────────────────────────────────
** │ Charge dossier/manifeste dans `p`.
** │
** │ Renvoie le nombre de lignes refusées (0 si tout va bien), ou -1 si le
** │ fichier ne s'ouvre pas. Le premier problème est décrit dans `msg`
** │ avec son numéro de ligne ; les autres lignes sont quand même lues.
** ╰──────────────────────────────────────────────────────────────────────
*/
int pack_charger(pack_t *p, const char *dossier, const char *manifeste, char *msg,
    size_t taille)
{
    char chemin[SON_CHEMIN_MAX];
    char ligne[LIGNE_MAX];
    char detail[160];
    FILE *f = NULL;
    int num = 0;
    int refus = 0;

    if (p == NULL || dossier == NULL || manifeste == NULL)
        return -1;
    pack_vider(p, dossier);
    if (snprintf(chemin, sizeof(chemin), "%s/%s", dossier, manifeste)
        >= (int)sizeof(chemin) || (f = fopen(chemin, "r")) == NULL) {
        if (msg != NULL && taille > 0)
            snprintf(msg, taille, "manifeste introuvable : %s", chemin);
        return -1;
    }
    while (fgets(ligne, sizeof(ligne), f) != NULL) {
        num++;
        if (strchr(ligne, '\n') == NULL && !feof(f)) {
            snprintf(detail, sizeof(detail), "ligne trop longue");
            while (fgets(ligne, sizeof(ligne), f) != NULL && strchr(ligne, '\n') == NULL)
                ;
        } else if (pack_lire_ligne(p, ligne, detail, sizeof(detail)) == 0) {
            continue;
        }
        if (refus++ == 0 && msg != NULL && taille > 0)
            snprintf(msg, taille, "%s, ligne %d : %s", chemin, num, detail);
    }
    fclose(f);
    return refus;
}

int pack_chemin(const pack_t *p, const char *fichier, char *chemin, size_t taille)
{
    int n = 0;

    if (p == NULL || fichier == NULL || chemin == NULL || taille == 0)
        return -1;
    n = snprintf(chemin, taille, "%s/%s", p->dossier, fichier);
    return n < 0 || (size_t)n >= taille ? -1 : 0;
}
