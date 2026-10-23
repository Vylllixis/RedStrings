/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ audio_moteur.c ─────────────────────────────────────────────────────
** │ But         Créer et détruire le moteur audio, charger les packs du
** │             thème actif, garder les bruitages décodés en mémoire, et
** │             changer de thème ou de réglages en cours de partie.
** │ Contenu     audio_chemin_pack, charger_pack, tampon_obtenir,
** │             precharger, vider_tampons, audio_init, audio_liberer,
** │             audio_changer_theme, audio_appliquer_reglages
** │ Règle       aucune erreur audio n'arrête le jeu : sans carte son ou
** │             sans fichier, le son correspondant reste muet
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "audio_interne.h"
#include "gui_app.h"

/* Dossier du pack d'un thème : csfml/assets/audio/<thème>, trouvé comme les polices. */
int audio_chemin_pack(theme_son_t t, char *dossier, size_t taille)
{
    char relatif[96];
    char chemin[SON_CHEMIN_MAX];
    char *slash = NULL;

    snprintf(relatif, sizeof(relatif), "audio/%s/%s", dossier_theme(t), MANIFESTE_AUDIO);
    if (chemin_ressource(relatif, chemin, sizeof(chemin)) != 0)
        return -1;
    slash = strrchr(chemin, '/');
    if (slash != NULL)
        *slash = '\0';
    return snprintf(dossier, taille, "%s", chemin) >= (int)taille ? -1 : 0;
}

static void charger_pack(pack_t *p, theme_son_t t)
{
    char dossier[sizeof(p->dossier)];
    char msg[300];
    int refus = 0;

    if (audio_chemin_pack(t, dossier, sizeof(dossier)) != 0) {
        pack_vider(p, "");
        fprintf(stderr, "redstrings_csfml : pack audio « %s » introuvable, "
            "ses sons seront remplacés par ceux du pack standard\n", nom_theme(t));
        return;
    }
    refus = pack_charger(p, dossier, MANIFESTE_AUDIO, msg, sizeof(msg));
    if (refus != 0)
        fprintf(stderr, "redstrings_csfml : %s%s\n", msg,
            refus > 1 ? " (et d'autres lignes ignorées)" : "");
}

/*
** ╭─ tampon_obtenir ─────────────────────────────────────────────────────
** │ Le son décodé d'un fichier du pack, chargé au premier besoin puis
** │ gardé : une lecture ne relit jamais le disque.
** │
** │ Un fichier illisible est mémorisé avec un tampon NULL : il est signalé
** │ une seule fois sur stderr, et ses lectures suivantes restent muettes
** │ sans nouveau message.
** ╰──────────────────────────────────────────────────────────────────────
*/
sfSoundBuffer *tampon_obtenir(audio_t *a, const pack_t *p, const char *fichier)
{
    char chemin[SON_CHEMIN_MAX];
    tampon_t *t = NULL;

    if (pack_chemin(p, fichier, chemin, sizeof(chemin)) != 0)
        return NULL;
    for (int i = 0; i < a->nb_tampons; i++) {
        if (strcmp(a->tampons[i].chemin, chemin) == 0)
            return a->tampons[i].tampon;
    }
    if (a->nb_tampons >= MAX_TAMPONS)
        return NULL;
    t = &a->tampons[a->nb_tampons++];
    snprintf(t->chemin, sizeof(t->chemin), "%s", chemin);
    t->tampon = sfSoundBuffer_createFromFile(chemin);
    if (t->tampon == NULL)
        fprintf(stderr, "redstrings_csfml : son illisible, ignoré : %s\n", chemin);
    return t->tampon;
}

/* Décode d'avance les bruitages du thème actif (et ceux du standard qui le complètent). */
static void precharger(audio_t *a)
{
    const pack_t *source = NULL;
    const entree_son_t *e = NULL;

    for (int s = 0; s < NB_SONS; s++) {
        e = son_resoudre(&a->pack, &a->standard, (son_t)s, &source);
        if (e == NULL)
            continue;
        for (int v = 0; v < e->nb; v++)
            tampon_obtenir(a, source, e->fichiers[v]);
        if (e->rare[0] != '\0')
            tampon_obtenir(a, source, e->rare);
    }
}

static void vider_tampons(app_t *app)
{
    audio_t *a = &app->audio;

    audio_couper_voix(app);     // SFML détache lui-même les voix d'un tampon détruit
    for (int i = 0; i < a->nb_tampons; i++) {
        if (a->tampons[i].tampon != NULL)
            sfSoundBuffer_destroy(a->tampons[i].tampon);
    }
    a->nb_tampons = 0;
}

