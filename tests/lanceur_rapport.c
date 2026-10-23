/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ lanceur_rapport.c ──────────────────────────────────────────────────
** │ But         L'affichage du lanceur de tests : une ligne par test
** │             (avec le fichier et la ligne où il est écrit), puis, pour
** │             un test raté, la vérification qui a échoué avec les
** │             valeurs obtenue et attendue, le code source autour, ou la
** │             cause d'un plantage.
** │ Contenu     rapport_ligne_test, rapport_echec, groupes, source
** ╰──────────────────────────────────────────────────────────────────────
*/

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "lanceur_interne.h"

#define ROUGE "\033[1;31m"
#define VERT "\033[1;32m"
#define JAUNE "\033[1;33m"
#define GRIS "\033[2m"
#define GRAS "\033[1m"
#define RESET "\033[0m"
#define BARRE "      ┃ "
#define LARGEUR_NOM 40
#define LARGEUR_LIEU 40
#define LARGEUR_SOURCE 110

const char *rapport_couleur(const char *code)
{
    return isatty(STDOUT_FILENO) ? code : "";
}

/* La ligne où `nom` est défini (« static void nom( » en début de ligne), sinon `repli`. */
int rapport_trouver_definition(const char *fichier, const char *nom, int repli)
{
    FILE *f = fopen(fichier, "r");
    char ligne[512];
    char motif[128];
    int numero = 0;

    if (f == NULL)
        return repli;
    snprintf(motif, sizeof(motif), " %s(", nom);
    while (fgets(ligne, sizeof(ligne), f) != NULL) {
        numero++;
        if ((strncmp(ligne, "static void", 11) == 0 || strncmp(ligne, "void", 4) == 0)
            && strstr(ligne, motif) != NULL) {
            fclose(f);
            return numero;
        }
        if (strncmp(ligne, "void", 4) == 0 && strstr(ligne, nom) != NULL && strstr(ligne, "(") != NULL) {
            fclose(f);
            return numero;
        }
    }
    fclose(f);
    return repli;
}

void rapport_entete_groupe(const char *fichier)
{
    printf("\n  %s▌ %s%s %s", rapport_couleur(JAUNE), fichier, rapport_couleur(RESET),
        rapport_couleur(GRIS));
    for (int i = (int)strlen(fichier); i < 60; i++)
        printf("─");
    printf("%s\n", rapport_couleur(RESET));
}

void rapport_pied_groupe(int reussis, int total)
{
    printf("  %s└ %d/%d réussi%s%s\n", rapport_couleur(GRIS), reussis, total,
        reussis > 1 ? "s" : "", rapport_couleur(RESET));
}

void rapport_ligne_test(const char *nom, const char *fichier, int ligne, int ok, int nb_verif,
    double ms)
{
    char lieu[TEXTE_LONG];
    const char *court = strncmp(nom, "test_", 5) == 0 ? nom + 5 : nom;

    snprintf(lieu, sizeof(lieu), "%s:%d", fichier, ligne);
    printf("   %s%s%s  %-*.*s %s%-*s %3d vérif  %6.1f ms%s\n", rapport_couleur(ok ? VERT : ROUGE),
        ok ? "✔" : "✘", rapport_couleur(RESET), LARGEUR_NOM, LARGEUR_NOM, court, rapport_couleur(GRIS),
        LARGEUR_LIEU, lieu, nb_verif, ms, rapport_couleur(RESET));
}

/* Quelques lignes du fichier autour de `ligne`, la ligne fautive marquée ▶. */
static void afficher_source(const char *fichier, int ligne, int avant, int apres)
{
    FILE *f = fopen(fichier, "r");
    char texte[512];
    int numero = 0;

    if (f == NULL)
        return;
    while (fgets(texte, sizeof(texte), f) != NULL) {
        numero++;
        if (numero < ligne - avant || numero > ligne + apres)
            continue;
        texte[strcspn(texte, "\n")] = '\0';
        printf("%s%s%4d │%s %s%.*s%s\n", BARRE, rapport_couleur(numero == ligne ? ROUGE : GRIS),
            numero, numero == ligne ? "▶" : " ", rapport_couleur(numero == ligne ? GRAS : GRIS),
            LARGEUR_SOURCE, texte, rapport_couleur(RESET));
    }
    fclose(f);
}

