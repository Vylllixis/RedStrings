/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include "my.h"

void attaquer(personnage_t *attaquant, personnage_t *cible)
{
    // [calcul_degats]
    int degats = attaquant->atk * (100 - cible->def) / 100;

    if (degats < 0)
        degats = 0;
    cible->pv -= degats;
    if (cible->pv < 0)
        cible->pv = 0;
    // [calcul_degats]

    printf("\n");
    boiteHaut(ROUGE);
    boiteLigne(ROUGE, "  " GRAS "⚔ %s attaque %s" RESET, attaquant->name, cible->name);
    boiteSeparateur(ROUGE);
    boiteLigne(ROUGE, "  Attaque      %d", attaquant->atk);
    boiteLigne(ROUGE, "  Réduction    " BLEU "-%d%%" RESET, cible->def);
    boiteLigne(ROUGE, "  Dégâts       " GRAS ROUGE "%d" RESET, degats);
    boiteLigne(ROUGE, "  PV restants  " VERT "%d" RESET, cible->pv);
    if (cible->pv == 0)
        boiteLigne(ROUGE, "  " GRAS ROUGE "☠ %s est mort" RESET, cible->name);
    boiteBas(ROUGE);
    printf("\n");
}
