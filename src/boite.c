/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdarg.h>
#include "my.h"

#define LARGEUR_BOITE 36

static int largeurVisible(const char *texte)
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

void boiteHaut(const char *couleur)
{
    printf("%s╭" LIGNE "╮" RESET "\n", couleur);
}

void boiteSeparateur(const char *couleur)
{
    printf("%s├" LIGNE "┤" RESET "\n", couleur);
}

void boiteBas(const char *couleur)
{
    printf("%s╰" LIGNE "╯" RESET "\n", couleur);
}

void boiteLigne(const char *couleur, const char *format, ...)
{
    char texte[512];
    va_list args;

    va_start(args, format);
    vsnprintf(texte, sizeof(texte), format, args);
    va_end(args);
    printf("%s│" RESET "%s", couleur, texte);
    for (int i = largeurVisible(texte); i < LARGEUR_BOITE; i++)
        printf(" ");
    printf("%s│" RESET "\n", couleur);
}
