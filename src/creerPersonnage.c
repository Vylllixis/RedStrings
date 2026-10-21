/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include "my.h"

static void afficherQuestion(const char *question, int min, int max)
{
    printf("  " GRAS "%s" RESET GRIS " (%d-%d)" RESET CYAN " › " RESET,
        question, min, max);
}

int lireLigne(char *buf, size_t taille)
{
    size_t len = 0;

    if (fgets(buf, (int)taille, stdin) == NULL)
        return -1;
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[len - 1] = '\0';         // on retire le '\n'
    else
        viderBuffer();               // ligne trop longue : on jette la fin
    return 0;
}

int lireEntier(const char *question, int min, int max, int *resultat)
{
    char buf[32];
    char *fin = NULL;
    long valeur = 0;

    while (1) {
        afficherQuestion(question, min, max);
        if (lireLigne(buf, sizeof(buf)) == -1)
            return -1;
        errno = 0;
        valeur = strtol(buf, &fin, 10);
        if (fin != buf && *fin == '\0' && errno == 0
            && valeur >= min && valeur <= max) {
            *resultat = (int)valeur;
            return 0;
        }
        afficherErreur("Valeur invalide");
    }
}

int creerPersonnage(personnage_t *p, personnage_t *tab, int len)
{
    //en-tête
    printf("\n");
    boiteHaut(CYAN);
    boiteLigne(CYAN, "  " GRAS "✦ Création d'un personnage" RESET);
    boiteBas(CYAN);
    printf("\n");

    //choix du nom
    printf("  " GRAS "Nom" RESET CYAN " › " RESET);
    if (scanf("%29s", p->name) != 1)
        return -1;
    viderBuffer();
    if (findPersonage(tab, len, p->name) != -1) {
        afficherErreur("Ce nom est déjà pris");
        return -1;
    }
    printf("\n");

    //choix des statistiques
    printf("  " GRIS "── Statistiques ──" RESET "\n");
    lireEntier("Points de vie", 0, 99, p->pv);
    lireEntier("Attaque      ", 0, 99, p->atk);
    lireEntier("Défense      ", 0, 99, p->def);
    printf("\n");

    //choix de la classe (1-3 saisi, l'enum commence à 0)
    printf("  " GRIS "── Classe ──" RESET "\n");
    printf("    " GRAS "1" RESET "  " ROUGE "Guerrier" RESET "\n");
    printf("    " GRAS "2" RESET "  " MAGENTA "Mage" RESET "\n");
    printf("    " GRAS "3" RESET "  " VERT "Archer" RESET "\n");
    lireEntier("Choix", 1, 3, p->classe) - 1;

    printf("\n  " VERT "✔ %s a été créé !" RESET "\n", p->name);
    return 0;
}
