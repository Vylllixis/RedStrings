/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdio.h>
#include "boite.h"
#include "couleurs.h"
#include "personnage.h"

void attaquer(personnage_t *attaquant, personnage_t *cible)
{
    int degats = subir_attaque(attaquant, cible);

    printf("\n");
    boite_haut(ROUGE);
    boite_ligne(ROUGE, "  " GRAS "⚔ %s attaque %s" RESET, attaquant->nom, cible->nom);
    boite_separateur(ROUGE);
    boite_champ(ROUGE, "Attaque", "%d", attaquant->atq);
    boite_champ(ROUGE, "Réduction", BLEU "-%d%%" RESET, cible->def);
    boite_champ(ROUGE, "Dégâts", GRAS ROUGE "%d" RESET, degats);
    boite_champ(ROUGE, "PV restants", VERT "%d" RESET, cible->pv);
    if (est_ko(cible))
        boite_ligne(ROUGE, "  " GRAS ROUGE "☠ %s est mort" RESET, cible->nom);
    boite_bas(ROUGE);
    printf("\n");
}
