/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Mode Yu-Gi-Oh! (easter egg, ./redstrings_csfml "Yu-Gi-Oh!") : chaque classe
 * est représentée par une carte de monstre. Rendu seulement : les classes,
 * leurs noms, leurs statistiques et le combat restent ceux du jeu.
 */

#ifndef GUI_YUGIOH_H
    #define GUI_YUGIOH_H

    #include <SFML/Graphics.h>
    #include "personnage.h"
    #include "gui_types.h"

    /* Taille d'une face, en pixels de texture (proportions d'une vraie carte : 59 × 86 mm). */
    #define FACE_L 360
    #define FACE_H 525

typedef enum {
    CADRE_NORMAL,
    CADRE_EFFET,
    CADRE_RITUEL,
    CADRE_FUSION,
    CADRE_SYNCHRO,
    CADRE_XYZ,
    NB_CADRES
} cadre_carte_t;

typedef enum {
    ATTR_TENEBRES,
    ATTR_LUMIERE,
    ATTR_TERRE,
    ATTR_EAU,
    ATTR_FEU,
    ATTR_VENT,
    NB_ATTRIBUTS
} attribut_carte_t;

/**
 * Carte qui représente une classe. `ressource` est le nom officiel de la
 * carte : il ne sert qu'à identifier l'illustration, jamais à l'affichage.
 */
typedef struct {
    const char *ressource;
    unsigned long code;         /**< Code de la carte = nom du fichier image. */
    cadre_carte_t cadre;
    attribut_carte_t attribut;
    int niveau;
} carte_classe_t;

/** Pose d'une carte à l'écran (tout est animable). */
typedef struct {
    sfVector2f centre;
    float largeur;              /**< Largeur posée à plat, échelle comprise. */
    float rot_x;                /**< Bascule avant/arrière (radians). */
    float rot_y;                /**< Rotation verticale : PI = on voit le dos. */
    float rot_z;                /**< Inclinaison dans le plan de l'écran. */
    float alpha;
    float reflet;               /**< 0..1 : reflet holographique. */
    float bord;                 /**< 0..1 : lumière qui court sur le cadre. */
    float ombre;                /**< 0..1 : ombre portée (carte soulevée). */
    float gris;                 /**< 0..1 : carte grisée (K.O., défaite). */
    sfColor halo;               /**< Halo derrière la carte ; alpha = intensité. */
} carte3d_t;

/* cartes.c : la correspondance classe → carte, et l'habillage des cadres */
const carte_classe_t *carte_de_classe(classe_t c);
sfColor couleur_cadre(cadre_carte_t c, int bas);
sfColor encre_cadre(cadre_carte_t c);
sfColor fond_titre(cadre_carte_t c);
const char *nom_cadre(cadre_carte_t c);
sfColor couleur_attribut(attribut_carte_t a);
const char *nom_attribut(attribut_carte_t a);
const char *symbole_attribut(attribut_carte_t a);

/* ressources.c : illustrations et faces, seulement en mode Yu-Gi-Oh! */
int yugioh_charger(app_t *app);
void yugioh_liberer(app_t *app);
int yugioh_actif(const app_t *app);

/* face.c : impression d'une face et du dos dans la cible courante */
void dessiner_face(app_t *app, classe_t c);
void dessiner_dos(app_t *app);

/* carte3d.c : rendu en perspective, avec reflets et lumière */
void carte3d(app_t *app, classe_t c, const carte3d_t *k);
sfVector2f carte3d_point(const carte3d_t *k, float u, float v);
int carte3d_face_visible(const carte3d_t *k);
void carte3d_titre(app_t *app, classe_t c, const carte3d_t *k);
carte3d_t carte3d_pose(sfVector2f centre, float largeur);

/* portrait.c : illustration dans un hexagone (jetons, avatars) */
int yugioh_portrait(app_t *app, classe_t c, sfVector2f centre, float rayon);

#endif /* GUI_YUGIOH_H */
