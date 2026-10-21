/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <math.h>
#include <string.h>
#include "gui.h"

#define POINTS_COIN 8
#define MAX_POINTS_LIGNE 160

void rectArrondi(app_t *app, sfFloatRect r, float rayon, sfColor fond)
{
    rectArrondiBord(app, r, rayon, fond, C_TRANSPARENT);
}

void rectArrondiBord(app_t *app, sfFloatRect r, float rayon, sfColor fond,
    sfColor bord)
{
    float rc = bornerf(rayon, 0.f, fminf(r.width, r.height) / 2.f);
    const sfVector2f centres[4] = {
        {r.left + r.width - rc, r.top + rc},
        {r.left + r.width - rc, r.top + r.height - rc},
        {r.left + rc, r.top + r.height - rc},
        {r.left + rc, r.top + rc}
    };
    float angle = 0.f;
    int k = 0;

    rayon = rc;
    sfConvexShape_setPointCount(app->forme, 4 * POINTS_COIN);
    for (int c = 0; c < 4; c++) {
        for (int i = 0; i < POINTS_COIN; i++) {
            angle = (c * 90.f - 90.f + i * 90.f / (POINTS_COIN - 1)) * PI / 180.f;
            sfConvexShape_setPoint(app->forme, k++, (sfVector2f){
                centres[c].x + cosf(angle) * rayon,
                centres[c].y + sinf(angle) * rayon});
        }
    }
    sfConvexShape_setFillColor(app->forme, avecAlpha(fond, app->alpha));
    sfConvexShape_setOutlineColor(app->forme, avecAlpha(bord, app->alpha));
    sfConvexShape_setOutlineThickness(app->forme, bord.a > 0 ? -1.5f : 0.f);
    sfRenderWindow_drawConvexShape(app->fenetre, app->forme, NULL);
}

/** Ombre portée douce : plusieurs couches de plus en plus larges. */
void ombre(app_t *app, sfFloatRect r, float rayon, float force)
{
    float e = 0.f;

    for (int i = 6; i >= 1; i--) {
        e = i * 3.f;
        rectArrondi(app, (sfFloatRect){r.left - e, r.top - e + 6.f + i,
            r.width + 2.f * e, r.height + 2.f * e}, rayon + e,
            avecAlpha(C_NOIR, force * 0.075f * (1.f - i / 7.f)));
    }
}

/** Halo lumineux (dégradé radial en mélange additif). */
void lueur(app_t *app, sfVector2f centre, sfVector2f rayon, sfColor c)
{
    if (c.a == 0 || app->alpha <= 0.f)
        return;
    sfSprite_setTexture(app->sprite, app->texLueur, sfTrue);
    sfSprite_setOrigin(app->sprite, (sfVector2f){64.f, 64.f});
    sfSprite_setPosition(app->sprite, centre);
    sfSprite_setRotation(app->sprite, 0.f);
    sfSprite_setScale(app->sprite, (sfVector2f){rayon.x / 64.f,
        rayon.y / 64.f});
    sfSprite_setColor(app->sprite, avecAlpha(c, app->alpha));
    sfRenderWindow_drawSprite(app->fenetre, app->sprite, &app->additif);
}

void cercle(app_t *app, sfVector2f centre, float r, sfColor fond,
    sfColor bord, float epaisseur)
{
    sfCircleShape_setRadius(app->cercle, r);
    sfCircleShape_setPointCount(app->cercle, r > 24.f ? 64 : 32);
    sfCircleShape_setOrigin(app->cercle, (sfVector2f){r, r});
    sfCircleShape_setPosition(app->cercle, centre);
    sfCircleShape_setFillColor(app->cercle, avecAlpha(fond, app->alpha));
    sfCircleShape_setOutlineColor(app->cercle, avecAlpha(bord, app->alpha));
    sfCircleShape_setOutlineThickness(app->cercle, -epaisseur);
    sfRenderWindow_drawCircleShape(app->fenetre, app->cercle, NULL);
}

