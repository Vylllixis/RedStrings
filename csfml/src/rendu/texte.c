/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ texte.c ────────────────────────────────────────────────────────────
** │ But         Le texte : conversion UTF-8, mesure, dessin avec contour
** │             et espacement, troncature.
** │ Contenu     facteur_texte, preparer_texte, largeur_texte,
** │             position_caractere, texte_echelle, texte, tronquer,
** │             etiquette
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_dessin.h"
#include "gui_types.h"
#include "gui_utf8.h"

/* Un texte plus long que cela est coupé : les libellés de l'interface sont courts. */
#define TAILLE_TEXTE_MAX 256

/*
** ╭─ facteur_texte ──────────────────────────────────────────────────────
** │ Le texte est tramé à sa taille réelle à l'écran (échelle de
** │ l'interface x zoom de la zone), puis réduit d'autant : il reste net à
** │ toute taille de fenêtre.
** │
** │ Dans une texture (faces des cartes), le facteur vaut 1 : le texte est
** │ tramé tel quel.
** ╰──────────────────────────────────────────────────────────────────────
*/
static float facteur_texte(const app_t *app)
{
    return app->cible != NULL ? 1.f : app->echelle * app->zoom;
}

/** Prépare le texte et renvoie le rapport entre sa taille tramée et `taille`. */
static float preparer_texte(app_t *app, police_t p, const char *s,
    unsigned int taille)
{
    uint32_t buf[TAILLE_TEXTE_MAX];
    unsigned int trame = (unsigned int)fmaxf(1.f, roundf(taille * facteur_texte(app)));

    utf8_decoder(s, buf, TAILLE_TEXTE_MAX);
    sfText_setFont(app->texte, app->polices[p]);
    sfText_setCharacterSize(app->texte, trame);
    sfText_setLetterSpacing(app->texte, app->espacement > 0.f
        ? app->espacement : 1.f);
    sfText_setUnicodeString(app->texte, buf);
    return taille > 0 ? (float)trame / taille : 1.f;
}

float largeur_texte(app_t *app, police_t p, const char *s, unsigned int taille)
{
    float r = preparer_texte(app, p, s, taille);

    return sfText_getLocalBounds(app->texte).width / r;
}

/** Abscisse du caractère numéro `index` (en points de code). */
float position_caractere(app_t *app, police_t p, const char *s,
    unsigned int taille, int index)
{
    float r = preparer_texte(app, p, s, taille);

    sfText_setPosition(app->texte, (sfVector2f){0.f, 0.f});
    sfText_setOrigin(app->texte, (sfVector2f){0.f, 0.f});
    sfText_setScale(app->texte, (sfVector2f){1.f, 1.f});
    sfText_setRotation(app->texte, 0.f);
    return sfText_findCharacterPos(app->texte, index).x / r;
}

/**
 * Dessine un texte UTF-8. `pos.y` désigne le centre vertical des majuscules,
 * ce qui permet d'aligner proprement des textes de tailles différentes.
 */
float texte_echelle(app_t *app, police_t p, const char *s, unsigned int taille,
    sfVector2f pos, sfColor c, alignement_t a, float echelle)
{
    sfFloatRect b;
    sfGlyph h;
    float ox = 0.f;
    float r = preparer_texte(app, p, s, taille);
    unsigned int trame = sfText_getCharacterSize(app->texte);

    h = sfFont_getGlyph(app->polices[p], 'H', trame, sfFalse, 0.f);
    b = sfText_getLocalBounds(app->texte);
    if (a == ALIGNE_CENTRE)
        ox = b.left + b.width / 2.f;
    else if (a == ALIGNE_DROITE)
        ox = b.left + b.width;
    sfText_setOrigin(app->texte, (sfVector2f){ox,
        trame + h.bounds.top + h.bounds.height / 2.f});
    sfText_setPosition(app->texte, pos);
    sfText_setScale(app->texte, (sfVector2f){echelle / r, echelle / r});
    sfText_setRotation(app->texte, app->rotation);
    sfText_setFillColor(app->texte, avec_alpha(c, app->alpha));
    sfText_setOutlineThickness(app->texte, app->contour * r);
    sfText_setOutlineColor(app->texte, avec_alpha((sfColor){8, 8, 18, 220},
        app->alpha * (c.a / 255.f)));
    cible_texte(app, app->texte, NULL);
    return b.width / r * echelle;
}

float texte(app_t *app, police_t p, const char *s, unsigned int taille,
    sfVector2f pos, sfColor c, alignement_t a)
{
    return texte_echelle(app, p, s, taille, pos, c, a, 1.f);
}

/*
** ╭─ tronquer ───────────────────────────────────────────────────────────
** │ Copie `src` dans `dst` (`taille_dst` octets, terminateur compris),
** │ abrégée avec « … » si le texte est trop large.
** │
** │ 1. copie complète ; si elle tient dans `max`, c'est fini
** │ 2. sinon retire un caractère à la fois par la fin (en reculant sur
** │    les octets de suite UTF-8 : jamais de caractère coupé en deux) et
** │    ajoute « … »
** │ 3. s'arrête dès que le texte tient
** │ snprintf() borne toutes les écritures : jamais au-delà de
** │ `taille_dst`.
** ╰──────────────────────────────────────────────────────────────────────
*/
void tronquer(app_t *app, police_t p, unsigned int taille, const char *src,
    float max, char *dst, size_t taille_dst)
{
    size_t len = strlen(src);

    if (taille_dst == 0)
        return;
    snprintf(dst, taille_dst, "%s", src);
    if (largeur_texte(app, p, dst, taille) <= max)
        return;
    while (len > 0) {
        len--;
        while (len > 0 && ((unsigned char)src[len] & 0xC0) == 0x80)
            len--;
        snprintf(dst, taille_dst, "%.*s…", (int)len, src);
        if (largeur_texte(app, p, dst, taille) <= max)
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
