/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ audio_panneau.c ────────────────────────────────────────────────────
** │ But         Les réglages audio à l'écran : F2 ouvre un panneau par-dessus
** │             n'importe quel écran (volumes général, musique et effets,
** │             son coupé, thème sonore) ; F3 coupe ou rétablit le son.
** │ Contenu     ouvrir_fermer, basculer_coupe, choix_suivant, raccourcis
** │             (audio_raccourcis), clavier, volumes, ligne_coupe,
** │             ligne_theme, audio_panneau
** │ Règle       le panneau prend le clavier et la souris tant qu'il est
** │             ouvert ; les réglages sont enregistrés à sa fermeture
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include "audio_interne.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_dessin.h"
#include "gui_ecran.h"
#include "gui_hud.h"
#include "gui_ui.h"

#define PW 620.f
#define PH 470.f
#define PX ((LARGEUR - PW) / 2.f)
#define PY ((HAUTEUR - PH) / 2.f)
#define NB_LIGNES 5     /* 3 volumes, son coupé, thème */
#define PAS_VOLUME 5

static const char *LIBELLES[3] = {"Général", "Musique", "Effets"};

static int *volume(audio_t *a, int i)
{
    int *v[3] = {&a->reglages.general, &a->reglages.musique, &a->reglages.effets};

    return v[i];
}

static void ouvrir_fermer(app_t *app)
{
    panneau_audio_t *p = &app->audio.panneau;

    p->ouvert = !p->ouvert;
    p->t = 0.f;
    if (!p->ouvert)
        audio_appliquer_reglages(app, &app->audio.reglages);   // enregistre le fichier
    son(app, p->ouvert ? SON_UI_OUVERTURE : SON_UI_FERMETURE);
}

static void basculer_coupe(app_t *app)
{
    reglages_audio_t r = app->audio.reglages;

    r.coupe = !r.coupe;
    audio_appliquer_reglages(app, &r);
    notifier(app, "♪", r.coupe ? "Son coupé  (F3 pour le rétablir)" : "Son rétabli", C_CYAN);
}

static void choix_suivant(app_t *app, int sens)
{
    int c = ((int)app->audio.reglages.theme + sens + NB_CHOIX_THEMES) % NB_CHOIX_THEMES;

    audio_changer_theme(app, (choix_theme_t)c);
    son(app, SON_UI_VALIDER);       // déjà dans le nouveau thème : on l'entend tout de suite
}

/* Raccourcis valables sur tous les écrans, appelés avant eux. */
void audio_raccourcis(app_t *app)
{
    int *k = app->in.touches;

    if (!app->audio.pret)
        return;
    if (k[sfKeyF2] || (app->audio.panneau.ouvert && k[sfKeyEscape])) {
        ouvrir_fermer(app);
        k[sfKeyEscape] = 0;         // Échap ferme le panneau, pas l'écran derrière
    } else if (k[sfKeyF3]) {
        basculer_coupe(app);
    }
    k[sfKeyF2] = 0;
    k[sfKeyF3] = 0;
}

int audio_panneau_ouvert(const app_t *app)
{
    return app->audio.panneau.ouvert;
}

/* ↑↓ choisir une ligne ; ←→ régler ; Entrée ou Espace pour basculer. */
static void clavier(app_t *app, panneau_audio_t *p)
{
    const int *k = app->in.touches;
    int sens = k[sfKeyRight] ? 1 : (k[sfKeyLeft] ? -1 : 0);
    int *v = NULL;

    if (k[sfKeyDown] || k[sfKeyUp]) {
        p->ligne = (p->ligne + (k[sfKeyDown] ? 1 : NB_LIGNES - 1)) % NB_LIGNES;
        son(app, SON_UI_DEPLACEMENT);
    }
    if (p->ligne < 3 && sens != 0) {
        v = volume(&app->audio, p->ligne);
        *v += sens * (app->in.shift ? 1 : PAS_VOLUME);
        *v = *v < 0 ? 0 : (*v > VOLUME_MAX ? VOLUME_MAX : *v);
    }
    if (p->ligne == 3 && (sens != 0 || k[sfKeyEnter] || k[sfKeySpace]))
        basculer_coupe(app);
    if (p->ligne == 4 && (sens != 0 || k[sfKeyEnter] || k[sfKeySpace]))
        choix_suivant(app, sens != 0 ? sens : 1);
}

/* Les trois volumes ; changer les effets fait entendre le nouveau niveau. */
static void volumes(app_t *app, panneau_audio_t *p, float y)
{
    static const int bornes[2] = {0, VOLUME_MAX};
    const sfColor couleurs[3] = {C_CYAN, C_VIOLET, C_OR};
    int avant = 0;

    for (int i = 0; i < 3; i++) {
        avant = *volume(&app->audio, i);
        glissiere_bornee(app, &p->glissieres[i], (sfFloatRect){PX + 56.f, y + i * 48.f,
            PW - 112.f, 38.f}, LIBELLES[i], bornes, volume(&app->audio, i), couleurs[i],
            p->ligne == i);
        if (*volume(&app->audio, i) != avant && i != 1)
            son_regle(app, SON_UI_VALEUR, 0.8f + 0.6f * (float)*volume(&app->audio, i)
                / VOLUME_MAX, 0.f);
    }
}

