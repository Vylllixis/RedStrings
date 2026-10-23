/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ glissiere.c ────────────────────────────────────────────────────────
** │ But         Glissière de valeur avec piste graduée et poignée animée.
** │ Contenu     piste, glissiere_bornee, glissiere (bornes des statistiques)
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include <stdio.h>
#include "personnage.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "gui_ui.h"

#define GL_LABEL 168.f

#define GL_VALEUR 96.f

#define GL_CASES 20

/** Piste en 20 cases (comme les barres de la fiche), remplie jusqu'à `f`. */
static void piste(app_t *app, float tx, float tw, float cy, float f, sfColor col)
{
    float cw = (tw - (GL_CASES - 1) * 3.f) / GL_CASES;
    float rempli = 0.f;
    float x = 0.f;

    for (int k = 0; k < GL_CASES; k++) {
        x = tx + k * (cw + 3.f);
        rempli = bornerf(f * GL_CASES - k, 0.f, 1.f);
        panneau(app, (sfFloatRect){x, cy - 5.f, cw, 10.f}, 3.f,
            avec_alpha(C_BLANC, 0.07f), avec_alpha(C_BLANC, 0.04f), C_TRANSPARENT);
        if (rempli > 0.f)
            panneau(app, (sfFloatRect){x, cy - 5.f, cw * rempli, 10.f}, 3.f * rempli,
                melanger(col, C_BLANC, 0.25f), col, C_TRANSPARENT);
    }
    lueur(app, (sfVector2f){tx + tw * f / 2.f, cy}, (sfVector2f){tw * f / 2.f + 24.f,
        18.f}, avec_alpha(col, 0.16f));
}

/*
** ╭─ glissiere_bornee ───────────────────────────────────────────────────
** │ Glissière d'une valeur : libellé, piste en cases avec un curseur
** │ en losange, puis valeur dans un cadran.
** │
** │ Se manipule à la souris ; le clavier est géré par l'écran appelant.
** │ `bornes` donne le minimum et le maximum (0..99 pour une statistique,
** │ 0..100 pour un volume).
** ╰──────────────────────────────────────────────────────────────────────
*/
int glissiere_bornee(app_t *app, glissiere_t *g, sfFloatRect r, const char *label,
    const int bornes[2], int *valeur, sfColor col, int focus)
{
    if (app == NULL)
        return 0;

    if (g == NULL || bornes == NULL || valeur == NULL)
        return 0;
    label = label != NULL ? label : "";

    float cy = r.top + r.height / 2.f;
    float tx = r.left + GL_LABEL;
    float tw = r.width - GL_LABEL - GL_VALEUR;
    int s = survole(app, r);
    float f = 0.f;
    float kx = 0.f;
    float act = 0.f;
    sfFloatRect cadran = {r.left + r.width - GL_VALEUR + 18.f, cy - 18.f,
        GL_VALEUR - 18.f, 36.f};
    char buf[8];
    char max[8];
    float etendue = (float)(bornes[1] > bornes[0] ? bornes[1] - bornes[0] : 1);

    if (s)
        app->curseur_voulu = CURSEUR_MAIN;
    if (s && app->in.clic && app->in.souris.x >= tx - 20.f)
        g->glisse = 1;
    if (!app->in.bas)
        g->glisse = 0;
    if (g->glisse)
        *valeur = bornes[0] + (int)roundf(bornerf((app->in.souris.x - tx) / tw, 0.f, 1.f)
            * etendue);
    g->survol = approcher(g->survol, s || g->glisse || focus ? 1.f : 0.f, 14.f, app->dt);
    g->affiche = approcher(g->affiche, *valeur, 16.f, app->dt);
    act = g->survol;
    f = (g->affiche - (float)bornes[0]) / etendue;
    panneau(app, (sfFloatRect){r.left - 14.f, r.top, r.width + 28.f, r.height}, 10.f,
        avec_alpha(col, 0.07f * act), avec_alpha(C_BLANC, 0.015f * act),
        avec_alpha(col, focus ? 0.35f : 0.f));
    if (focus)
        rect_arrondi(app, (sfFloatRect){r.left - 14.f, r.top + 10.f, 3.f,
            r.height - 10.f}, 0.f, col);
    texte(app, POLICE_NORMALE, label, 17, (sfVector2f){r.left + 6.f * act, cy},
        melanger(C_TEXTE_DOUX, C_TEXTE, act), ALIGNE_GAUCHE);
    piste(app, tx, tw, cy, f, col);
    kx = tx + tw * f;
    lueur(app, (sfVector2f){kx, cy}, (sfVector2f){34.f + 10.f * act, 34.f + 10.f * act},
        avec_alpha(col, 0.35f + 0.35f * act));
    losange(app, (sfVector2f){kx, cy}, 10.f + 2.f * act + 2.f * g->glisse,
        melanger(col, C_BLANC, 0.85f));
    losange(app, (sfVector2f){kx, cy}, 5.f, col);
    panneau(app, cadran, 8.f, melanger(C_SURFACE_HAUTE, col, 0.10f + 0.12f * act),
        C_CHAMP, avec_alpha(col, 0.25f + 0.45f * act));
    snprintf(buf, sizeof(buf), "%d", (int)roundf(g->affiche));
    texte_echelle(app, POLICE_GRAS, buf, 22, (sfVector2f){cadran.left + 38.f, cy},
        melanger(col, C_BLANC, 0.15f), ALIGNE_DROITE, 1.f + 0.1f * g->glisse);
    snprintf(max, sizeof(max), "/%d", bornes[1]);
    texte(app, POLICE_NORMALE, max, 12, (sfVector2f){cadran.left + 42.f, cy + 2.f},
        C_TEXTE_FAIBLE, ALIGNE_GAUCHE);
    if (focus)
        texte(app, POLICE_GRAS, "‹ ›", 15, (sfVector2f){cadran.left - 22.f, cy - 1.f},
            avec_alpha(col, 0.5f + 0.5f * sinf(app->temps * 5.f)), ALIGNE_CENTRE);
    return s && app->in.clic;
}

int glissiere(app_t *app, glissiere_t *g, sfFloatRect r, const char *label,
    int *valeur, sfColor col, int focus)
{
    static const int bornes[2] = {STAT_MIN, STAT_MAX};

    if (app == NULL || g == NULL || valeur == NULL)
        return 0;
    label = label != NULL ? label : "";

    return glissiere_bornee(app, g, r, label, bornes, valeur, col, focus);
}
