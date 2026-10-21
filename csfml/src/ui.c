/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"

#define DUREE_TOAST 2.8f

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
    sfRenderWindow_drawConvexShape(app->fenetre, app->forme, &app->additif);
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

static void supprimer_dernier(char *s)
{
    int len = my_strlen(s);

    if (len == 0)
        return;
    len--;
    while (len > 0 && ((unsigned char)s[len] & 0xC0) == 0x80)
        len--;
    s[len] = '\0';
}

static int encoder_utf8(sfUint32 cp, char *out)
{
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

/** Ajoute les caractères tapés. L'espace est refusé (le nom sert d'identifiant). */
static void saisir(app_t *app, champ_t *c, int max)
{
    char u[4];
    int n = 0;
    int len = 0;

    for (int i = 0; i < app->in.nb_texte; i++) {
        if (app->in.texte[i] <= 32) {
            c->secousse = 1.f;
            continue;
        }
        n = encoder_utf8(app->in.texte[i], u);
        len = my_strlen(c->texte);
        if (len + n > max) {
            c->secousse = 1.f;
            continue;
        }
        memcpy(c->texte + len, u, n);
        c->texte[len + n] = '\0';
        c->tape = 0.f;
    }
    if (app->in.touches[sfKeyBackspace] && c->texte[0] != '\0') {
        supprimer_dernier(c->texte);
        c->tape = 1.f;
    }
}

static int nb_caracteres(const char *s)
{
    int n = 0;

    for (int i = 0; s[i] != '\0'; i++)
        n += ((unsigned char)s[i] & 0xC0) != 0x80;
    return n;
}

static void dessiner_saisie(app_t *app, champ_t *c, float x0, float cy)
{
    char prefixe[32];
    char dernier[8];
    int nb = nb_caracteres(c->texte);
    float xd = x0 + position_caractere(app, POLICE_NORMALE, c->texte, 21, nb - 1);
    float xf = x0 + position_caractere(app, POLICE_NORMALE, c->texte, 21, nb);
    float e = sortie_rebond(progression(c->tape, 0.f, 0.24f));

    my_strcpy(prefixe, c->texte);
    supprimer_dernier(prefixe);
    my_strcpy(dernier, c->texte + my_strlen(prefixe));
    texte(app, POLICE_NORMALE, prefixe, 21, (sfVector2f){x0, cy}, C_TEXTE,
        ALIGNE_GAUCHE);
    texte_echelle(app, POLICE_NORMALE, dernier, 21, (sfVector2f){(xd + xf) / 2.f,
        cy - 8.f * (1.f - e)}, melanger(C_CYAN, C_TEXTE, e), ALIGNE_CENTRE,
        lerpf(0.4f, 1.f, e));
    c->caret_x = approcher(c->caret_x, xf + 3.f, 30.f, app->dt);
}

int champ_texte(app_t *app, champ_t *c, sfFloatRect r, const char *aide,
    int max)
{
    int s = survole(app, r);
    float dx = 0.f;
    float cy = r.top + r.height / 2.f;
    float x0 = 0.f;
    float clignote = 0.f;
    char compteur[16];

    if (s)
        app->curseur_voulu = CURSEUR_TEXTE;
    if (c->focus)
        saisir(app, c, max);
    c->focus_anim = approcher(c->focus_anim, c->focus ? 1.f : 0.f, 12.f, app->dt);
    c->secousse = approcher(c->secousse, 0.f, 5.f, app->dt);
    c->tape += app->dt;
    dx = sinf(app->temps * 60.f) * 9.f * c->secousse;
    x0 = r.left + dx + (c->marge > 0.f ? c->marge : 20.f);
    lueur(app, (sfVector2f){r.left + r.width / 2.f, cy}, (sfVector2f){r.width * 0.6f,
        r.height * 1.6f}, avec_alpha(C_CYAN, 0.10f * c->focus_anim));
    panneau(app, (sfFloatRect){r.left + dx, r.top, r.width, r.height}, 12.f,
        melanger(C_CHAMP, (sfColor){18, 30, 52, 245}, c->focus_anim), C_CHAMP,
        melanger(melanger(C_BORD, avec_alpha(C_CYAN, 0.85f), c->focus_anim),
        C_ROUGE, c->secousse * 1.5f));
    rect_arrondi(app, (sfFloatRect){r.left + dx + 12.f, r.top + r.height - 2.f,
        (r.width - 24.f) * sortie_cubique(c->focus_anim), 2.f}, 0.f,
        avec_alpha(C_CYAN, 0.9f * c->focus_anim));
    if (c->texte[0] == '\0') {
        texte(app, POLICE_NORMALE, aide, 19, (sfVector2f){x0, cy}, C_TEXTE_FAIBLE,
            ALIGNE_GAUCHE);
        c->caret_x = approcher(c->caret_x, x0 - dx, 30.f, app->dt);
    } else {
        dessiner_saisie(app, c, x0 - dx, cy);
    }
    clignote = c->tape < 0.5f ? 1.f : 0.55f + 0.45f * cosf((c->tape - 0.5f) * 5.f);
    rect_arrondi(app, (sfFloatRect){c->caret_x + dx, cy - 13.f, 2.f, 26.f}, 1.f,
        avec_alpha(C_CYAN, c->focus_anim * clignote));
    snprintf(compteur, sizeof(compteur), "%d/%d", my_strlen(c->texte), max);
    texte(app, POLICE_NORMALE, compteur, 13, (sfVector2f){r.left + dx + r.width - 18.f,
        cy}, avec_alpha(C_TEXTE_FAIBLE, 0.4f + 0.6f * c->focus_anim), ALIGNE_DROITE);
    return s && app->in.clic;
}

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

/**
 * Glissière d'une statistique : libellé, piste en cases avec un curseur en
 * losange, puis valeur dans un cadran. Se manipule à la souris ou au
 * clavier (géré par l'écran appelant).
 */
int glissiere(app_t *app, glissiere_t *g, sfFloatRect r, const char *label,
    int *valeur, sfColor col, int focus)
{
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

    if (s)
        app->curseur_voulu = CURSEUR_MAIN;
    if (s && app->in.clic && app->in.souris.x >= tx - 20.f)
        g->glisse = 1;
    if (!app->in.bas)
        g->glisse = 0;
    if (g->glisse)
        *valeur = STAT_MIN + (int)roundf(bornerf((app->in.souris.x - tx) / tw, 0.f, 1.f)
            * (STAT_MAX - STAT_MIN));
    g->survol = approcher(g->survol, s || g->glisse || focus ? 1.f : 0.f, 14.f, app->dt);
    g->affiche = approcher(g->affiche, *valeur, 16.f, app->dt);
    act = g->survol;
    f = (g->affiche - STAT_MIN) / (STAT_MAX - STAT_MIN);
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
    texte(app, POLICE_NORMALE, "/99", 12, (sfVector2f){cadran.left + 42.f, cy + 2.f},
        C_TEXTE_FAIBLE, ALIGNE_GAUCHE);
    if (focus)
        texte(app, POLICE_GRAS, "‹ ›", 15, (sfVector2f){cadran.left - 22.f, cy - 1.f},
            avec_alpha(col, 0.5f + 0.5f * sinf(app->temps * 5.f)), ALIGNE_CENTRE);
    return s && app->in.clic;
}

/** Pastille (badge) centrée sur `pos`. Renvoie sa largeur. */
float pastille(app_t *app, const char *s, sfVector2f pos, sfColor c, int plein)
{
    float w = 0.f;

    app->espacement = 1.7f;
    w = largeur_texte(app, POLICE_GRAS, s, 12) + 26.f;
    rect_arrondi_bord(app, (sfFloatRect){pos.x - w / 2.f, pos.y - 13.f, w, 26.f}, 13.f,
        plein ? c : avec_alpha(c, 0.14f), plein ? C_TRANSPARENT : avec_alpha(c, 0.45f));
    texte(app, POLICE_GRAS, s, 12, pos, plein ? C_BLANC : melanger(c, C_BLANC, 0.3f),
        ALIGNE_CENTRE);
    app->espacement = 0.f;
    return w;
}

void notifier(app_t *app, const char *icone, const char *message, sfColor c)
{
    toast_t *t = &app->toasts[0];

    memmove(&app->toasts[1], &app->toasts[0], sizeof(toast_t) * (MAX_TOASTS - 1));
    snprintf(t->message, sizeof(t->message), "%s", message);
    snprintf(t->icone, sizeof(t->icone), "%s", icone);
    t->couleur = c;
    t->t = 0.f;
    t->y = -50.f;
    t->actif = 1;
}

static void dessiner_toast(app_t *app, toast_t *t)
{
    float f = 1.f - progression(t->t, DUREE_TOAST - 0.35f, 0.35f);
    float w = largeur_texte(app, POLICE_NORMALE, t->message, 16) + 80.f;
    sfFloatRect r = {LARGEUR / 2.f - w / 2.f, t->y - 14.f * (1.f - f), w, 46.f};
    float cy = r.top + r.height / 2.f;

    app->alpha = fminf(progression(t->t, 0.f, 0.2f), f);
    ombre_biseau(app, r, 12.f, 1.2f);
    panneau(app, r, 12.f, melanger((sfColor){24, 28, 50, 250}, t->couleur, 0.12f),
        (sfColor){16, 19, 36, 250}, avec_alpha(t->couleur, 0.55f));
    lueur(app, (sfVector2f){r.left + 26.f, cy}, (sfVector2f){40.f, 40.f},
        avec_alpha(t->couleur, 0.3f));
    cercle(app, (sfVector2f){r.left + 26.f, cy}, 14.f, avec_alpha(t->couleur, 0.2f),
        C_TRANSPARENT, 0.f);
    texte(app, POLICE_SYMBOLES, t->icone, 14, (sfVector2f){r.left + 26.f, cy},
        t->couleur, ALIGNE_CENTRE);
    texte(app, POLICE_NORMALE, t->message, 16, (sfVector2f){r.left + 50.f, cy},
        C_TEXTE, ALIGNE_GAUCHE);
    rect_arrondi(app, (sfFloatRect){r.left + 12.f, r.top + r.height - 2.f,
        (w - 24.f) * (1.f - t->t / DUREE_TOAST), 2.f}, 0.f,
        avec_alpha(t->couleur, 0.8f));
    app->alpha = 1.f;
}

void dessiner_toasts(app_t *app)
{
    toast_t *t = NULL;

    for (int i = MAX_TOASTS - 1; i >= 0; i--) {
        t = &app->toasts[i];
        if (!t->actif)
            continue;
        t->t += app->dt;
        if (t->t > DUREE_TOAST) {
            t->actif = 0;
            continue;
        }
        t->y = approcher(t->y, 24.f + i * 56.f, 13.f, app->dt);
        dessiner_toast(app, t);
    }
}

/**
 * Bouton retour, surtitre (facultatif), titre et sous-titre communs aux
 * écrans secondaires. Le surtitre porte une information de contexte réelle
 * (ex. « RECRUE 03 / 10 »).
 */
void en_tete_surtitre(app_t *app, bouton_t *retour, const char *surtitre,
    const char *titre, const char *sous_titre)
{
    float t = app->t_scene;
    float e1 = sortie_cubique(progression(t, 0.f, 0.5f));
    float e2 = sortie_cubique(progression(t, 0.08f, 0.5f));
    float e3 = sortie_cubique(progression(t, 0.16f, 0.5f));
    float a = app->alpha;
    float lt = 0.f;

    app->alpha = a * e1;
    if (bouton_retour(app, retour, (sfVector2f){64.f - 20.f * (1.f - e1), 30.f}))
        changer_scene(app, SCENE_MENU);
    app->alpha = a * e2;
    if (surtitre) {
        losange(app, (sfVector2f){68.f, 98.f}, 4.f, C_ROUGE);
        etiquette(app, surtitre, (sfVector2f){82.f, 98.f}, C_ROUGE, ALIGNE_GAUCHE);
    }
    app->espacement = 0.6f;
    lt = texte(app, POLICE_GRAS, titre, 36, (sfVector2f){64.f, 130.f + 16.f * (1.f - e2)},
        C_TEXTE, ALIGNE_GAUCHE);
    app->espacement = 0.f;
    rect_arrondi(app, (sfFloatRect){64.f + lt + 16.f, 128.f + 16.f * (1.f - e2),
        120.f * e2, 2.f}, 0.f, avec_alpha(C_ROUGE, 0.6f));
    app->alpha = a * e3;
    texte(app, POLICE_NORMALE, sous_titre, 15, (sfVector2f){64.f,
        164.f + 16.f * (1.f - e3)}, C_TEXTE_DOUX, ALIGNE_GAUCHE);
    app->alpha = a;
}

void en_tete(app_t *app, bouton_t *retour, const char *titre,
    const char *sous_titre)
{
    en_tete_surtitre(app, retour, NULL, titre, sous_titre);
}