static void ligne_coupe(app_t *app, panneau_audio_t *p, float y)
{
    int coupe = app->audio.reglages.coupe;

    if (bouton(app, &p->boutons[0], (sfFloatRect){PX + 56.f, y, PW - 112.f, 48.f},
        coupe ? "Son coupé" : "Son activé", "F3", coupe ? "cliquer pour rétablir"
        : "cliquer pour couper", coupe ? C_ROUGE : C_VERT, 1)) {
        p->ligne = 3;
        basculer_coupe(app);
    }
}

static void ligne_theme(app_t *app, panneau_audio_t *p, float y)
{
    char auto_libelle[48];
    const char *libelles[NB_CHOIX_THEMES] = {auto_libelle, "Standard", "Yu-Gi-Oh!"};
    float w = (PW - 112.f - 2 * 12.f) / NB_CHOIX_THEMES;
    sfFloatRect r;
    int clic = 0;

    snprintf(auto_libelle, sizeof(auto_libelle), "Auto (%s)",
        nom_theme(theme_effectif(CHOIX_THEME_AUTO, app->mode == MODE_YUGIOH)));
    for (int i = 0; i < NB_CHOIX_THEMES; i++) {
        r = (sfFloatRect){PX + 56.f + i * (w + 12.f), y, w, 48.f};
        clic = (int)app->audio.reglages.theme == i
            ? bouton_plein(app, &p->boutons[1 + i], r, libelles[i], NULL, C_ROUGE, 1)
            : bouton(app, &p->boutons[1 + i], r, libelles[i], NULL, NULL, C_TEXTE_DOUX, 1);
        if (clic && (int)app->audio.reglages.theme != i) {
            p->ligne = 4;
            choix_suivant(app, i - (int)app->audio.reglages.theme);
        }
    }
}

/*
** ╭─ audio_panneau ──────────────────────────────────────────────────────
** │ Dessine le panneau (vue fixe, par-dessus l'écran) et applique les
** │ réglages au fil de l'eau.
** │
** │ 1. voile sombre et panneau qui apparaissent en fondu
** │ 2. clavier : lignes, réglage, bascule
** │ 3. trois glissières de volume (0..100), le son coupé, le thème sonore
** │ La musique change de volume à l'image suivante (audio_maj) ; le
** │ fichier de réglages est écrit à la fermeture.
** ╰──────────────────────────────────────────────────────────────────────
*/
void audio_panneau(app_t *app)
{
    panneau_audio_t *p = &app->audio.panneau;
    float a = app->alpha;
    float e = 0.f;

    p->apparition = approcher(p->apparition, p->ouvert ? 1.f : 0.f, 16.f, app->dt);
    if (p->apparition < 0.01f)
        return;
    p->t += app->dt;
    e = sortie_cubique(p->apparition);
    app->alpha = p->apparition;
    rect_arrondi(app, ecran_couvrant(app), 0.f, avec_alpha(C_NOIR, 0.55f));
    ombre_biseau(app, (sfFloatRect){PX, PY + 14.f * (1.f - e), PW, PH}, 18.f, 0.8f);
    panneau(app, (sfFloatRect){PX, PY + 14.f * (1.f - e), PW, PH}, 18.f, C_SURFACE_HAUTE,
        C_SURFACE, avec_alpha(C_CYAN, 0.35f));
    titre_section(app, NULL, "AUDIO", (sfVector2f){PX + 40.f, PY + 46.f}, PW - 80.f, C_CYAN);
    texte(app, POLICE_NORMALE, "Volumes, coupure et identité sonore", 15,
        (sfVector2f){PX + 40.f, PY + 78.f}, C_TEXTE_FAIBLE, ALIGNE_GAUCHE);
    if (p->ouvert)
        clavier(app, p);
    volumes(app, p, PY + 112.f);
    ligne_coupe(app, p, PY + 270.f);
    etiquette(app, "THÈME SONORE", (sfVector2f){PX + 56.f, PY + 345.f}, C_TEXTE_DOUX,
        ALIGNE_GAUCHE);
    ligne_theme(app, p, PY + 362.f);
    texte(app, POLICE_NORMALE, "↑↓ choisir   ←→ régler   F2 ou Échap : fermer", 13,
        (sfVector2f){LARGEUR / 2.f, PY + PH - 26.f}, C_TEXTE_FAIBLE, ALIGNE_CENTRE);
    app->alpha = a;
}
