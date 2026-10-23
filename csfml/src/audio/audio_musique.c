/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ audio_musique.c ────────────────────────────────────────────────────
** │ But         La musique d'ambiance : une boucle par situation (menu,
** │             combat), lue en continu depuis le disque, avec des fondus
** │             enchaînés et une baisse automatique pendant les sons
** │             importants.
** │ Contenu     ouvrir, musique, musique_arreter, maj_piste, audio_maj
** │ Règle       demander la musique déjà en cours ne la relance jamais
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include "audio_interne.h"
#include "gui_app.h"

#define FONDU_ENTREE 1.4f       /* secondes pour monter */
#define FONDU_SORTIE 0.9f       /* secondes pour s'éteindre lors d'un changement */
#define NIVEAU_ATTENUE 0.30f    /* niveau de la musique sous un son important */

/* Ouvre la musique `m` du thème actif (sinon celle du standard) ; NULL si aucune. */
static sfMusic *ouvrir(audio_t *a, musique_t m, float *gain)
{
    const pack_t *p = a->pack.musiques[m].fichier[0] != '\0' ? &a->pack : &a->standard;
    char chemin[SON_CHEMIN_MAX];
    unsigned int bit = 1u << (a->theme * NB_MUSIQUES + m);
    sfMusic *flux = NULL;

    if (p->musiques[m].fichier[0] == '\0'
        || pack_chemin(p, p->musiques[m].fichier, chemin, sizeof(chemin)) != 0)
        return NULL;
    flux = sfMusic_createFromFile(chemin);
    if (flux == NULL && !(a->musiques_signalees & bit)) {
        a->musiques_signalees |= bit;
        fprintf(stderr, "redstrings_csfml : musique illisible, ignorée : %s\n", chemin);
    }
    *gain = p->musiques[m].volume;
    return flux;
}

/*
** ╭─ musique ────────────────────────────────────────────────────────────
** │ Fait jouer la musique `m`, en fondu enchaîné.
** │
** │ 1. si `m` joue déjà dans le thème actif : rien (pas de redémarrage)
** │ 2. la musique courante passe dans l'emplacement « sortie » et s'éteint
** │    (une musique qui s'éteignait encore est arrêtée tout de suite)
** │ 3. la nouvelle démarre à 0 et monte en FONDU_ENTREE secondes, en boucle
** ╰──────────────────────────────────────────────────────────────────────
*/
void musique(app_t *app, musique_t m)
{
    if (app == NULL)
        return;

    audio_t *a = &app->audio;
    piste_t *p = &a->pistes[0];
    float gain = 1.f;

    if (!a->pret || (int)m < 0 || m >= NB_MUSIQUES
        || (p->piste == (int)m && p->theme == (int)a->theme && p->flux != NULL))
        return;
    musique_arreter(app, FONDU_SORTIE);
    p->flux = ouvrir(a, m, &gain);
    p->piste = (int)m;
    p->theme = (int)a->theme;
    p->niveau = 0.f;
    p->vitesse = 1.f / FONDU_ENTREE;
    p->gain = gain;
    if (p->flux == NULL)
        return;
    sfMusic_setLoop(p->flux, sfTrue);
    sfMusic_setVolume(p->flux, 0.f);
    sfMusic_play(p->flux);
}

/* La musique courante s'éteint en `duree` secondes (écran d'au revoir, changement). */
void musique_arreter(app_t *app, float duree)
{
    if (app == NULL)
        return;

    audio_t *a = &app->audio;

    if (a->pistes[1].flux != NULL)
        sfMusic_destroy(a->pistes[1].flux);
    a->pistes[1] = a->pistes[0];
    a->pistes[1].vitesse = -1.f / (duree > 0.01f ? duree : 0.01f);
    a->pistes[0] = (piste_t){NULL, -1, 0, 0.f, 0.f, 1.f};
}

static void maj_piste(audio_t *a, piste_t *p, float dt)
{
    if (p->flux == NULL)
        return;
    p->niveau += p->vitesse * dt;
    if (p->niveau >= 1.f)
        p->niveau = 1.f;
    if (p->niveau <= 0.f && p->vitesse < 0.f) {
        sfMusic_destroy(p->flux);
        p->flux = NULL;
        return;
    }
    sfMusic_setVolume(p->flux, volume_effectif(&a->reglages, 1,
        p->gain * p->niveau * a->attenuation));
}

/*
** ╭─ audio_maj ──────────────────────────────────────────────────────────
** │ Une fois par image, avant les écrans.
** │
** │ 1. avance l'horloge audio et oublie les sons importants de l'image
** │    précédente (pour son_secondaire)
** │ 2. atténuation : la musique descend vite sous un son important, puis
** │    remonte doucement quand il est fini
** │ 3. fondus des deux musiques ; le volume suit les réglages à chaque
** │    image, donc un changement de curseur s'entend tout de suite
** ╰──────────────────────────────────────────────────────────────────────
*/
void audio_maj(app_t *app)
{
    if (app == NULL)
        return;

    audio_t *a = &app->audio;
    float cible = 1.f;

    if (!a->pret)
        return;
    a->temps += app->dt;
    a->important = 0;
    cible = a->temps < a->fin_attenuation ? NIVEAU_ATTENUE : 1.f;
    if (a->attenuation > cible)
        a->attenuation = a->attenuation - app->dt * 4.f < cible ? cible
            : a->attenuation - app->dt * 4.f;
    else
        a->attenuation = a->attenuation + app->dt * 0.8f > cible ? cible
            : a->attenuation + app->dt * 0.8f;
    for (int i = 0; i < 2; i++)
        maj_piste(a, &a->pistes[i], app->dt);
}
