/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdio.h>
#include "boite.h"
#include "couleurs.h"
#include "personnage.h"

int calculer_degats(const personnage_t *attaquant, const personnage_t *cible)
{
    int degats = attaquant->atq * (100 - cible->def) / 100;

    return degats < 0 ? 0 : degats;
}

void attaquer(personnage_t *attaquant, personnage_t *cible)
{
    int degats = calculer_degats(attaquant, cible);
    cible->pv -= degats;

    printf("\n");
    boite_haut(ROUGE);
    boite_ligne(ROUGE, "  " GRAS "⚔ %s attaque %s" RESET, attaquant->nom, cible->nom);
    boite_separateur(ROUGE);
    boite_champ(ROUGE, "Attaque", "%d", attaquant->atq);
    boite_champ(ROUGE, "Réduction", BLEU "-%d%%" RESET, cible->def);
    boite_champ(ROUGE, "Dégâts", GRAS ROUGE "%d" RESET, degats);
    boite_champ(ROUGE, "PV restants", VERT "%d" RESET, cible->pv);
    if (cible->pv == 0)
        boite_ligne(ROUGE, "  " GRAS ROUGE "☠ %s est mort" RESET, cible->nom);
    boite_bas(ROUGE);
    printf("\n");
}
