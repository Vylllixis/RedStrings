/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Cartes de personnage : avatar, fiche et mini-carte.
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
#include "gui_ui.h"

void avatar(app_t *app, const personnage_t *p, sfVector2f centre, float r,
    float mort)
{
    sfColor c = griser(couleur_classe(p->classe), mort);

    lueur(app, centre, (sfVector2f){r * 2.4f, r * 2.4f},
        avec_alpha(c, 0.32f * (1.f - mort * 0.6f)));
    hexagone(app, centre, r * 1.08f, 0.f, melanger((sfColor){16, 18, 34, 255}, c, 0.22f),
        avec_alpha(c, 0.9f));
    hexagone(app, centre, r * 0.86f, PI / 6.f, C_TRANSPARENT, avec_alpha(c, 0.25f));
    app->desature = mort;
    embleme(app, p->classe, centre, r * 0.5f, melanger(c, C_BLANC, 0.55f));
    app->desature = 0.f;
}

/** Barre de 20 segments, comme la barre █░ de la version terminal. */
void barre_segments(app_t *app, sfFloatRect r, float segments, sfColor c)
{
    float sw = (r.width - 19.f * 3.f) / 20.f;
    float f = 0.f;
    float h = 0.f;
    float x = 0.f;
    int dernier = (int)ceilf(segments) - 1;

    for (int i = 0; i < 20; i++) {
        x = r.left + i * (sw + 3.f);
        f = bornerf(segments - i, 0.f, 1.f);
        rect_arrondi(app, (sfFloatRect){x, r.top, sw, r.height}, 2.5f,
            avec_alpha(C_BLANC, 0.06f));
        if (f <= 0.f)
            continue;
        h = r.height * (0.5f + 0.5f * f);
        rect_arrondi(app, (sfFloatRect){x, r.top + (r.height - h) / 2.f, sw, h}, 2.5f,
            avec_alpha(melanger(c, C_BLANC, 0.6f * (1.f - f)), f));
    }
    if (dernier >= 0 && dernier < 20)
        lueur(app, (sfVector2f){r.left + dernier * (sw + 3.f) + sw / 2.f,
            r.top + r.height / 2.f}, (sfVector2f){28.f, 20.f}, avec_alpha(c, 0.45f));
}

/** Fiche détaillée d'un personnage (équivalent de afficher_personnage). */
void fiche(app_t *app, const personnage_t *p, sfFloatRect r,
    const float segments[NB_STATS])
{
    sfColor c = couleur_classe(p->classe);
    float cx = r.left + r.width / 2.f;
    float y = 0.f;
    char buf[64];

    ombre_biseau(app, r, 26.f, 1.4f);
    panneau(app, r, 26.f, C_SURFACE_HAUTE, (sfColor){12, 14, 28, 255}, C_BORD);
    crochets(app, (sfFloatRect){r.left - 6.f, r.top - 6.f, r.width + 12.f,
        r.height + 12.f}, 20.f, avec_alpha(c, 0.6f));
    lueur(app, (sfVector2f){cx, r.top + 80.f}, (sfVector2f){r.width * 0.55f, 140.f},
        avec_alpha(c, 0.16f));
    rect_arrondi(app, (sfFloatRect){cx - 44.f, r.top, 88.f, 3.f}, 1.5f, c);
    avatar(app, p, (sfVector2f){cx, r.top + 104.f}, 60.f, est_ko(p) ? 1.f : 0.f);
    tronquer(app, POLICE_GRAS, 32, p->nom[0] ? p->nom : "Sans nom",
        r.width - 60.f, buf, sizeof(buf));
    texte(app, POLICE_GRAS, buf, 32, (sfVector2f){cx, r.top + 200.f},
        p->nom[0] ? C_TEXTE : C_TEXTE_FAIBLE, ALIGNE_CENTRE);
    pastille(app, nom_classe_maj(p->classe), (sfVector2f){cx, r.top + 244.f}, c, 0);
    if (est_ko(p))
        pastille(app, "K.O.", (sfVector2f){r.left + r.width - 46.f, r.top + 32.f},
            C_ROUGE, 1);
    rect_arrondi(app, (sfFloatRect){r.left + 32.f, r.top + 282.f, r.width - 64.f, 1.f},
        0.f, avec_alpha(C_BLANC, 0.07f));
    for (int i = 0; i < NB_STATS; i++) {
        y = r.top + 312.f + i * 48.f;
        etiquette(app, nom_stat((stat_t)i), (sfVector2f){r.left + 32.f, y},
            C_TEXTE_DOUX, ALIGNE_GAUCHE);
        snprintf(buf, sizeof(buf), "%d%s", valeur_stat(p, (stat_t)i),
            i == STAT_DEF ? " %" : "");
        texte(app, POLICE_GRAS, buf, 20, (sfVector2f){r.left + r.width - 32.f, y},
            couleur_stat((stat_t)i), ALIGNE_DROITE);
        barre_segments(app, (sfFloatRect){r.left + 32.f, y + 18.f, r.width - 64.f,
            10.f}, segments[i], couleur_stat((stat_t)i));
    }
}

