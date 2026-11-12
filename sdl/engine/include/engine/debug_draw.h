#pragma once

#include <stdint.h>

/* PROVISOIRE (jusqu'à M1) : ce que le jeu demande de dessiner. Remplacé par le vrai rendu. */
typedef struct EngDebugSquare {
    double x;    /* 0 = bord gauche de la fenêtre, 1 = bord droit */
    double y;    /* 0 = bord haut de la fenêtre, 1 = bord bas */
    double size; /* côté, en fraction de la hauteur de la fenêtre */
    uint8_t r;
    uint8_t g;
    uint8_t b;
} EngDebugSquare;
