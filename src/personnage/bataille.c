/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ bataille.c ─────────────────────────────────────────────────────────
** │ But         Le moteur du combat en équipe, au tour par tour. Il ne
** │             contient AUCUN affichage : il décide, les interfaces
** │             montrent.
** │ Sections    Journal · Combattants et effets · Initialisation et tours
** │             · Règles des actions · Résolution
** │ Utilisé par bataille_console.c (terminal) et l'arène de la version
** │             graphique
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdarg.h>
#include <stdio.h>
#include "bataille.h"

/* ------------------------------------------------------------------ */
/*  Journal                                                            */
/* ------------------------------------------------------------------ */

/*
** ╭─ noter ──────────────────────────────────────────────────────────────
** │ Écrit une ligne dans le journal du combat.
** │
** │ Le journal est un tampon circulaire de JOURNAL_MAX lignes : la case
** │ est nb_journal % JOURNAL_MAX, donc les lignes les plus anciennes sont
** │ écrasées. vsnprintf() borne chaque ligne.
** ╰──────────────────────────────────────────────────────────────────────
*/
static void noter(bataille_t *b, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

static void noter(bataille_t *b, const char *format, ...)
{
    va_list args;

    if (b == NULL || format == NULL)
        return;

    va_start(args, format);
    vsnprintf(b->journal[b->nb_journal % JOURNAL_MAX], JOURNAL_LIGNE, format, args);
    va_end(args);
    b->nb_journal++;
}

const char *bataille_journal(const bataille_t *b, int i)
{
    if (b == NULL || i < 0 || i >= b->nb_journal || i >= JOURNAL_MAX)
        return NULL;
    return b->journal[(b->nb_journal - 1 - i) % JOURNAL_MAX];
}

/* ------------------------------------------------------------------ */
/*  Combattants et effets                                              */
/* ------------------------------------------------------------------ */

/** 1 si r désigne un combattant qui existe dans ce combat. */
static int ref_valide(const bataille_t *b, ref_t r)
{
    return b != NULL && r.equipe >= 0 && r.equipe < NB_EQUIPES
        && b->equipes[r.equipe].taille >= 0
        && b->equipes[r.equipe].taille <= TAILLE_EQUIPE_MAX
        && r.index >= 0 && r.index < b->equipes[r.equipe].taille;
}

const combattant_t *bataille_lire(const bataille_t *b, ref_t r)
{
    return ref_valide(b, r) ? &b->equipes[r.equipe].membres[r.index] : NULL;
}

combattant_t *bataille_combattant(bataille_t *b, ref_t r)
{
    return ref_valide(b, r) ? &b->equipes[r.equipe].membres[r.index] : NULL;
}

int combattant_vivant(const combattant_t *c)
{
    return c != NULL && c->pv > 0;
}

static int chercher_effet(const combattant_t *c, type_effet_t type)
{
    if (c == NULL)
        return -1;
    for (int i = 0; i < c->nb_effets && i < MAX_EFFETS; i++) {
        if (c->effets[i].type == type)
            return i;
    }
    return -1;
}

int combattant_en_garde(const combattant_t *c)
{
    return chercher_effet(c, EFFET_GARDE) != -1;
}

/** Ajoute l'effet, ou relance sa durée s'il est déjà actif. */
static void poser_effet(combattant_t *c, type_effet_t type, int tours)
{
    int i = chercher_effet(c, type);

    if (c == NULL)
        return;
    if (i == -1 && c->nb_effets < MAX_EFFETS)
        i = c->nb_effets++;
    if (i == -1)
        return;
    c->effets[i] = (effet_t){type, tours};
}

/*
** ╭─ vieillir_effets ────────────────────────────────────────────────────
** │ Au début du tour du porteur, chaque effet perd 1 tour ; à 0 il
** │ expire.
** │
** │ Un effet expiré est retiré en recopiant le dernier à sa place
** │ (l'ordre n'a pas d'importance). Quand c'est la garde, on produit un
** │ événement EV_GARDE_FIN et une ligne de journal.
** ╰──────────────────────────────────────────────────────────────────────
*/
static int vieillir_effets(bataille_t *b, ref_t r, evenement_t *ev, int n)
{
    combattant_t *c = bataille_combattant(b, r);
    int i = 0;

    if (c == NULL)
        return n;
    while (i < c->nb_effets && c->nb_effets <= MAX_EFFETS) {
        if (--c->effets[i].tours > 0) {
            i++;
            continue;
        }
        if (c->effets[i].type == EFFET_GARDE && n < MAX_EVENEMENTS) {
            ev[n++] = (evenement_t){EV_GARDE_FIN, r, r, 0, c->pv, 0};
            noter(b, "%s baisse sa garde.", c->perso.nom);
        }
        c->effets[i] = c->effets[--c->nb_effets];
    }
    return n;
}

/* ------------------------------------------------------------------ */
/*  Initialisation et tours                                            */
/* ------------------------------------------------------------------ */

static void preparer_equipe(equipe_t *e, const personnage_t *persos, int n)
{
    e->taille = n;
    for (int i = 0; i < n; i++) {
        e->membres[i] = (combattant_t){0};
        e->membres[i].perso = persos[i];
        e->membres[i].pv = persos[i].pv;
        e->membres[i].pv_max = persos[i].pv;
    }
}

static int equipe_valide(const personnage_t *persos, int n)
{
    if (persos == NULL || n < 1 || n > TAILLE_EQUIPE_MAX)
        return 0;
    for (int i = 0; i < n; i++) {
        if (persos[i].pv <= 0)
            return 0;
    }
    return 1;
}

/*
** ╭─ calculer_ordre ─────────────────────────────────────────────────────
** │ Fixe l'ordre des tours, une seule fois au début du combat.
** │
** │ Les équipes alternent, rang par rang : J1, A1, J2, A2... Cet ordre ne
** │ dépend d'aucune statistique, donc le combat est prévisible.
** ╰──────────────────────────────────────────────────────────────────────
*/
static void calculer_ordre(bataille_t *b)
{
    b->nb_ordre = 0;
    for (int i = 0; i < TAILLE_EQUIPE_MAX; i++) {
        for (int e = 0; e < NB_EQUIPES; e++) {
            if (i < b->equipes[e].taille)
                b->ordre[b->nb_ordre++] = (ref_t){e, i};
        }
    }
}

int bataille_init(bataille_t *b, const personnage_t *joueurs, int nb_joueurs,
    const personnage_t *adversaires, int nb_adversaires)
{
    if (b == NULL || !equipe_valide(joueurs, nb_joueurs)
        || !equipe_valide(adversaires, nb_adversaires))
        return -1;
    *b = (bataille_t){0};
    preparer_equipe(&b->equipes[0], joueurs, nb_joueurs);
    preparer_equipe(&b->equipes[1], adversaires, nb_adversaires);
    calculer_ordre(b);
    b->gagnant = -1;
    noter(b, "Le combat commence : %d contre %d.", nb_joueurs, nb_adversaires);
    return 0;
}

ref_t bataille_actif(const bataille_t *b)
{
    if (b == NULL || b->tour < 0 || b->tour >= b->nb_ordre
        || b->nb_ordre > NB_EQUIPES * TAILLE_EQUIPE_MAX)
        return (ref_t){-1, -1};
    return b->ordre[b->tour];
}

static int equipe_vivante(const equipe_t *e)
{
    for (int i = 0; i < e->taille; i++) {
        if (combattant_vivant(&e->membres[i]))
            return 1;
    }
    return 0;
}

/*
** ╭─ tour_suivant ───────────────────────────────────────────────────────
** │ Passe au prochain combattant VIVANT.
** │
** │ 1. avance dans l'ordre ; en fin de liste on revient à 0 et manche++
** │ 2. saute les K.O. (au plus nb_ordre essais, donc jamais de boucle
** │    infinie)
** │ 3. fait vieillir les effets du nouvel actif
** ╰──────────────────────────────────────────────────────────────────────
*/
static int tour_suivant(bataille_t *b, evenement_t *ev, int n)
{
    if (b->nb_ordre <= 0)
        return n;
    for (int k = 0; k < b->nb_ordre; k++) {
        b->tour++;
        if (b->tour >= b->nb_ordre) {
            b->tour = 0;
            b->manche++;
        }
        if (combattant_vivant(bataille_lire(b, bataille_actif(b))))
            break;
    }
    return vieillir_effets(b, bataille_actif(b), ev, n);
}

/* ------------------------------------------------------------------ */
/*  Règles des actions                                                 */
/* ------------------------------------------------------------------ */

/*
** ╭─ bataille_degats ────────────────────────────────────────────────────
** │ Dégâts réels d'une attaque.
** │
** │ 1. calculer_degats() : ATQ x (100 - DEF) / 100
** │ 2. si la cible est en garde : encore -GARDE_REDUCTION %
** │ 3. au minimum 1 : un coup fait toujours au moins 1 dégât
** ╰──────────────────────────────────────────────────────────────────────
*/
int bataille_degats(const combattant_t *attaquant, const combattant_t *cible)
{
    int degats = 0;

    if (attaquant == NULL || cible == NULL)
        return 0;
    degats = calculer_degats(&attaquant->perso, &cible->perso);
    if (combattant_en_garde(cible))
        degats = degats * (100 - GARDE_REDUCTION) / 100;
    return degats < 1 ? 1 : degats;
}

int bataille_soin(const combattant_t *soigneur, const combattant_t *cible)
{
    int manque = 0;
    int soin = 0;

    if (soigneur == NULL || cible == NULL)
        return 0;
    manque = cible->pv_max - cible->pv;
    soin = soigneur->perso.soin;
    if (manque <= 0 || soin <= 0)
        return 0;
    return soin < manque ? soin : manque;
}

/*
** ╭─ bataille_cible_valide ──────────────────────────────────────────────
** │ Dit si `cible` est permise pour l'action `a`. C'est LE point de
** │ contrôle des règles.
** │
** │ - toujours refusé : combat fini, indice hors équipe, cible K.O.
** │ - ATTAQUER : une cible de l'équipe adverse
** │ - SOIGNER : un allié (soi-même compris) qui a des PV à regagner
** │ - DEFENDRE : uniquement soi-même
** │ bataille_agir() refuse tout ce que cette fonction n'accepte pas.
** ╰──────────────────────────────────────────────────────────────────────
*/
int bataille_cible_valide(const bataille_t *b, action_t a, ref_t cible)
{
    ref_t r = bataille_actif(b);
    const combattant_t *acteur = bataille_lire(b, r);
    const combattant_t *c = bataille_lire(b, cible);

    if (b == NULL || b->gagnant != -1 || !combattant_vivant(acteur)
        || !combattant_vivant(c) || (unsigned)a >= NB_ACTIONS)
        return 0;
    if (a == ACTION_ATTAQUER)
        return cible.equipe != r.equipe;
    if (a == ACTION_SOIGNER)
        return cible.equipe == r.equipe && bataille_soin(acteur, c) > 0;
    return a == ACTION_DEFENDRE && cible.equipe == r.equipe && cible.index == r.index;
}

int bataille_cibles(const bataille_t *b, action_t a,
    ref_t cibles[NB_EQUIPES * TAILLE_EQUIPE_MAX])
{
    int n = 0;

    if (b == NULL || cibles == NULL)
        return 0;
    for (int e = 0; e < NB_EQUIPES; e++) {
        for (int i = 0; i < b->equipes[e].taille && i < TAILLE_EQUIPE_MAX; i++) {
            if (bataille_cible_valide(b, a, (ref_t){e, i}))
                cibles[n++] = (ref_t){e, i};
        }
    }
    return n;
}

int bataille_action_possible(const bataille_t *b, action_t a)
{
    ref_t cibles[NB_EQUIPES * TAILLE_EQUIPE_MAX];

    return bataille_cibles(b, a, cibles) > 0;
}

/* ------------------------------------------------------------------ */
/*  Résolution                                                         */
/* ------------------------------------------------------------------ */

static int attaquer_cible(bataille_t *b, ref_t r, ref_t cible, evenement_t *ev,
    int n)
{
    const combattant_t *a = bataille_lire(b, r);
    combattant_t *c = bataille_combattant(b, cible);
    int garde = combattant_en_garde(c);
    int degats = bataille_degats(a, c);

    if (a == NULL || c == NULL)
        return n;
    c->pv = degats >= c->pv ? 0 : c->pv - degats;
    ev[n++] = (evenement_t){EV_ATTAQUE, r, cible, degats, c->pv, garde};
    noter(b, "%s attaque %s et inflige %d dégât%s%s.", a->perso.nom, c->perso.nom,
        degats, degats > 1 ? "s" : "", garde ? " (garde)" : "");
    if (c->pv == 0) {
        ev[n++] = (evenement_t){EV_KO, r, cible, 0, 0, 0};
        noter(b, "%s est vaincu.", c->perso.nom);
    }
    return n;
}

static int soigner_cible(bataille_t *b, ref_t r, ref_t cible, evenement_t *ev,
    int n)
{
    const combattant_t *s = bataille_lire(b, r);
    combattant_t *c = bataille_combattant(b, cible);
    int rendu = bataille_soin(s, c);

    if (s == NULL || c == NULL)
        return n;
    c->pv += rendu;
    ev[n++] = (evenement_t){EV_SOIN, r, cible, rendu, c->pv, 0};
    if (r.index == cible.index)
        noter(b, "%s se soigne de %d PV.", s->perso.nom, rendu);
    else
        noter(b, "%s soigne %s de %d PV.", s->perso.nom, c->perso.nom, rendu);
    return n;
}

static int se_defendre(bataille_t *b, ref_t r, evenement_t *ev, int n)
{
    combattant_t *c = bataille_combattant(b, r);

    if (c == NULL)
        return n;
    poser_effet(c, EFFET_GARDE, GARDE_DUREE);
    ev[n++] = (evenement_t){EV_GARDE, r, r, GARDE_REDUCTION, c->pv, 0};
    noter(b, "%s adopte une posture défensive.", c->perso.nom);
    return n;
}

/*
** ╭─ bataille_agir ──────────────────────────────────────────────────────
** │ Joue l'action du combattant actif.
** │
** │ 1. bataille_cible_valide() : sinon renvoie -1, rien n'a changé
** │ 2. applique l'action (attaquer_cible, soigner_cible ou se_defendre)
** │ 3. si une équipe n'a plus de vivant : gagnant + événement EV_VICTOIRE
** │ 4. sinon tour_suivant() passe la main
** │ ev[] reçoit les événements produits (de quoi animer l'interface).
** │ Renvoie leur nombre, ou -1 si l'action est refusée.
** ╰──────────────────────────────────────────────────────────────────────
*/
int bataille_agir(bataille_t *b, action_t a, ref_t cible,
    evenement_t ev[MAX_EVENEMENTS])
{
    ref_t r = bataille_actif(b);
    combattant_t *acteur = NULL;
    int n = 0;

    if (ev == NULL || !bataille_cible_valide(b, a, cible))
        return -1;
    acteur = bataille_combattant(b, r);
    if (acteur == NULL)
        return -1;
    acteur->defensifs = a == ACTION_ATTAQUER ? 0 : acteur->defensifs + 1;
    if (a == ACTION_ATTAQUER)
        n = attaquer_cible(b, r, cible, ev, n);
    else if (a == ACTION_SOIGNER)
        n = soigner_cible(b, r, cible, ev, n);
    else
        n = se_defendre(b, r, ev, n);
    for (int e = 0; e < NB_EQUIPES; e++) {
        if (!equipe_vivante(&b->equipes[e])) {
            b->gagnant = 1 - e;
            ev[n++] = (evenement_t){EV_VICTOIRE, r, r, b->gagnant, 0, 0};
            noter(b, b->gagnant == 0 ? "L'équipe adverse a été vaincue."
                : "Ton équipe a été vaincue.");
            return n;
        }
    }
    return tour_suivant(b, ev, n);
}
