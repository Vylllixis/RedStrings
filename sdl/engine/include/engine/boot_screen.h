#ifndef ENG_BOOT_SCREEN_H
#define ENG_BOOT_SCREEN_H

/* Écran d'attente LOGICIEL (retour de Théo, 2026-11-22 : « encore environ 5 secondes de vide au
 * lancement, où il y a mon fond d'écran au lieu du chargement »).
 *
 * Entre l'ouverture de la fenêtre et la première image du GPU, il s'écoule le temps que le pilote
 * de la carte graphique démarre : 2,7 s mesurées à froid sur la machine de Théo (la carte dédiée
 * d'un portable dort tant que rien ne s'en sert), 0,2 s à chaud. Pendant ce temps la fenêtre
 * n'était pas dessinée : elle montrait ce qu'elle recouvre.
 *
 * Ce module dessine, avec le processeur, la CORDE DE CHARGEMENT de l'écran de chargement
 * (shaders/splash.frag.hlsl : les deux chevilles, la corde qui vibre et se remplit, le nœud qui
 * avance, la légende) sur fond noir, avec les mêmes formules que le shader : quand le GPU prend le
 * relais (la scène apparaît alors en fondu derrière la corde), rien ne saute. Le lanceur vérifie
 * cette égalité image contre image (--splash-shots).
 *
 * Repère du shader : p.x vers la droite, p.y vers le HAUT, hauteur de l'écran = 1 (p.y de -0,5 à
 * 0,5), centre de l'écran en (0, 0). Module pur : ni SDL, ni GPU, ni allocation. Tests :
 * tests/test_boot_screen.c. */

#include <stddef.h>
#include <stdint.h>

/* La corde apparaît en fondu depuis le noir, en ENG_BOOT_FADE_S secondes (comme dans le shader). */
#define ENG_BOOT_FADE_S 0.3F
/* Part de la barre de progression que l'attente du pilote peut remplir : le chargement du jeu
 * (écran de chargement) reprend la corde là où l'attente l'a laissée. */
#define ENG_BOOT_PROGRESS_END 0.1F

/* La légende sous la corde : un champ de distance à un canal (engine/text_sdf.h). `pixels` est
 * l'image ENTIÈRE envoyée au shader (le titre y est aussi) ; `rect` y désigne la légende. */
typedef struct EngBootText {
    const uint8_t *pixels;
    uint32_t width; /* taille de l'image, en pixels */
    uint32_t height;
    float rect[4]; /* uv de la légende dans l'image (u0, v0, u1, v1) */
    float aspect;  /* largeur / hauteur de la légende */
} EngBootText;

typedef struct EngBootFrame {
    uint32_t width; /* taille de l'écran, en pixels */
    uint32_t height;
    float time_s;             /* temps écoulé depuis la première image de l'attente */
    float progress;           /* progression affichée, de 0 à 1 */
    const EngBootText *label; /* NULL : pas de légende */
} EngBootFrame;

typedef struct EngBootRect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} EngBootRect;

/* Zone de l'écran que l'attente peut éclairer (corde, halo, nœud, chevilles, légende), quels que
 * soient le temps et la progression ; tout le reste est noir. Vide si l'écran est vide. */
EngBootRect eng_boot_bounds(uint32_t width, uint32_t height, const EngBootText *label);

/* Dessine la zone eng_boot_bounds de l'image `frame` dans `pixels` : `pixels` désigne le coin haut
 * gauche de CETTE ZONE (pas de l'écran), en XRGB8888 (0x00RRVVBB), `pitch` octets par ligne (un
 * multiple de 4, au moins la largeur de la zone). Sans effet si un paramètre est nul ou si le pas
 * de ligne ne convient pas. */
void eng_boot_draw(const EngBootFrame *frame, uint32_t *pixels, size_t pitch);

/* Avance la progression affichée pendant `dt` secondes d'attente : elle monte vers
 * ENG_BOOT_PROGRESS_END sans jamais l'atteindre (on ne sait pas combien de temps le pilote mettra).
 * Renvoie la nouvelle valeur ; une valeur ou une durée absurde ne la fait pas reculer. */
float eng_boot_progress(float progress, float dt);

#endif
