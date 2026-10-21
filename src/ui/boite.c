/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdarg.h>
#include <stdio.h>
#include "boite.h"
#include "couleurs.h"
#include "my_string.h"

int largeur_visible(const char *texte)
{
    int largeur = 0;

    // [largeur_visible]
    for (int i = 0; texte[i] != '\0'; i++) {
        if (texte[i] == '\033') {
            while (texte[i] != '\0' && texte[i] != 'm')
                i++;
            if (texte[i] == '\0')
                break;
        } else if ((texte[i] & 0xC0) != 0x80) {
            largeur++;
        }
    }
    // [largeur_visible]
    return largeur;
}

void boite_haut(const char *couleur)
{
    printf("%s╭" LIGNE "╮" RESET "\n", couleur);
}

void boite_separateur(const char *couleur)
{
    printf("%s├" LIGNE "┤" RESET "\n", couleur);
}

void boite_bas(const char *couleur)
{
    printf("%s╰" LIGNE "╯" RESET "\n", couleur);
}

void boite_ligne(const char *couleur, const char *format, ...)
{
    char texte[512];
    va_list args;

    va_start(args, format);
    vsnprintf(texte, sizeof(texte), format, args);
    va_end(args);
    printf("%s│" RESET "%s", couleur, texte);
    for (int i = largeur_visible(texte); i < LARGEUR_BOITE; i++)
        printf(" ");
    printf("%s│" RESET "\n", couleur);
}

void aligner_texte(char *dst, size_t taille, const char *texte, int largeur)
{
    int visible = largeur_visible(texte);
    int pad = visible < largeur ? largeur - visible : 0;

    snprintf(dst, taille, "%s%*s", texte, pad, "");
}

void boite_champ(const char *couleur, const char *libelle,
    const char *format, ...)
{
    char libelle_aligne[64];
    char valeur[256];
    va_list args;

    aligner_texte(libelle_aligne, sizeof(libelle_aligne), libelle,
        LARGEUR_LIBELLE);
    va_start(args, format);
    vsnprintf(valeur, sizeof(valeur), format, args);
    va_end(args);
    boite_ligne(couleur, "  %s%s", libelle_aligne, valeur);
}
