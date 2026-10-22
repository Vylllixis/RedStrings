/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Création : aperçu en direct des statistiques finales.
 */

#include <math.h>
#include <stdio.h>
#include "personnage.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_etat.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "gui_ui.h"
#include "creation_interne.h"

#define AY 74.f

#define AH 610.f

/** « +10 » vert, « -5 » rouge, « ±0 » discret : l'effet de la classe. */
static void puce_mod(app_t *app, int mod, sfVector2f pos)
{
    sfColor c = mod > 0 ? C_VERT : (mod < 0 ? C_ROUGE : C_TEXTE_FAIBLE);
    char buf[8];
    float w = 0.f;

    snprintf(buf, sizeof(buf), mod == 0 ? "±0" : "%+d", mod);
    w = largeur_texte(app, POLICE_GRAS, buf, 11) + 12.f;
    panneau(app, (sfFloatRect){pos.x, pos.y - 9.f, w, 18.f}, 5.f, avec_alpha(c, 0.16f),
        avec_alpha(c, 0.06f), avec_alpha(c, 0.4f));
    texte(app, POLICE_GRAS, buf, 11, (sfVector2f){pos.x + w / 2.f, pos.y}, c,
        ALIGNE_CENTRE);
}

/** Statistiques finales = base choisie + modificateurs de la classe. */
static void stats_apercu(app_t *app, creation_t *c, sfColor col)
{
    int finale = 0;
    const sfVector2f radar = {AX + 108.f, 590.f};
    const float x0 = AX + 214.f;
    const float w = AW - 214.f - 26.f;
    float v[NB_STATS];
    float y = 0.f;
    float lx = 0.f;
    char buf[16];

    for (int i = 0; i < NB_STATS; i++) {
        finale = stat_finale(c->classe, (stat_t)i, c->valeurs[i]);
        c->affiche[i] = approcher(c->affiche[i], finale, 10.f, app->dt);
        c->segments[i] = approcher(c->segments[i], (finale + 4) / 5, 10.f, app->dt);
        v[i] = c->affiche[i] / STAT_MAX;
    }
    radar_stats(app, radar, 62.f, v, col);
    for (int i = 0; i < NB_STATS; i++) {
        y = radar.y + sinf(-PI / 2.f + i * 2.f * PI / NB_STATS) * 78.f;
        texte(app, POLICE_GRAS, abrege_stat((stat_t)i), 11, (sfVector2f){radar.x
            + cosf(-PI / 2.f + i * 2.f * PI / NB_STATS) * 84.f, y},
            avec_alpha(couleur_stat((stat_t)i), 0.8f), ALIGNE_CENTRE);
    }
    for (int i = 0; i < NB_STATS; i++) {
        y = 516.f + i * 40.f;
        lx = etiquette(app, nom_stat((stat_t)i), (sfVector2f){x0, y}, C_TEXTE_DOUX,
            ALIGNE_GAUCHE);
        puce_mod(app, classe_info(c->classe)->mods[i], (sfVector2f){x0 + lx + 10.f, y});
        snprintf(buf, sizeof(buf), "%d%s", (int)roundf(c->affiche[i]),
            i == STAT_DEF ? " %" : "");
        texte(app, POLICE_GRAS, buf, 18, (sfVector2f){x0 + w, y - 1.f},
            melanger(couleur_stat((stat_t)i), C_BLANC, 0.1f), ALIGNE_DROITE);
        barre_segments(app, (sfFloatRect){x0, y + 13.f, w, 8.f}, c->segments[i],
            couleur_stat((stat_t)i));
    }
}

void creation_apercu(app_t *app, creation_t *c, float a)
{
    personnage_t p = {0};
    float e = sortie_cubique(progression(app->t_scene, 0.2f, 0.7f));
    float envol = c->sortie >= 0.f
        ? entree_cubique(1.f - c->sortie / DUREE_SORTIE_CREATION) : 0.f;
    sfColor col = melanger(couleur_classe(c->classe_prec), couleur_classe(c->classe),
        progression(c->t_classe, 0.f, 0.35f));
    sfFloatRect r = {AX + 40.f * (1.f - e), AY - 30.f * envol, AW, AH};
    float cx = r.left + AW / 2.f;
    char buf[64];

    snprintf(p.nom, sizeof(p.nom), "%s", c->nom.texte);
    p.classe = c->classe;
    app->alpha = a * progression(app->t_scene, 0.2f, 0.35f);
    ombre_biseau(app, r, 26.f, 1.4f);
    panneau(app, r, 26.f, (sfColor){24, 29, 52, 235}, (sfColor){12, 14, 28, 245},
        melanger(C_BORD, avec_alpha(col, 0.5f), 0.5f));
    crochets(app, (sfFloatRect){r.left - 6.f, r.top - 6.f, r.width + 12.f,
        r.height + 12.f}, 22.f, avec_alpha(col, 0.7f));
    rect_arrondi(app, (sfFloatRect){r.left + 26.f, r.top, 140.f, 3.f}, 0.f, col);
    cercle(app, (sfVector2f){r.left + 34.f, r.top + 30.f}, 4.f, avec_alpha(C_ROUGE,
        0.5f + 0.5f * sinf(app->temps * 4.f)), C_TRANSPARENT, 0.f);
    etiquette(app, "APERÇU EN DIRECT", (sfVector2f){r.left + 46.f, r.top + 30.f},
        C_TEXTE_DOUX, ALIGNE_GAUCHE);
    etiquette(app, nom_classe_maj(c->classe), (sfVector2f){r.left + AW - 26.f,
        r.top + 30.f}, col, ALIGNE_DROITE);
    creation_hologramme(app, c);
    tronquer(app, POLICE_GRAS, 32, p.nom[0] ? p.nom : "Sans nom", AW - 80.f, buf, sizeof(buf));
    app->espacement = 0.5f;
    texte(app, POLICE_GRAS, buf, 32, (sfVector2f){cx, 394.f - 30.f * envol},
        p.nom[0] ? C_TEXTE : C_TEXTE_FAIBLE, ALIGNE_CENTRE);
    app->espacement = 0.f;
    pastille(app, nom_classe_maj(c->classe), (sfVector2f){cx, 432.f - 30.f * envol},
        col, 0);
    snprintf(buf, sizeof(buf), "%s  ·  %s", nom_categorie(classe_info(c->classe)->categorie),
        classe_info(c->classe)->description);
    texte(app, POLICE_NORMALE, buf, 13, (sfVector2f){cx, 460.f - 30.f * envol},
        C_TEXTE_FAIBLE, ALIGNE_CENTRE);
    titre_section(app, NULL, "STATISTIQUES FINALES", (sfVector2f){r.left + 26.f,
        490.f - 30.f * envol}, AW - 52.f, col);
    stats_apercu(app, c, col);
    panneau(app, r, 26.f, avec_alpha(C_BLANC, 0.45f * c->flash),
        avec_alpha(C_BLANC, 0.2f * c->flash), C_TRANSPARENT);
    c->flash = approcher(c->flash, 0.f, 5.f, app->dt);
    app->alpha = a;
}
