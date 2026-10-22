/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ face.c ─────────────────────────────────────────────────────────────
** │ But         Face des cartes, dessinée une seule fois dans une texture
** │             au lancement (voir ressources.c). Le titre est le nom de
** │             la classe RedStrings : le nom officiel de la carte
** │             n'apparaît jamais.
** │ Contenu     yugioh_degrade, grain, etoile_niveau, titre, niveau,
** │             illustration, mod_texte, encart, dessiner_face
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include <stdio.h>
#include "personnage.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_emblemes.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "gui_yugioh.h"
#include "yugioh_interne.h"

/* Assez de place sur la carte pour écrire « SOIN » en entier. */
static const char *STATS[NB_STATS] = {"PV", "ATQ", "DEF", "SOIN"};

#define ENCRE_PARCHEMIN ((sfColor){28, 20, 12, 255})

/* Zones de la face, en pixels de texture */
#define TITRE ((sfFloatRect){20.f, 20.f, 320.f, 46.f})
#define ILLUSTRATION ((sfFloatRect){36.f, 104.f, 288.f, 288.f})
#define TEXTE_CARTE ((sfFloatRect){24.f, 414.f, 312.f, 92.f})

/** Rectangle en dégradé vertical. */
void yugioh_degrade(app_t *app, sfFloatRect r, sfColor haut, sfColor bas)
{
    sfVertex v[4] = {
        {{r.left, r.top}, haut, {0, 0}},
        {{r.left + r.width, r.top}, haut, {0, 0}},
        {{r.left + r.width, r.top + r.height}, bas, {0, 0}},
        {{r.left, r.top + r.height}, bas, {0, 0}},
    };

    cible_primitives(app, v, 4, sfQuads, NULL);
}

/** Fines diagonales sur le corps : le grain du carton. */
static void grain(app_t *app, sfFloatRect r, sfColor c)
{
    sfVertex v[2];
    float x0 = 0.f;
    float x1 = 0.f;

    for (float k = -r.height; k < r.width; k += 7.f) {
        x0 = fmaxf(r.left, r.left + k);
        x1 = fminf(r.left + r.width, r.left + k + r.height);
        if (x1 <= x0)
            continue;
        v[0] = (sfVertex){{x0, r.top + (x0 - r.left - k)}, c, {0, 0}};
        v[1] = (sfVertex){{x1, r.top + (x1 - r.left - k)}, c, {0, 0}};
        cible_primitives(app, v, 2, sfLines, NULL);
    }
}

/** Orbe de niveau : disque rouge cerclé d'or, étoile jaune (noir pour les Xyz). */
static void etoile_niveau(app_t *app, sfVector2f c, float r, int xyz)
{
    sfVertex v[12];
    float a = 0.f;
    float l = 0.f;
    sfColor jaune = {255, 214, 74, 255};

    cercle(app, c, r, xyz ? (sfColor){20, 20, 24, 255} : (sfColor){200, 50, 30, 255},
        OR_CADRE, 1.5f);
    v[0] = (sfVertex){c, jaune, {0, 0}};
    for (int i = 0; i <= 10; i++) {
        a = -PI / 2.f + i * PI / 5.f;
        l = i % 2 == 0 ? r * 0.7f : r * 0.3f;
        v[i + 1] = (sfVertex){{c.x + cosf(a) * l, c.y + sinf(a) * l}, jaune, {0, 0}};
    }
    cible_primitives(app, v, 12, sfTriangleFan, NULL);
}

static void titre(app_t *app, classe_t c, const carte_classe_t *k)
{
    sfFloatRect r = TITRE;
    unsigned int taille = 26;
    sfVector2f orbe = {r.left + r.width - 24.f, r.top + r.height / 2.f};
    char nom[48];

    rect_arrondi_bord(app, r, 3.f, fond_titre(k->cadre), (sfColor){40, 28, 14, 255});
    rect_arrondi(app, (sfFloatRect){r.left + 3.f, r.top + 3.f, r.width - 6.f, 2.f}, 0.f,
        avec_alpha(C_BLANC, 0.3f));
    while (taille > 16 && largeur_texte(app, POLICE_GRAS, classe_info(c)->nom, taille)
        > r.width - 70.f)
        taille--;
    tronquer(app, POLICE_GRAS, taille, classe_info(c)->nom, r.width - 70.f, nom,
        sizeof(nom));
    texte(app, POLICE_GRAS, nom, taille, (sfVector2f){r.left + 14.f, orbe.y},
        encre_cadre(k->cadre), ALIGNE_GAUCHE);
    cercle(app, orbe, 17.f, couleur_attribut(k->attribut), OR_CADRE, 2.f);
    lueur(app, (sfVector2f){orbe.x - 5.f, orbe.y - 6.f}, (sfVector2f){12.f, 10.f},
        avec_alpha(C_BLANC, 0.55f));
    texte(app, POLICE_SYMBOLES, symbole_attribut(k->attribut), 17, orbe, C_BLANC,
        ALIGNE_CENTRE);
}

static void niveau(app_t *app, const carte_classe_t *k)
{
    int xyz = k->cadre == CADRE_XYZ;
    float y = 84.f;

    for (int i = 0; i < k->niveau && i < 12; i++)
        etoile_niveau(app, (sfVector2f){xyz ? 44.f + i * 24.f : 316.f - i * 24.f, y},
            10.f, xyz);
}