/** Polygone en coordonnées unitaires, mis à l'échelle et tourné. */
void polygone(app_t *app, const float *pts, int n, sfVector2f centre,
    float taille, float angle, sfColor c)
{
    float ca = cosf(angle);
    float sa = sinf(angle);

    sfConvexShape_setPointCount(app->forme, n);
    for (int i = 0; i < n; i++) {
        sfConvexShape_setPoint(app->forme, i, (sfVector2f){
            centre.x + (pts[2 * i] * ca - pts[2 * i + 1] * sa) * taille,
            centre.y + (pts[2 * i] * sa + pts[2 * i + 1] * ca) * taille});
    }
    sfConvexShape_setFillColor(app->forme, avecAlpha(c, app->alpha));
    sfConvexShape_setOutlineThickness(app->forme, 0.f);
    sfRenderWindow_drawConvexShape(app->fenetre, app->forme, NULL);
}

/** Polyligne épaisse construite en bande de triangles. */
void ligneEpaisse(app_t *app, const sfVector2f *pts, int n, float epaisseur,
    sfColor c, int additif)
{
    sfVertex v[MAX_POINTS_LIGNE * 2];
    sfVector2f d;
    float len = 0.f;
    int a = 0;
    int b = 0;

    if (n > MAX_POINTS_LIGNE)
        n = MAX_POINTS_LIGNE;
    c = avecAlpha(c, app->alpha);
    for (int i = 0; i < n; i++) {
        a = i > 0 ? i - 1 : i;
        b = i < n - 1 ? i + 1 : i;
        d = (sfVector2f){pts[b].x - pts[a].x, pts[b].y - pts[a].y};
        len = sqrtf(d.x * d.x + d.y * d.y);
        if (len < 0.0001f)
            len = 1.f;
        d = (sfVector2f){-d.y / len * epaisseur / 2.f, d.x / len * epaisseur / 2.f};
        v[2 * i] = (sfVertex){{pts[i].x + d.x, pts[i].y + d.y}, c, {0, 0}};
        v[2 * i + 1] = (sfVertex){{pts[i].x - d.x, pts[i].y - d.y}, c, {0, 0}};
    }
    sfRenderWindow_drawPrimitives(app->fenetre, v, n * 2, sfTriangleStrip,
        additif ? &app->additif : NULL);
}

static int utf8VersUtf32(const char *s, sfUint32 *dst, int max)
{
    const unsigned char *u = (const unsigned char *)s;
    int n = 0;

    while (*u && n < max - 1) {
        if (*u < 0x80) {
            dst[n++] = *u++;
        } else if ((*u & 0xE0) == 0xC0 && u[1]) {
            dst[n++] = ((u[0] & 0x1F) << 6) | (u[1] & 0x3F);
            u += 2;
        } else if ((*u & 0xF0) == 0xE0 && u[1] && u[2]) {
            dst[n++] = ((u[0] & 0x0F) << 12) | ((u[1] & 0x3F) << 6)
                | (u[2] & 0x3F);
            u += 3;
        } else if ((*u & 0xF8) == 0xF0 && u[1] && u[2] && u[3]) {
            dst[n++] = ((u[0] & 0x07) << 18) | ((u[1] & 0x3F) << 12)
                | ((u[2] & 0x3F) << 6) | (u[3] & 0x3F);
            u += 4;
        } else {
            u++;
        }
    }
    dst[n] = 0;
    return n;
}

static void preparerTexte(app_t *app, police_t p, const char *s,
    unsigned int taille)
{
    sfUint32 buf[256];

    utf8VersUtf32(s, buf, 256);
    sfText_setFont(app->texte, app->polices[p]);
    sfText_setCharacterSize(app->texte, taille);
    sfText_setLetterSpacing(app->texte, app->espacement > 0.f
        ? app->espacement : 1.f);
    sfText_setUnicodeString(app->texte, buf);
}

