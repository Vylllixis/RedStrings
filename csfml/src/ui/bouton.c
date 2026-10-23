/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ bouton.c ───────────────────────────────────────────────────────────
** │ But         Boutons : survol, appui, reflet, et leurs variantes
** │             (pleine, de retour, ronde).
** │ Contenu     survole, interaction, bouton, couper_x, reflet,
** │             bouton_plein, bouton_retour, bouton_rond
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "gui_ui.h"

int survole(app_t *app, sfFloatRect r)
{
    return !app->souris_bloquee
        && sfFloatRect_contains(&r, app->in.souris.x, app->in.souris.y);
}

/** Met à jour les animations de survol / appui et renvoie 1 si cliqué. */
static int interaction(app_t *app, bouton_t *b, sfFloatRect r, int actif)
{
    int s = survole(app, r);

    if (s)
        app->curseur_voulu = CURSEUR_MAIN;
    if (s && actif && b->survol < 0.02f && !app->souris_bloquee)
        son(app, SON_UI_SURVOL);
    b->survol = approcher(b->survol, s && actif ? 1.f : 0.f, 14.f, app->dt);
    b->appui = approcher(b->appui, s && app->in.bas ? 1.f : 0.f, 30.f, app->dt);
    return s && app->in.clic;
}

int bouton(app_t *app, bouton_t *b, sfFloatRect r, const char *label,
    const char *touche, const char *info, sfColor accent, int actif)
{
    int clic = interaction(app, b, r, actif);
    float h = b->survol;
    float a = app->alpha;
    sfFloatRect rr = {r.left + 6.f * h, r.top + 2.f * b->appui, r.width, r.height};
    float cy = rr.top + rr.height / 2.f;
    float x = rr.left + 22.f;
    float lb = 0.f;

    if (!actif)
        app->alpha *= 0.45f;
    lueur(app, (sfVector2f){rr.left + rr.width / 2.f, cy},
        (sfVector2f){rr.width * 0.75f, rr.height * 1.6f}, avec_alpha(accent, 0.14f * h));
    ombre_biseau(app, rr, 14.f, 0.8f);
    panneau(app, rr, 14.f, melanger(C_SURFACE_HAUTE, melanger(C_SURFACE_HAUTE, accent,
        0.25f), h), C_SURFACE, melanger(C_BORD, avec_alpha(accent, 0.8f), h));
    rect_arrondi(app, (sfFloatRect){rr.left, rr.top + 14.f, 3.f, rr.height - 14.f},
        0.f, avec_alpha(accent, 0.35f + 0.65f * h));
    if (touche) {
        lb = largeur_touche(app, touche);
        touche_clavier(app, touche, (sfVector2f){x + lb / 2.f, cy}, accent, h);
        x += lb + 14.f;
    }
    texte(app, POLICE_GRAS, label, 19, (sfVector2f){x, cy},
        melanger(C_TEXTE_DOUX, C_TEXTE, 0.55f + 0.45f * h), ALIGNE_GAUCHE);
    if (info)
        texte(app, POLICE_NORMALE, info, 15, (sfVector2f){rr.left + rr.width - 52.f, cy},
            C_TEXTE_FAIBLE, ALIGNE_DROITE);
    texte(app, POLICE_GRAS, "›", 28, (sfVector2f){rr.left + rr.width - 28.f + 5.f * h,
        cy - 2.f}, melanger(C_TEXTE_FAIBLE, accent, h), ALIGNE_CENTRE);
    app->alpha = a;
    return clic;
}

/** Garde la partie d'un polygone située d'un côté de la droite x = lim. */
static int couper_x(const sfVector2f *in, int n, sfVector2f *out, float lim,
    int garder_droite)
{
    sfVector2f a;
    sfVector2f b;
    int m = 0;
    int ina = 0;
    int inb = 0;
    float t = 0.f;

    for (int i = 0; i < n; i++) {
        a = in[i];
        b = in[(i + 1) % n];
        ina = garder_droite ? a.x >= lim : a.x <= lim;
        inb = garder_droite ? b.x >= lim : b.x <= lim;
        if (ina)
            out[m++] = a;
        if (ina != inb) {
            t = (lim - a.x) / (b.x - a.x);
            out[m++] = (sfVector2f){lim, a.y + (b.y - a.y) * t};
        }
    }
    return m;
}

/** Reflet lumineux qui balaie périodiquement un bouton. */
static void reflet(app_t *app, sfFloatRect r, float rayon)
{
    float p = fmodf(app->temps, 3.4f) / 1.1f;
    float x0 = r.left + rayon;
    float x1 = r.left + r.width - rayon;
    float bx = 0.f;
    sfVector2f quad[4];
    sfVector2f tmp[8];
    sfVector2f res[8];
    int n = 0;

    if (p > 1.f)
        return;
    bx = lerpf(x0 - 80.f, x1 + 10.f, entree_sortie_cubique(p));
    quad[0] = (sfVector2f){bx + 30.f, r.top};
    quad[1] = (sfVector2f){bx + 76.f, r.top};
    quad[2] = (sfVector2f){bx + 46.f, r.top + r.height};
    quad[3] = (sfVector2f){bx, r.top + r.height};
    n = couper_x(quad, 4, tmp, x0, 1);
    n = couper_x(tmp, n, res, x1, 0);
    if (n < 3)
        return;
    sfConvexShape_setPointCount(app->forme, n);
    for (int i = 0; i < n; i++)
        sfConvexShape_setPoint(app->forme, i, res[i]);
    sfConvexShape_setFillColor(app->forme, avec_alpha(C_BLANC, 0.16f * app->alpha));
    sfConvexShape_setOutlineThickness(app->forme, 0.f);
    cible_convexe(app, app->forme, &app->additif);
}

