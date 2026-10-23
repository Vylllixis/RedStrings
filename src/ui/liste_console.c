/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ liste_console.c ────────────────────────────────────────────────────
** │ But         Montrer la liste des personnages avant d'en demander un :
** │             ID, nom, classe, PV et état (K.O., dans une équipe...).
** │ Contenu     groupe_de, afficher_liste
** │ Utilisé par menu_personnages.c (Afficher, Charger) et
** │             composition_console.c (choix des équipes)
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include "boite.h"
#include "couleurs.h"
#include "liste_console.h"
#include "my_string.h"

/* Premier groupe qui contient ce nom, ou NULL. */
static const groupe_liste_t *groupe_de(const groupe_liste_t *groupes, int nb,
    const char *nom)
{
    for (int g = 0; g < nb; g++) {
        for (int i = 0; i < groupes[g].n; i++) {
            if (my_strcmp(groupes[g].membres[i].nom, nom) == 0)
                return &groupes[g];
        }
    }
    return NULL;
}

/*
** ╭─ afficher_liste ─────────────────────────────────────────────────────
** │ Une ligne par personnage, avec son ID : sa position dans la liste, à
** │ partir de 1. C'est ce que roster_trouver() comprend à la place d'un nom.
** │
** │ Les colonnes sont alignées avec aligner_texte() (qui compte les
** │ colonnes visibles, pas les octets UTF-8) ; l'état vient en dernier,
** │ si bien qu'un très long nom décale seulement la fin de sa ligne.
** │ Comme un nom ne peut pas être un nombre (nom_est_un_id), un ID et un
** │ nom ne se confondent jamais.
** ╰──────────────────────────────────────────────────────────────────────
*/
void afficher_liste(const roster_t *r, const groupe_liste_t *groupes, int nb_groupes)
{
    const groupe_liste_t *g = NULL;
    char nom[64];
    char classe[64];

    printf("  " GRIS "── Personnages (%d) ──" RESET "\n", r->len);
    printf("  " GRIS "ID  %-14s  %-15s  PV" RESET "\n", "Nom", "Classe");
    for (int i = 0; i < r->len; i++) {
        aligner_texte(nom, sizeof(nom), r->data[i].nom, 14);
        aligner_texte(classe, sizeof(classe), classes[r->data[i].classe].nom, 15);
        g = groupe_de(groupes, nb_groupes, r->data[i].nom);
        printf("  " GRAS "%2d" RESET "  %s%s" RESET "  %s%s" RESET "  " GRIS "%-3d" RESET,
            i + 1, est_ko(&r->data[i]) ? GRIS : GRAS, nom,
            couleur_ansi(r->data[i].classe), classe, r->data[i].pv);
        if (est_ko(&r->data[i]))
            printf("  " ROUGE "K.O." RESET);
        else if (g != NULL)
            printf("  %s%s" RESET, g->couleur, g->libelle);
        printf("\n");
    }
    printf("\n");
}