float largeurTexte(app_t *app, police_t p, const char *s, unsigned int taille)
{
    preparerTexte(app, p, s, taille);
    return sfText_getLocalBounds(app->texte).width;
}

/** Abscisse du caractère numéro `index` (en points de code). */
float positionCaractere(app_t *app, police_t p, const char *s,
    unsigned int taille, int index)
{
    preparerTexte(app, p, s, taille);
    sfText_setPosition(app->texte, (sfVector2f){0.f, 0.f});
    sfText_setOrigin(app->texte, (sfVector2f){0.f, 0.f});
    sfText_setScale(app->texte, (sfVector2f){1.f, 1.f});
    sfText_setRotation(app->texte, 0.f);
    return sfText_findCharacterPos(app->texte, index).x;
}

/**
 * Dessine un texte UTF-8. `pos.y` désigne le centre vertical des majuscules,
 * ce qui permet d'aligner proprement des textes de tailles différentes.
 */
float texteEchelle(app_t *app, police_t p, const char *s, unsigned int taille,
    sfVector2f pos, sfColor c, alignement_t a, float echelle)
{
    sfFloatRect b;
    sfGlyph h = sfFont_getGlyph(app->polices[p], 'H', taille, sfFalse, 0.f);
    float ox = 0.f;

    preparerTexte(app, p, s, taille);
    b = sfText_getLocalBounds(app->texte);
    if (a == ALIGNE_CENTRE)
        ox = b.left + b.width / 2.f;
    else if (a == ALIGNE_DROITE)
        ox = b.left + b.width;
    sfText_setOrigin(app->texte, (sfVector2f){ox,
        taille + h.bounds.top + h.bounds.height / 2.f});
    sfText_setPosition(app->texte, pos);
    sfText_setScale(app->texte, (sfVector2f){echelle, echelle});
    sfText_setRotation(app->texte, app->rotation);
    sfText_setFillColor(app->texte, avecAlpha(c, app->alpha));
    sfText_setOutlineThickness(app->texte, app->contour);
    sfText_setOutlineColor(app->texte, avecAlpha((sfColor){8, 8, 18, 220},
        app->alpha * (c.a / 255.f)));
    sfRenderWindow_drawText(app->fenetre, app->texte, NULL);
    return b.width * echelle;
}

float texte(app_t *app, police_t p, const char *s, unsigned int taille,
    sfVector2f pos, sfColor c, alignement_t a)
{
    return texteEchelle(app, p, s, taille, pos, c, a, 1.f);
}

/** Copie `src` dans `dst` en l'abrégeant avec « … » s'il est trop large. */
void tronquer(app_t *app, police_t p, unsigned int taille, const char *src,
    float max, char *dst)
{
    int len = my_strlen(src);

    my_strcpy(dst, src);
    if (largeurTexte(app, p, dst, taille) <= max)
        return;
    while (len > 0) {
        len--;
        while (len > 0 && ((unsigned char)src[len] & 0xC0) == 0x80)
            len--;
        memcpy(dst, src, len);
        my_strcpy(dst + len, "…");
        if (largeurTexte(app, p, dst, taille) <= max)
            return;
    }
}

/** Petit libellé en capitales espacées. */
float etiquette(app_t *app, const char *s, sfVector2f pos, sfColor c,
    alignement_t a)
{
    float l = 0.f;

    app->espacement = 2.2f;
    l = texte(app, POLICE_GRAS, s, 12, pos, c, a);
    app->espacement = 0.f;
    return l;
}