int bouton_plein(app_t *app, bouton_t *b, sfFloatRect r, const char *label,
    const char *touche, sfColor accent, int actif)
{
    int clic = interaction(app, b, r, actif);
    float h = b->survol;
    float pulse = 0.5f + 0.5f * sinf(app->temps * 3.f);
    sfFloatRect rr = {r.left, r.top - 3.f * h + 2.f * b->appui, r.width, r.height};
    sfVector2f c = {rr.left + rr.width / 2.f, rr.top + rr.height / 2.f};
    float lt = largeur_texte(app, POLICE_GRAS, label, 20);
    float lk = touche ? largeur_touche(app, touche) : 0.f;
    float x = c.x - (lt + (touche ? lk + 16.f : 0.f)) / 2.f;
    sfColor haut = melanger(accent, C_BLANC, 0.18f + 0.12f * h);
    sfColor bas = melanger(accent, C_NOIR, 0.28f);

    if (!actif) {
        haut = melanger(C_SURFACE_HAUTE, accent, 0.12f);
        bas = C_SURFACE;
    }
    if (actif)
        lueur(app, (sfVector2f){c.x, c.y + 8.f}, (sfVector2f){rr.width * 0.7f,
            rr.height * 1.5f}, avec_alpha(accent, 0.20f + 0.10f * pulse + 0.2f * h));
    ombre_biseau(app, rr, 18.f, 1.f);
    panneau(app, rr, 18.f, haut, bas, avec_alpha(actif ? melanger(accent, C_BLANC, 0.5f)
        : C_BORD, actif ? 0.9f : 1.f));
    panneau(app, (sfFloatRect){rr.left + 18.f, rr.top + 2.f, rr.width - 36.f, 2.f}, 0.f,
        avec_alpha(C_BLANC, actif ? 0.35f : 0.06f), avec_alpha(C_BLANC, 0.f),
        C_TRANSPARENT);
    if (actif) {
        crochets(app, (sfFloatRect){rr.left - 5.f, rr.top - 5.f, rr.width + 10.f,
            rr.height + 10.f}, 12.f + 6.f * h, avec_alpha(melanger(accent, C_BLANC, 0.4f),
            0.5f + 0.5f * h));
        reflet(app, rr, 18.f);
    }
    app->espacement = 1.25f;
    texte(app, POLICE_GRAS, label, 20, (sfVector2f){x, c.y},
        actif ? C_BLANC : C_TEXTE_FAIBLE, ALIGNE_GAUCHE);
    app->espacement = 0.f;
    if (touche)
        touche_clavier(app, touche, (sfVector2f){x + lt + 16.f + lk / 2.f, c.y},
            actif ? C_BLANC : C_TEXTE_FAIBLE, actif ? 0.85f : 0.f);
    return clic;
}

int bouton_retour(app_t *app, bouton_t *b, sfVector2f pos)
{
    sfFloatRect r = {pos.x, pos.y, 126.f, 40.f};
    int clic = interaction(app, b, r, 1);
    float h = b->survol;
    sfFloatRect rr = {r.left - 4.f * h, r.top, r.width, r.height};
    float cy = rr.top + rr.height / 2.f;

    panneau(app, rr, 10.f, melanger(avec_alpha(C_SURFACE_HAUTE, 0.7f), C_SURFACE_HAUTE, h),
        avec_alpha(C_SURFACE, 0.7f), melanger(C_BORD, avec_alpha(C_CYAN, 0.7f), h));
    texte(app, POLICE_GRAS, "‹", 26, (sfVector2f){rr.left + 22.f - 3.f * h, cy - 2.f},
        melanger(C_TEXTE_DOUX, C_CYAN, h), ALIGNE_CENTRE);
    app->espacement = 1.6f;
    texte(app, POLICE_GRAS, "RETOUR", 13, (sfVector2f){rr.left + 40.f, cy},
        melanger(C_TEXTE_DOUX, C_TEXTE, h), ALIGNE_GAUCHE);
    app->espacement = 0.f;
    touche_clavier(app, "Échap", (sfVector2f){r.left + r.width + 34.f, cy},
        C_CYAN, 0.f);
    return clic;
}

int bouton_rond(app_t *app, bouton_t *b, sfVector2f centre, const char *sym)
{
    sfFloatRect r = {centre.x - 24.f, centre.y - 24.f, 48.f, 48.f};
    int clic = interaction(app, b, r, 1);
    float h = b->survol;

    lueur(app, centre, (sfVector2f){64.f, 64.f}, avec_alpha(C_CYAN, 0.2f * h));
    cercle(app, centre, 22.f + 2.f * h, melanger(C_SURFACE, C_SURFACE_HAUTE, h),
        melanger(C_BORD, avec_alpha(C_CYAN, 0.7f), h), 1.5f);
    texte(app, POLICE_GRAS, sym, 26, (sfVector2f){centre.x, centre.y - 1.f},
        melanger(C_TEXTE_DOUX, C_TEXTE, h), ALIGNE_CENTRE);
    return clic;
}