/* Le pack du thème actif ; pour le thème standard il reste vide (tout vient du standard). */
static void charger_theme(audio_t *a, theme_son_t t)
{
    a->theme = t;
    if (t == THEME_STANDARD)
        pack_vider(&a->pack, a->standard.dossier);
    else
        charger_pack(&a->pack, t);
    precharger(a);
}

/*
** ╭─ audio_init ─────────────────────────────────────────────────────────
** │ Prépare l'audio, une fois, après la fenêtre et les ressources.
** │
** │ 1. réglages du joueur (fichier reglages_audio.txt, défauts sinon)
** │ 2. NB_VOIX voix de lecture créées une fois pour toutes
** │ 3. pack standard, puis pack du thème choisi (« auto » : le mode visuel)
** │ 4. préchargement des bruitages du thème (la musique, elle, est lue en
** │    continu depuis le disque)
** │ Si une voix ne peut pas être créée, l'audio reste désactivé (pret = 0).
** ╰──────────────────────────────────────────────────────────────────────
*/
void audio_init(app_t *app)
{
    audio_t *a = &app->audio;

    reglages_defaut(&a->reglages);
    reglages_lire(&a->reglages, FICHIER_REGLAGES_AUDIO);
    a->graine = (unsigned int)time(NULL) | 1u;
    a->attenuation = 1.f;
    for (int i = 0; i < NB_SONS; i++) {
        a->derniere[i] = -1000.f;
        a->precedente[i] = -1;
    }
    for (int i = 0; i < 2; i++)
        a->pistes[i].piste = -1;
    for (int i = 0; i < NB_VOIX; i++) {
        a->voix[i].id = -1;
        a->voix[i].son = sfSound_create();
        if (a->voix[i].son == NULL) {
            fprintf(stderr, "redstrings_csfml : audio indisponible, le jeu reste muet\n");
            audio_liberer(app);
            return;
        }
        sfSound_setRelativeToListener(a->voix[i].son, sfTrue);
        sfSound_setAttenuation(a->voix[i].son, 0.f);
    }
    charger_pack(&a->standard, THEME_STANDARD);
    charger_theme(a, theme_effectif(a->reglages.theme, app->mode == MODE_YUGIOH));
    a->pret = 1;
}

void audio_liberer(app_t *app)
{
    audio_t *a = &app->audio;

    for (int i = 0; i < 2; i++) {
        if (a->pistes[i].flux != NULL)
            sfMusic_destroy(a->pistes[i].flux);
        a->pistes[i].flux = NULL;
    }
    vider_tampons(app);
    for (int i = 0; i < NB_VOIX; i++) {
        if (a->voix[i].son != NULL)
            sfSound_destroy(a->voix[i].son);
        a->voix[i].son = NULL;
    }
    a->pret = 0;
}

/*
** ╭─ audio_changer_theme ────────────────────────────────────────────────
** │ Change d'identité sonore en pleine partie, sans toucher aux écrans.
** │
** │ 1. retient le choix dans les réglages (et le fichier)
** │ 2. si le thème effectif change : arrête les voix, libère les sons
** │    décodés, charge et précharge le nouveau pack
** │ 3. la musique en cours passe en fondu sur sa version du nouveau thème
** ╰──────────────────────────────────────────────────────────────────────
*/
void audio_changer_theme(app_t *app, choix_theme_t choix)
{
    audio_t *a = &app->audio;
    theme_son_t t = theme_effectif(choix, app->mode == MODE_YUGIOH);
    int piste = a->pistes[0].piste;
    reglages_audio_t r = a->reglages;

    r.theme = choix;
    audio_appliquer_reglages(app, &r);
    if (!a->pret || t == a->theme)
        return;
    vider_tampons(app);
    charger_theme(a, t);
    if (piste >= 0)
        musique(app, (musique_t)piste);
}

/* Nouveaux réglages : la coupure est immédiate, et le fichier est mis à jour. */
void audio_appliquer_reglages(app_t *app, const reglages_audio_t *r)
{
    audio_t *a = &app->audio;

    if (r->coupe && !a->reglages.coupe)
        audio_couper_voix(app);
    a->reglages = *r;
    if (reglages_ecrire(&a->reglages, FICHIER_REGLAGES_AUDIO) != 0)
        fprintf(stderr, "redstrings_csfml : réglages audio non enregistrés (%s)\n",
            FICHIER_REGLAGES_AUDIO);
}