static void degrade(app_t *app)
{
    float t = app->temps * 0.15f;
    sfColor haut = {(sfUint8)(10 + 3 * sinf(t)), 12, (sfUint8)(28 + 4 * cosf(t)), 255};
    sfColor bas = {(sfUint8)(18 + 4 * cosf(t * 1.3f)), 10, 30, 255};
    sfVertex v[4] = {
        {{0, 0}, haut, {0, 0}},
        {{LARGEUR, 0}, melanger(haut, bas, 0.3f), {0, 0}},
        {{0, HAUTEUR}, bas, {0, 0}},
        {{LARGEUR, HAUTEUR}, melanger(bas, haut, 0.4f), {0, 0}}
    };

    sfRenderWindow_drawPrimitives(app->fenetre, v, 4, sfTriangleStrip, NULL);
}

static void aurores(app_t *app)
{
    float t = app->temps;

    lueur(app, (sfVector2f){340 + 180 * sinf(t * 0.13f), 220 + 90 * cosf(t * 0.17f)},
        (sfVector2f){620, 420}, avecAlpha(C_CYAN, 0.10f));
    lueur(app, (sfVector2f){980 + 160 * cosf(t * 0.11f), 520 + 80 * sinf(t * 0.19f)},
        (sfVector2f){640, 440}, avecAlpha(C_MAGE, 0.08f));
    lueur(app, (sfVector2f){700 + 260 * sinf(t * 0.07f), 380 + 60 * sinf(t * 0.23f)},
        (sfVector2f){560, 300}, avecAlpha(C_ROUGE, 0.07f));
}

/** Les « fils rouges » qui ondulent en arrière-plan. */
static void filsRouges(app_t *app)
{
    sfVector2f pts[90];
    float t = app->temps;
    float x = 0.f;
    float base = 0.f;

    for (int f = 0; f < 3; f++) {
        base = 190.f + f * 190.f;
        for (int i = 0; i < 90; i++) {
            x = -40.f + i * (LARGEUR + 80.f) / 89.f;
            pts[i] = (sfVector2f){x, base
                + sinf(x * 0.0042f + t * (0.35f + f * 0.08f) + f * 2.1f) * (46.f + f * 10.f)
                + sinf(x * 0.011f - t * 0.6f + f) * 14.f};
        }
        ligneEpaisse(app, pts, 90, 14.f, avecAlpha(C_ROUGE, 0.035f), 1);
        ligneEpaisse(app, pts, 90, 5.f, avecAlpha(C_ROUGE, 0.07f), 1);
        ligneEpaisse(app, pts, 90, 1.6f, avecAlpha(C_ROUGE, 0.30f), 1);
    }
}

static void poussieres(app_t *app)
{
    poussiere_t *p = NULL;
    float scintille = 0.f;

    for (int i = 0; i < NB_POUSSIERES; i++) {
        p = &app->poussieres[i];
        p->y -= p->vitesse * app->dt;
        p->x += sinf(app->temps * 0.5f + p->phase) * 8.f * app->dt;
        if (p->y < -10.f) {
            p->y = HAUTEUR + 10.f;
            p->x = aleatoire(0.f, LARGEUR);
        }
        scintille = 0.35f + 0.65f * (0.5f + 0.5f * sinf(app->temps * 1.7f + p->phase * 3.f));
        lueur(app, (sfVector2f){p->x, p->y}, (sfVector2f){p->taille * 3.f,
            p->taille * 3.f}, avecAlpha(i % 3 == 0 ? C_ROUGE : C_CYAN,
            0.35f * scintille));
    }
}

void dessinerFond(app_t *app)
{
    degrade(app);
    aurores(app);
    filsRouges(app);
    poussieres(app);
    sfSprite_setTexture(app->sprite, app->texVignette, sfTrue);
    sfSprite_setOrigin(app->sprite, (sfVector2f){0, 0});
    sfSprite_setPosition(app->sprite, (sfVector2f){0, 0});
    sfSprite_setScale(app->sprite, (sfVector2f){LARGEUR / 256.f, HAUTEUR / 256.f});
    sfSprite_setColor(app->sprite, C_BLANC);
    sfRenderWindow_drawSprite(app->fenetre, app->sprite, NULL);
}
