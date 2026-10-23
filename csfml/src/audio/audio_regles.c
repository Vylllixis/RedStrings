/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ audio_regles.c ─────────────────────────────────────────────────────
** │ But         Les décisions du système audio, sans rien jouer : quel
** │             pack fournit le son, quelle variante, à quel volume, s'il
** │             a le droit de sonner maintenant, sur quelle voix.
** │ Contenu     hasard, son_resoudre, variante_choisir, volume_effectif,
** │             son_autorise, voix_choisir, theme_effectif
** │ Règle       aucun SFML ici : tout se teste sans carte son
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "gui_sons.h"

/* Nombre pseudo-aléatoire dans [0, 1) (xorshift 32 bits, graine jamais nulle). */
float hasard(unsigned int *graine)
{
    unsigned int x = *graine != 0 ? *graine : 0x9E3779B9u;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *graine = x;
    return (float)(x >> 8) / 16777216.f;
}

/*
** ╭─ son_resoudre ───────────────────────────────────────────────────────
** │ Hiérarchie de repli d'un événement :
** │
** │ 1. le pack du thème actif, s'il définit l'événement
** │ 2. sinon le pack standard
** │ 3. sinon NULL : l'événement reste silencieux, sans erreur
** │ `source` reçoit le pack retenu (pour construire le chemin du fichier).
** ╰──────────────────────────────────────────────────────────────────────
*/
const entree_son_t *son_resoudre(const pack_t *theme, const pack_t *standard, son_t s,
    const pack_t **source)
{
    const pack_t *ordre[2] = {theme, standard};

    *source = NULL;
    if ((int)s < 0 || s >= NB_SONS)
        return NULL;
    for (int i = 0; i < 2; i++) {
        if (ordre[i] != NULL && ordre[i]->sons[s].nb > 0) {
            *source = ordre[i];
            return &ordre[i]->sons[s];
        }
    }
    return NULL;
}

/*
** ╭─ variante_choisir ───────────────────────────────────────────────────
** │ Choisit le fichier à jouer parmi les variantes d'une entrée.
** │
** │ - la variante rare passe en premier, avec sa probabilité (*rare = 1)
** │ - sinon une variante au hasard, jamais deux fois la même de suite
** │   (la répétition exacte est ce qui rend un bruitage agaçant)
** │ Renvoie l'indice de la variante (0 pour la rare).
** ╰──────────────────────────────────────────────────────────────────────
*/
int variante_choisir(const entree_son_t *e, int precedente, unsigned int *graine,
    int *rare)
{
    int i = 0;

    *rare = e->rare[0] != '\0' && hasard(graine) < e->proba_rare;
    if (*rare || e->nb <= 1)
        return 0;
    if (precedente < 0 || precedente >= e->nb)
        return (int)(hasard(graine) * (float)e->nb) % e->nb;
    i = (int)(hasard(graine) * (float)(e->nb - 1)) % (e->nb - 1);
    return i >= precedente ? i + 1 : i;
}

/* Courbe du curseur : l'oreille perçoit le volume de façon logarithmique. */
static float courbe(int v)
{
    float x = (float)(v < 0 ? 0 : (v > VOLUME_MAX ? VOLUME_MAX : v)) / VOLUME_MAX;

    return x * x;
}

/*
** ╭─ volume_effectif ────────────────────────────────────────────────────
** │ Volume SFML (0..100) d'une lecture :
** │ général x (musique ou effets) x gain du manifeste, chaque curseur
** │ passant par une courbe au carré (50 % sur le curseur ≈ -12 dB, ce
** │ que l'oreille entend comme « moitié moins fort »). Coupé : 0.
** ╰──────────────────────────────────────────────────────────────────────
*/
float volume_effectif(const reglages_audio_t *r, int musique, float gain)
{
    float v = 0.f;

    if (r->coupe)
        return 0.f;
    v = courbe(r->general) * courbe(musique ? r->musique : r->effets) * gain * 100.f;
    if (v < 0.f)
        return 0.f;
    return v > 100.f ? 100.f : v;
}

/* Un événement sonne s'il ne dépasse ni ses lectures simultanées ni son rythme. */
int son_autorise(son_t s, float derniere, int actives, float maintenant)
{
    const info_son_t *info = son_info(s);

    if (info == NULL || actives >= info->max)
        return 0;
    return maintenant - derniere >= info->delai;
}

/*
** ╭─ voix_choisir ───────────────────────────────────────────────────────
** │ Voix de lecture pour un son de priorité `priorite`.
** │
** │ 1. une voix libre, s'il y en a une
** │ 2. sinon la voix de plus faible priorité (la plus ancienne à
** │    égalité), si elle n'est pas plus importante que le nouveau son
** │ 3. sinon -1 : le nouveau son est abandonné, pour ne pas couper un
** │    son plus important (un K.O. n'est jamais masqué par des survols)
** ╰──────────────────────────────────────────────────────────────────────
*/
int voix_choisir(const voix_info_t *voix, int n, int priorite)
{
    int choix = -1;

    for (int i = 0; i < n; i++) {
        if (!voix[i].active)
            return i;
    }
    for (int i = 0; i < n; i++) {
        if (voix[i].priorite > priorite)
            continue;
        if (choix == -1 || voix[i].priorite < voix[choix].priorite
            || (voix[i].priorite == voix[choix].priorite
            && voix[i].debut < voix[choix].debut))
            choix = i;
    }
    return choix;
}

theme_son_t theme_effectif(choix_theme_t choix, int mode_yugioh)
{
    if (choix == CHOIX_THEME_STANDARD)
        return THEME_STANDARD;
    if (choix == CHOIX_THEME_YUGIOH)
        return THEME_YUGIOH;
    return mode_yugioh ? THEME_YUGIOH : THEME_STANDARD;
}
