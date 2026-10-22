/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Dimensions de la fenêtre, limites de l'interface et palette de couleurs.
 */

#ifndef GUI_CONFIG_H
    #define GUI_CONFIG_H

    #include <SFML/Graphics.h>

    #define LARGEUR 1280
    #define HAUTEUR 720
    /* Le roster est illimité, mais l'écran n'a de place que pour 10 cartes. */
    #define MAX_PERSONNAGES 10
    /* Cartes de personnage des grilles (liste, composition des équipes). */
    #define CARTE_L 216.f
    #define CARTE_H 236.f
    #define MAX_PARTICULES 900
    #define MAX_TOASTS 4
    #define NB_POUSSIERES 70
    #define NB_ETOILES 160
    #define PI 3.14159265f

/* Palette */
    #define C_FOND          ((sfColor){9, 11, 24, 255})
    #define C_SURFACE       ((sfColor){22, 26, 46, 255})
    #define C_SURFACE_HAUTE ((sfColor){33, 39, 68, 255})
    #define C_CHAMP         ((sfColor){14, 17, 33, 240})
    #define C_BORD          ((sfColor){255, 255, 255, 22})
    #define C_TEXTE         ((sfColor){236, 240, 255, 255})
    #define C_TEXTE_DOUX    ((sfColor){150, 160, 192, 255})
    #define C_TEXTE_FAIBLE  ((sfColor){92, 102, 136, 255})
    #define C_BLANC         ((sfColor){255, 255, 255, 255})
    #define C_NOIR          ((sfColor){0, 0, 0, 255})
    #define C_TRANSPARENT   ((sfColor){0, 0, 0, 0})
    #define C_CYAN          ((sfColor){56, 189, 248, 255})
    #define C_ROUGE         ((sfColor){244, 63, 94, 255})
    #define C_VERT          ((sfColor){52, 211, 153, 255})
    #define C_BLEU          ((sfColor){96, 165, 250, 255})
    #define C_OR            ((sfColor){251, 191, 36, 255})
    #define C_VIOLET        ((sfColor){217, 70, 239, 255})
    #define C_SOIN          ((sfColor){103, 232, 249, 255})

#endif /* GUI_CONFIG_H */
