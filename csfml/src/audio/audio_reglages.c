/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ audio_reglages.c ───────────────────────────────────────────────────
** │ But         Conserver les réglages audio entre deux parties, dans un
** │             petit fichier texte lisible (« general=80 »…).
** │ Contenu     reglages_defaut, reglages_lire, reglages_ecrire,
** │             nom_choix_theme
** │ Règle       un fichier absent ou abîmé ne bloque jamais le jeu : les
** │             valeurs illisibles gardent leur valeur par défaut
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui_sons.h"

#define LIGNE_REGLAGE 128

static const char *CHOIX[NB_CHOIX_THEMES] = {
    [CHOIX_THEME_AUTO] = "auto",
    [CHOIX_THEME_STANDARD] = "standard",
    [CHOIX_THEME_YUGIOH] = "yugioh",
};

void reglages_defaut(reglages_audio_t *r)
{
    r->general = 80;
    r->musique = 55;
    r->effets = 80;
    r->coupe = 0;
    r->theme = CHOIX_THEME_AUTO;
}

const char *nom_choix_theme(choix_theme_t c)
{
    return (int)c >= 0 && c < NB_CHOIX_THEMES ? CHOIX[c] : CHOIX[CHOIX_THEME_AUTO];
}

/* Entier de `texte`, ramené entre min et max ; -1 si ce n'est pas un nombre. */
static int lire_entier(const char *texte, int min, int max, int *v)
{
    char *fin = NULL;
    long x = strtol(texte, &fin, 10);

    while (*fin == ' ' || *fin == '\r' || *fin == '\n')
        fin++;
    if (fin == texte || *fin != '\0')
        return -1;
    *v = x < min ? min : (x > max ? max : (int)x);
    return 0;
}

static void lire_choix(reglages_audio_t *r, const char *valeur)
{
    for (int i = 0; i < NB_CHOIX_THEMES; i++) {
        if (strncmp(valeur, CHOIX[i], strlen(CHOIX[i])) == 0
            && (valeur[strlen(CHOIX[i])] == '\0' || valeur[strlen(CHOIX[i])] == '\n'
            || valeur[strlen(CHOIX[i])] == '\r'))
            r->theme = (choix_theme_t)i;
    }
}

static void lire_ligne(reglages_audio_t *r, char *ligne)
{
    char *egal = strchr(ligne, '=');
    int v = 0;

    if (ligne[0] == '#' || egal == NULL)
        return;
    *egal = '\0';
    if (strcmp(ligne, "general") == 0 && lire_entier(egal + 1, 0, VOLUME_MAX, &v) == 0)
        r->general = v;
    else if (strcmp(ligne, "musique") == 0 && lire_entier(egal + 1, 0, VOLUME_MAX, &v) == 0)
        r->musique = v;
    else if (strcmp(ligne, "effets") == 0 && lire_entier(egal + 1, 0, VOLUME_MAX, &v) == 0)
        r->effets = v;
    else if (strcmp(ligne, "coupe") == 0 && lire_entier(egal + 1, 0, 1, &v) == 0)
        r->coupe = v;
    else if (strcmp(ligne, "theme") == 0)
        lire_choix(r, egal + 1);
}

/* Remplace dans `r` les valeurs lues dans le fichier ; -1 s'il ne s'ouvre pas. */
int reglages_lire(reglages_audio_t *r, const char *chemin)
{
    FILE *f = fopen(chemin, "r");
    char ligne[LIGNE_REGLAGE];

    if (f == NULL)
        return -1;
    while (fgets(ligne, sizeof(ligne), f) != NULL)
        lire_ligne(r, ligne);
    fclose(f);
    return 0;
}

int reglages_ecrire(const reglages_audio_t *r, const char *chemin)
{
    FILE *f = fopen(chemin, "w");
    int erreur = 0;

    if (f == NULL)
        return -1;
    erreur = fprintf(f, "# Réglages audio de RedStrings (panneau F2 du jeu)\n"
        "general=%d\nmusique=%d\neffets=%d\ncoupe=%d\ntheme=%s\n", r->general,
        r->musique, r->effets, r->coupe ? 1 : 0, nom_choix_theme(r->theme)) < 0;
    if (fclose(f) == EOF)
        erreur = 1;
    return erreur ? -1 : 0;
}
