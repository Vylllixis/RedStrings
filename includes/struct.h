/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef STRUCT_H
    #define STRUCT_H

typedef enum Classe {
    GUERRIER,   /**< Affiché en rouge. */
    MAGE,       /**< Affiché en magenta. */
    ARCHER      /**< Affiché en vert. */
} Classe_t;

typedef struct {
    char name[30];
    int pv;
    int atk;
    int def;
    Classe_t classe;
} personnage_t;

#endif /* STRUCT_H */