/** Illustration de la carte, ou, si l'image manque, l'emblème de la classe mis en scène. */
static void illustration(app_t *app, classe_t c)
{
    sfFloatRect r = ILLUSTRATION;
    const sfTexture *art = app->yugioh.art[c];
    sfVector2u t;
    sfVector2f centre = {r.left + r.width / 2.f, r.top + r.height / 2.f};
    sfColor col = couleur_classe(c);

    rect_arrondi(app, (sfFloatRect){r.left - 4.f, r.top - 4.f, r.width + 8.f,
        r.height + 8.f}, 2.f, (sfColor){30, 22, 12, 255});
    if (art != NULL) {
        t = sfTexture_getSize(art);
        sfSprite_setTexture(app->sprite, art, sfTrue);
        sfSprite_setOrigin(app->sprite, (sfVector2f){0.f, 0.f});
        sfSprite_setRotation(app->sprite, 0.f);
        sfSprite_setPosition(app->sprite, (sfVector2f){r.left, r.top});
        sfSprite_setScale(app->sprite, (sfVector2f){r.width / t.x, r.height / t.y});
        sfSprite_setColor(app->sprite, C_BLANC);
        cible_sprite(app, app->sprite, NULL);
    } else {
        yugioh_degrade(app, r, melanger(col, C_NOIR, 0.45f), melanger(col, C_NOIR, 0.85f));
        lueur(app, centre, (sfVector2f){150.f, 150.f}, avec_alpha(col, 0.55f));
        for (int i = 0; i < 3; i++)
            arc_cercle(app, centre, 86.f + i * 16.f, 2.f, (sfVector2f){i * 2.1f,
                i * 2.1f + 3.4f}, avec_alpha(melanger(col, C_BLANC, 0.4f), 0.6f), 1);
        hexagone(app, centre, 78.f, 0.f, melanger((sfColor){14, 16, 32, 255}, col, 0.25f),
            col);
        embleme(app, c, centre, 42.f, melanger(col, C_BLANC, 0.55f));
    }
    rect_arrondi_bord(app, r, 0.f, C_TRANSPARENT, avec_alpha(OR_CADRE, 0.6f));
}

static void mod_texte(char *buf, size_t taille, const char *stat, int mod)
{
    if (mod == 0)
        snprintf(buf, taille, "%s ±0", stat);
    else
        snprintf(buf, taille, "%s %+d", stat, mod);
}

static void encart(app_t *app, classe_t c, const carte_classe_t *k)
{
    const info_classe_t *info = classe_info(c);
    sfFloatRect r = TEXTE_CARTE;
    char ligne[96];
    char mods[NB_STATS][16];

    yugioh_degrade(app, r, (sfColor){240, 230, 204, 255}, (sfColor){216, 200, 160, 255});
    rect_arrondi_bord(app, r, 0.f, C_TRANSPARENT, (sfColor){108, 84, 50, 255});
    snprintf(ligne, sizeof(ligne), "[ %s / %s ]", nom_categorie(info->categorie),
        nom_cadre(k->cadre));
    texte(app, POLICE_GRAS, ligne, 14, (sfVector2f){r.left + 10.f, r.top + 16.f},
        ENCRE_PARCHEMIN, ALIGNE_GAUCHE);
    texte(app, POLICE_NORMALE, info->description, 14, (sfVector2f){r.left + 10.f,
        r.top + 36.f}, ENCRE_PARCHEMIN, ALIGNE_GAUCHE);
    snprintf(ligne, sizeof(ligne), "Attribut %s  ·  Niveau %d", nom_attribut(k->attribut),
        k->niveau);
    texte(app, POLICE_NORMALE, ligne, 12, (sfVector2f){r.left + 10.f, r.top + 54.f},
        (sfColor){80, 62, 40, 255}, ALIGNE_GAUCHE);
    rect_arrondi(app, (sfFloatRect){r.left + 10.f, r.top + 66.f, r.width - 20.f, 1.f},
        0.f, (sfColor){108, 84, 50, 200});
    for (int i = 0; i < NB_STATS; i++)
        mod_texte(mods[i], sizeof(mods[i]), STATS[i], info->mods[i]);
    snprintf(ligne, sizeof(ligne), "%s   %s   %s   %s", mods[0], mods[1], mods[2],
        mods[3]);
    texte(app, POLICE_GRAS, ligne, 13, (sfVector2f){r.left + r.width - 10.f,
        r.top + 79.f}, ENCRE_PARCHEMIN, ALIGNE_DROITE);
}

void dessiner_face(app_t *app, classe_t c)
{
    const carte_classe_t *k = carte_de_classe(c);
    sfFloatRect corps = {8.f, 8.f, FACE_L - 16.f, FACE_H - 16.f};
    char code[24];

    rect_arrondi(app, (sfFloatRect){0.f, 0.f, FACE_L, FACE_H}, 14.f,
        (sfColor){22, 17, 11, 255});
    yugioh_degrade(app, corps, couleur_cadre(k->cadre, 0), couleur_cadre(k->cadre, 1));
    grain(app, corps, avec_alpha(C_BLANC, k->cadre == CADRE_XYZ ? 0.05f : 0.08f));
    rect_arrondi_bord(app, (sfFloatRect){11.f, 11.f, FACE_L - 22.f, FACE_H - 22.f}, 4.f,
        C_TRANSPARENT, avec_alpha(OR_CADRE, 0.45f));
    titre(app, c, k);
    niveau(app, k);
    illustration(app, c);
    snprintf(code, sizeof(code), "RDS-FR%03d", (int)c + 1);
    texte(app, POLICE_GRAS, code, 11, (sfVector2f){FACE_L - 36.f, 404.f},
        encre_cadre(k->cadre), ALIGNE_DROITE);
    encart(app, c, k);
}