static void mini_stats(app_t *app, const personnage_t *p, sfFloatRect r)
{
    float y = 0.f;
    float bw = r.width - 52.f - 48.f;
    int v = 0;
    char buf[8];

    for (int i = 0; i < NB_STATS; i++) {
        y = r.top + 178.f + i * 14.f;
        v = valeur_stat(p, (stat_t)i);
        texte(app, POLICE_GRAS, abrege_stat((stat_t)i), 10, (sfVector2f){r.left + 18.f,
            y}, C_TEXTE_FAIBLE, ALIGNE_GAUCHE);
        rect_arrondi(app, (sfFloatRect){r.left + 52.f, y - 2.f, bw, 4.f}, 2.f,
            avec_alpha(C_BLANC, 0.07f));
        if (v > 0)
            rect_arrondi(app, (sfFloatRect){r.left + 52.f, y - 2.f,
                fmaxf(4.f, bw * v / (float)STAT_MAX), 4.f}, 2.f, couleur_stat((stat_t)i));
        snprintf(buf, sizeof(buf), "%d", v);
        texte(app, POLICE_GRAS, buf, 12, (sfVector2f){r.left + r.width - 18.f, y},
            couleur_stat((stat_t)i), ALIGNE_DROITE);
    }
}

void mini_carte(app_t *app, const personnage_t *p, sfFloatRect r,
    float survol, const marque_t *marque)
{
    float mort = est_ko(p) ? 1.f : 0.f;
    sfColor c = griser(couleur_classe(p->classe), mort * 0.8f);
    sfColor bord = melanger(C_BORD, avec_alpha(c, 0.8f), survol);
    float cx = r.left + r.width / 2.f;
    char buf[64];

    if (marque) {
        bord = melanger(avec_alpha(marque->couleur, 0.6f), marque->couleur,
            0.5f + 0.5f * sinf(app->temps * 4.f));
        lueur(app, (sfVector2f){cx, r.top + r.height / 2.f}, (sfVector2f){r.width * 0.85f,
            r.height * 0.75f}, avec_alpha(marque->couleur, 0.2f));
    }
    lueur(app, (sfVector2f){cx, r.top + r.height}, (sfVector2f){r.width * 0.7f, 50.f},
        avec_alpha(c, 0.3f * survol));
    ombre_biseau(app, r, 18.f, 0.9f + 0.6f * survol);
    panneau(app, r, 18.f, melanger(C_SURFACE_HAUTE, melanger(C_SURFACE_HAUTE, c, 0.2f),
        survol), C_SURFACE, bord);
    if (marque)
        crochets(app, (sfFloatRect){r.left - 5.f, r.top - 5.f, r.width + 10.f,
            r.height + 10.f}, 16.f, marque->couleur);
    else if (survol > 0.01f)
        crochets(app, (sfFloatRect){r.left - 5.f, r.top - 5.f, r.width + 10.f,
            r.height + 10.f}, 14.f * survol, avec_alpha(c, survol));
    avatar(app, p, (sfVector2f){cx, r.top + 64.f}, 36.f, mort);
    tronquer(app, POLICE_GRAS, 19, p->nom, r.width - 28.f, buf, sizeof(buf));
    texte(app, POLICE_GRAS, buf, 19, (sfVector2f){cx, r.top + 122.f}, C_TEXTE,
        ALIGNE_CENTRE);
    pastille(app, nom_classe_maj(p->classe), (sfVector2f){cx, r.top + 152.f}, c, 0);
    mini_stats(app, p, r);
    if (mort > 0.f)
        pastille(app, "K.O.", (sfVector2f){r.left + r.width - 36.f, r.top + 24.f},
            C_ROUGE, 1);
    if (marque && marque->badge)
        pastille(app, marque->badge, (sfVector2f){cx, r.top}, marque->couleur, 1);
}
