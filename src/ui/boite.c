/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ boite.c ────────────────────────────────────────────────────────────
** │ But         Dessiner les cadres arrondis du terminal (╭─╮ │ ╰─╯)
** │             autour du texte du jeu.
** │ Contenu     largeur_visible, boite_haut / separateur / bas,
** │             boite_ligne, boite_champ, aligner_texte
** │ Utilisé par afficher_personnage.c, redstrings.c, bataille_console.c
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdarg.h>
#include <stdio.h>
#include "boite.h"
#include "couleurs.h"
#include "my_string.h"

/*
** ╭─ largeur_visible ────────────────────────────────────────────────────
** │ Compte les colonnes réellement visibles d'un texte à l'écran.
** │
** │ - ignore les codes couleur : de ESC (\033) jusqu'au 'm' inclus
** │ - ignore les octets de suite UTF-8 (de la forme 10xxxxxx) : un "é" ou
** │   un "█" compte pour 1 colonne, pas pour 2 ou 3 octets
** │ C'est ce qui permet d'aligner la bordure droite malgré les couleurs.
** ╰──────────────────────────────────────────────────────────────────────
*/
int largeur_visible(const char *texte)
{
    int largeur = 0;

    if (texte == NULL)
        return 0;
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
    couleur = couleur != NULL ? couleur : "";
    printf("%s╭" LIGNE "╮" RESET "\n", couleur);
}

void boite_separateur(const char *couleur)
{
    couleur = couleur != NULL ? couleur : "";
    printf("%s├" LIGNE "┤" RESET "\n", couleur);
}

void boite_bas(const char *couleur)
{
    couleur = couleur != NULL ? couleur : "";
    printf("%s╰" LIGNE "╯" RESET "\n", couleur);
}

/*
** ╭─ boite_ligne ────────────────────────────────────────────────────────
** │ Écrit une ligne de cadre, au format printf, avec la bordure droite
** │ alignée.
** │
** │ 1. vsnprintf() formate le texte dans un tampon borné (512)
** │ 2. bordure gauche, puis le texte
** │ 3. des espaces jusqu'à LARGEUR_BOITE, calculés avec largeur_visible()
** │ 4. bordure droite
** ╰──────────────────────────────────────────────────────────────────────
*/
void boite_ligne(const char *couleur, const char *format, ...)
{
    char texte[512] = "";
    va_list args;

    couleur = couleur != NULL ? couleur : "";
    if (format != NULL) {
        va_start(args, format);
        vsnprintf(texte, sizeof(texte), format, args);
        va_end(args);
    }
    printf("%s│" RESET "%s", couleur, texte);
    for (int i = largeur_visible(texte); i < LARGEUR_BOITE; i++)
        printf(" ");
    printf("%s│" RESET "\n", couleur);
}

void aligner_texte(char *dst, size_t taille, const char *texte, int largeur)
{
    int visible = 0;
    int pad = 0;

    if (dst == NULL || taille == 0)
        return;
    if (texte == NULL)
        texte = "";
    visible = largeur_visible(texte);
    pad = visible < largeur ? largeur - visible : 0;
    if (pad > LARGEUR_BOITE * 4)
        pad = LARGEUR_BOITE * 4;
    snprintf(dst, taille, "%s%*s", texte, pad, "");
}

void boite_champ(const char *couleur, const char *libelle,
    const char *format, ...)
{
    char libelle_aligne[64];
    char valeur[256] = "";
    va_list args;

    aligner_texte(libelle_aligne, sizeof(libelle_aligne), libelle,
        LARGEUR_LIBELLE);
    if (format != NULL) {
        va_start(args, format);
        vsnprintf(valeur, sizeof(valeur), format, args);
        va_end(args);
    }
    boite_ligne(couleur, "  %s%s", libelle_aligne, valeur);
}
