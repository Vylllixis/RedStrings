/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ audio_lecture.c ────────────────────────────────────────────────────
** │ But         Jouer un bruitage à partir d'un événement : c'est la seule
** │             porte d'entrée des écrans vers les sons.
** │ Contenu     actives, jouer, son, son_secondaire, son_regle,
** │             audio_couper_voix
** │ Utilise     les règles pures d'audio_regles.c (repli, variante,
** │             rythme, voix, volume) et les tampons d'audio_moteur.c
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include <stdio.h>
#include "audio_interne.h"
#include "gui_app.h"

#define ATTENUATION_SON 0.f     /* la distance ne baisse jamais le volume */

/* Lectures de `s` encore en cours. */
static int actives(const audio_t *a, son_t s)
{
    int n = 0;

    for (int i = 0; i < NB_VOIX; i++)
        n += a->voix[i].id == (int)s && sfSound_getStatus(a->voix[i].son) == sfPlaying;
    return n;
}

static int voix_libre(audio_t *a, int priorite)
{
    voix_info_t infos[NB_VOIX];

    for (int i = 0; i < NB_VOIX; i++) {
        infos[i].active = sfSound_getStatus(a->voix[i].son) == sfPlaying;
        infos[i].priorite = a->voix[i].priorite;
        infos[i].debut = a->voix[i].debut;
    }
    return voix_choisir(infos, NB_VOIX, priorite);
}

/* La musique baisse pendant un son important (fanfare, K.O.), le temps qu'il dure. */
static void attenuer_musique(audio_t *a, const sfSoundBuffer *t)
{
    float fin = a->temps + sfTime_asSeconds(sfSoundBuffer_getDuration(t));

    if (fin > a->fin_attenuation)
        a->fin_attenuation = fin;
}

/*
** ╭─ jouer ──────────────────────────────────────────────────────────────
** │ Joue l'événement `s`, ou ne fait rien si une règle l'interdit.
** │
** │ 1. audio prêt, son non coupé, et (pour un son secondaire) aucun son
** │    plus parlant dans la même image
** │ 2. son_resoudre() : thème actif, sinon standard, sinon silence
** │ 3. son_autorise() : délai minimum et lectures simultanées
** │ 4. variante (jamais deux fois la même de suite, parfois la rare)
** │ 5. voix_choisir() : une voix libre, ou celle d'un son moins important
** │ 6. volume des réglages, hauteur légèrement variée, position gauche/droite
** ╰──────────────────────────────────────────────────────────────────────
*/
static void jouer(app_t *app, son_t s, float hauteur, float pan, int secondaire)
{
    audio_t *a = &app->audio;
    const info_son_t *info = son_info(s);
    const pack_t *source = NULL;
    const entree_son_t *e = NULL;
    sfSoundBuffer *t = NULL;
    voix_t *v = NULL;
    int rare = 0;
    int var = 0;
    int i = 0;
    float h = 0.f;

    if (!a->pret || info == NULL || a->reglages.coupe || (secondaire && a->important))
        return;
    e = son_resoudre(&a->pack, &a->standard, s, &source);
    if (e == NULL || !son_autorise(s, a->derniere[s], actives(a, s), a->temps)) {
        if (a->trace)
            fprintf(stderr, "audio %7.2f  %-20s %s\n", a->temps, info->nom,
                e == NULL ? "muet (aucun pack)" : "refusé (rythme)");
        return;
    }
    var = variante_choisir(e, a->precedente[s], &a->graine, &rare);
    t = tampon_obtenir(a, source, rare ? e->rare : e->fichiers[var]);
    i = t != NULL ? voix_libre(a, info->priorite) : -1;
    if (i < 0)
        return;
    v = &a->voix[i];
    h = e->hauteur >= 0.f ? e->hauteur : info->hauteur;
    sfSound_stop(v->son);
    sfSound_setBuffer(v->son, t);
    sfSound_setVolume(v->son, volume_effectif(&a->reglages, 0, e->volume));
    sfSound_setPitch(v->son, hauteur * (1.f + (hasard(&a->graine) * 2.f - 1.f) * h));
    sfSound_setPosition(v->son, (sfVector3f){pan, 0.f, fabsf(pan) > 0.001f ? -1.f : 0.f});
    sfSound_setAttenuation(v->son, ATTENUATION_SON);
    sfSound_play(v->son);
    if (a->trace)
        fprintf(stderr, "audio %7.2f  %-20s %s%s\n", a->temps, info->nom,
            rare ? e->rare : e->fichiers[var], source == &a->standard && a->theme
            != THEME_STANDARD ? "  (repli standard)" : "");
    *v = (voix_t){v->son, (int)s, info->priorite, a->temps};
    a->derniere[s] = a->temps;
    a->precedente[s] = rare ? a->precedente[s] : var;
    a->important |= info->priorite >= 2;
    if (info->priorite >= 3)
        attenuer_musique(a, t);
}

void son(app_t *app, son_t s)
{
    jouer(app, s, 1.f, 0.f, 0);
}

/* Retour sonore de repli : seulement si rien de plus parlant n'a sonné dans l'image. */
void son_secondaire(app_t *app, son_t s)
{
    jouer(app, s, 1.f, 0.f, 1);
}

/* `hauteur` multiplie la hauteur (1 = normale) ; `pan` va de -1 (gauche) à 1 (droite). */
void son_regle(app_t *app, son_t s, float hauteur, float pan)
{
    jouer(app, s, hauteur > 0.f ? hauteur : 1.f, pan < -1.f ? -1.f : (pan > 1.f ? 1.f : pan),
        0);
}

void audio_couper_voix(app_t *app)
{
    for (int i = 0; i < NB_VOIX; i++) {
        if (app->audio.voix[i].son != NULL)
            sfSound_stop(app->audio.voix[i].son);
    }
}
