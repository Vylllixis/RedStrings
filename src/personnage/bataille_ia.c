/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ bataille_ia.c ──────────────────────────────────────────────────────
** │ But         Le cerveau de l'ordinateur pendant un combat : un choix
** │             raisonnable mais pas parfait, pour que le combat reste
** │             amusant (jamais de soin à l'infini).
** │ Utilise     bataille.h ; rand() pour le hasard
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdlib.h>
#include "bataille.h"

#define SOIN_SEUIL_POURCENT 50      /* un allié sous ce seuil mérite un soin */
#define GARDE_SEUIL_POURCENT 25     /* on se garde sous ce seuil */
#define LASSITUDE_PAR_ACTION 35     /* % de motivation perdus par soin/garde d'affilée */
#define ERREUR_CIBLE_POURCENT 20    /* chance de viser un ennemi au hasard */

/** Vrai avec une probabilité de pourcent % (0 = jamais, 100 = toujours). */
static int tirage(int pourcent)
{
    if (pourcent >= 100)
        return 1;
    if (pourcent <= 0)
        return 0;
    return rand() % 100 < pourcent;
}

/** Envie de se soigner / se garder : 100 % au début, puis de moins en moins. */
static int motivation(const combattant_t *moi)
{
    int m = 100 - moi->defensifs * LASSITUDE_PAR_ACTION;

    return moi->defensifs >= DEFENSIFS_MAX || m < 0 ? 0 : m;
}

/** Indice de l'allié le plus blessé sous le seuil de soin, ou -1. */
static int allie_a_soigner(const bataille_t *b, const ref_t *cibles, int n)
{
    const combattant_t *c = NULL;
    const combattant_t *pire = NULL;
    int choix = -1;

    for (int i = 0; i < n; i++) {
        c = bataille_lire(b, cibles[i]);
        if (c->pv * 100 >= c->pv_max * SOIN_SEUIL_POURCENT)
            continue;
        if (pire == NULL || c->pv * pire->pv_max < pire->pv * c->pv_max) {
            pire = c;
            choix = i;
        }
    }
    return choix;
}

/** Indice d'un ennemi que l'attaque tuerait (le plus fort des deux : le
 *  premier trouvé), ou -1. */
static int ennemi_achevable(const bataille_t *b, const combattant_t *moi,
    const ref_t *cibles, int n)
{
    for (int i = 0; i < n; i++) {
        if (bataille_degats(moi, bataille_lire(b, cibles[i]))
            >= bataille_lire(b, cibles[i])->pv)
            return i;
    }
    return -1;
}

/** Ennemi aux PV les plus bas, sauf une fois sur cinq où il se trompe. */
static int ennemi_a_viser(const bataille_t *b, const ref_t *cibles, int n)
{
    int choix = 0;

    if (n > 1 && tirage(ERREUR_CIBLE_POURCENT))
        return rand() % n;
    for (int i = 1; i < n; i++) {
        if (bataille_lire(b, cibles[i])->pv < bataille_lire(b, cibles[choix])->pv)
            choix = i;
    }
    return choix;
}

/* Soigne ou se garde si c'est utile ET si l'ordinateur en a envie. */
static int choisir_defense(const bataille_t *b, const combattant_t *moi, ref_t r,
    action_t *a, ref_t *cible)
{
    ref_t soins[NB_EQUIPES * TAILLE_EQUIPE_MAX];
    int n = bataille_cibles(b, ACTION_SOIGNER, soins);
    int choix = allie_a_soigner(b, soins, n);
    int garde = moi->pv_max > 0 && moi->pv * 100 < moi->pv_max * GARDE_SEUIL_POURCENT
        && !combattant_en_garde(moi);

    if ((choix == -1 && !garde) || !tirage(motivation(moi)))
        return 0;
    *a = choix != -1 ? ACTION_SOIGNER : ACTION_DEFENDRE;
    *cible = choix != -1 ? soins[choix] : r;
    return 1;
}

/*
** ╭─ bataille_ia ────────────────────────────────────────────────────────
** │ Dans l'ordre :
** │
** │ 1. si une attaque achève un ennemi, il la donne (toujours)
** │ 2. s'il y a un blessé (soin) ou s'il est très bas (garde), il y va
** │    avec une envie qui baisse à chaque soin/garde d'affilée : 100 %,
** │    65 %, 30 %, puis 0 % (combattant_t.defensifs, remis à 0 par une
** │    attaque). Il ne peut donc jamais se soigner indéfiniment.
** │ 3. sinon il attaque, en visant d'habitude l'ennemi le plus faible
** │    mais en se trompant une fois sur cinq
** ╰──────────────────────────────────────────────────────────────────────
*/
void bataille_ia(const bataille_t *b, action_t *a, ref_t *cible)
{
    ref_t cibles[NB_EQUIPES * TAILLE_EQUIPE_MAX];
    ref_t r;
    const combattant_t *moi = NULL;
    int n = 0;
    int choix = 0;

    if (b == NULL || a == NULL || cible == NULL)
        return;
    r = bataille_actif(b);
    moi = bataille_lire(b, r);
    if (moi == NULL)
        return;
    n = bataille_cibles(b, ACTION_ATTAQUER, cibles);
    choix = ennemi_achevable(b, moi, cibles, n);
    if (choix == -1 && choisir_defense(b, moi, r, a, cible))
        return;
    if (choix == -1)
        choix = ennemi_a_viser(b, cibles, n);
    if (n <= 0 || choix < 0 || choix >= n)
        return;
    *a = ACTION_ATTAQUER;
    *cible = cibles[choix];
}