static void afficher_verification(const echec_verif_t *e, int numero, int total)
{
    printf("%s%séchec %d/%d%s  %s%s:%d%s  dans %s%s%s\n", BARRE, rapport_couleur(ROUGE), numero, total,
        rapport_couleur(RESET), rapport_couleur(GRAS), e->fichier, e->ligne, rapport_couleur(RESET),
        rapport_couleur(GRAS), e->fonction, rapport_couleur(RESET));
    printf("%s%svérifie  %s %s\n", BARRE, rapport_couleur(GRIS), rapport_couleur(RESET), e->expr);
    printf("%s%sobtenu   %s %s%s%s\n", BARRE, rapport_couleur(GRIS), rapport_couleur(RESET),
        rapport_couleur(ROUGE), e->obtenu, rapport_couleur(RESET));
    printf("%s%sattendu  %s %s%s%s\n", BARRE, rapport_couleur(GRIS), rapport_couleur(RESET),
        rapport_couleur(VERT), e->attendu, rapport_couleur(RESET));
    afficher_source(e->fichier, e->ligne, 2, 0);
}

/* « fonction  (chemin/fichier.c:42) » → chemin et ligne, 0 si illisible. */
static int lieu_de_frame(const char *frame, char *fichier, size_t taille, int *ligne)
{
    const char *debut = strrchr(frame, '(');

    if (debut == NULL)
        return 0;
    return sscanf(debut, "(%[^:]:%d)", fichier, ligne) == 2 && strlen(fichier) < taille;
}

static void afficher_mort(const rapport_t *r, const mort_t *mort)
{
    char fichier[256];
    int ligne = 0;
    int montre = 0;

    printf("%s%s%s%s\n", BARRE, rapport_couleur(ROUGE), mort->titre, rapport_couleur(RESET));
    printf("%s%scause probable%s : %s\n", BARRE, rapport_couleur(GRIS), rapport_couleur(RESET), mort->cause);
    for (int i = 0; i < mort->nb_detail; i++)
        printf("%s%s%s%s\n", BARRE, rapport_couleur(GRIS), mort->detail[i], rapport_couleur(RESET));
    for (int i = 0; i < mort->nb_frames; i++)
        printf("%s%s%s #%d%s %s\n", BARRE, rapport_couleur(GRIS), i == 0 ? "▶" : " ", i,
            rapport_couleur(RESET), mort->frames[i]);
    for (int i = 0; i < mort->nb_frames && !montre; i++) {
        if (lieu_de_frame(mort->frames[i], fichier, sizeof(fichier), &ligne)) {
            afficher_source(fichier, ligne, 2, 2);
            montre = 1;
        }
    }
    if (r->nb_verif > 0)
        printf("%s%sdernière vérification réussie%s : %s:%d (dans %s), %d au total\n", BARRE,
            rapport_couleur(GRIS), rapport_couleur(RESET), r->dernier_fichier, r->derniere_ligne,
            r->derniere_fonction, r->nb_verif);
}

void rapport_echec(const rapport_t *r, const mort_t *mort, const char *fichier, int ligne)
{
    int montres = r->nb_echecs < ECHECS_MAX ? r->nb_echecs : ECHECS_MAX;

    printf("%s%stest défini à %s:%d  ·  %d vérification%s, %d échec%s%s\n", BARRE,
        rapport_couleur(GRIS), fichier, ligne, r->nb_verif, r->nb_verif > 1 ? "s" : "",
        r->nb_echecs, r->nb_echecs > 1 ? "s" : "", rapport_couleur(RESET));
    for (int i = 0; i < montres; i++)
        afficher_verification(&r->echecs[i], i + 1, r->nb_echecs);
    if (r->nb_echecs > montres)
        printf("%s… et %d autre%s échec%s non détaillé%s\n", BARRE, r->nb_echecs - montres,
            r->nb_echecs - montres > 1 ? "s" : "", r->nb_echecs - montres > 1 ? "s" : "",
            r->nb_echecs - montres > 1 ? "s" : "");
    if (r->bloque)
        printf("%s%sle test s'est arrêté ici : la suite dépendait de cette vérification%s\n", BARRE,
            rapport_couleur(JAUNE), rapport_couleur(RESET));
    if (mort->titre[0] != '\0' && !r->bloque)
        afficher_mort(r, mort);
    printf("\n");
}
