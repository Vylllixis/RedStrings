/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <math.h>
#include <stdio.h>
#include "gui.h"

#define CARTE_L 216.f
#define CARTE_H 236.f
#define ECART 22.f

/** Nom de la classe, pris dans la table classes[] partagée avec la console. */
const char *nom_classe(classe_t c)
{
    return classes[c].nom;
}

/**
 * Majuscules UTF-8 : ASCII, plus les lettres accentuées du français codées
 * sur deux octets (à → À, é → É, ï → Ï…), pour « ÉLÉMENTALISTE » ou « PRÊTRE ».
 */
static void majuscules_utf8(char *s)
{
    unsigned char *u = (unsigned char *)s;

    for (int i = 0; u[i] != '\0'; i++) {
        if (u[i] >= 'a' && u[i] <= 'z')
            u[i] -= 32;
        else if (u[i] == 0xC3 && u[i + 1] >= 0xA0 && u[i + 1] <= 0xBE
            && u[i + 1] != 0xB7)
            u[++i] -= 0x20;
    }
}

/** Nom en majuscules, calculé une fois par classe depuis classes[]. */
const char *nom_classe_maj(classe_t c)
{
    static char maj[NB_CLASSES][NOM_MAX];

    if (maj[c][0] == '\0') {
        my_strncpy(maj[c], classes[c].nom, NOM_MAX - 1);
        majuscules_utf8(maj[c]);
    }
    return maj[c];
}

/** Même couleur que dans le terminal : le RGB central de classes[]. */
sfColor couleur_classe(classe_t c)
{
    return (sfColor){classes[c].couleur.r, classes[c].couleur.g,
        classes[c].couleur.b, 255};
}

void avatar(app_t *app, const personnage_t *p, sfVector2f centre, float r,
    float mort)
{
    sfColor c = griser(couleur_classe(p->classe), mort);

    lueur(app, centre, (sfVector2f){r * 2.4f, r * 2.4f},
        avec_alpha(c, 0.32f * (1.f - mort * 0.6f)));
    hexagone(app, centre, r * 1.08f, 0.f, melanger((sfColor){16, 18, 34, 255}, c, 0.22f),
        avec_alpha(c, 0.9f));
    hexagone(app, centre, r * 0.86f, PI / 6.f, C_TRANSPARENT, avec_alpha(c, 0.25f));
    embleme(app, p->classe, centre, r * 0.5f, melanger(c, C_BLANC, 0.55f));
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
    const float segments[3])
{
    static const char *noms[3] = {"POINTS DE VIE", "ATTAQUE", "DÉFENSE"};
    const sfColor cs[3] = {C_VERT, C_ROUGE, C_BLEU};
    const int vals[3] = {p->pv, p->atq, p->def};
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
        r.width - 60.f, buf);
    texte(app, POLICE_GRAS, buf, 32, (sfVector2f){cx, r.top + 200.f},
        p->nom[0] ? C_TEXTE : C_TEXTE_FAIBLE, ALIGNE_CENTRE);
    pastille(app, nom_classe_maj(p->classe), (sfVector2f){cx, r.top + 244.f}, c, 0);
    if (est_ko(p))
        pastille(app, "K.O.", (sfVector2f){r.left + r.width - 46.f, r.top + 32.f},
            C_ROUGE, 1);
    rect_arrondi(app, (sfFloatRect){r.left + 32.f, r.top + 282.f, r.width - 64.f, 1.f},
        0.f, avec_alpha(C_BLANC, 0.07f));
    for (int i = 0; i < 3; i++) {
        y = r.top + 322.f + i * 60.f;
        etiquette(app, noms[i], (sfVector2f){r.left + 32.f, y}, C_TEXTE_DOUX,
            ALIGNE_GAUCHE);
        snprintf(buf, sizeof(buf), "%d%s", vals[i], i == 2 ? " %" : "");
        texte(app, POLICE_GRAS, buf, 20, (sfVector2f){r.left + r.width - 32.f, y},
            cs[i], ALIGNE_DROITE);
        barre_segments(app, (sfFloatRect){r.left + 32.f, y + 20.f, r.width - 64.f,
            12.f}, segments[i], cs[i]);
    }
}

static void mini_stats(app_t *app, const personnage_t *p, sfFloatRect r)
{
    static const char *noms[3] = {"PV", "ATK", "DEF"};
    const sfColor cs[3] = {C_VERT, C_ROUGE, C_BLEU};
    const int vals[3] = {p->pv, p->atq, p->def};
    float y = 0.f;
    float bw = r.width - 52.f - 48.f;
    char buf[8];

    for (int i = 0; i < 3; i++) {
        y = r.top + 186.f + i * 16.f;
        texte(app, POLICE_GRAS, noms[i], 11, (sfVector2f){r.left + 18.f, y},
            C_TEXTE_FAIBLE, ALIGNE_GAUCHE);
        rect_arrondi(app, (sfFloatRect){r.left + 52.f, y - 2.5f, bw, 5.f}, 2.5f,
            avec_alpha(C_BLANC, 0.07f));
        if (vals[i] > 0)
            rect_arrondi(app, (sfFloatRect){r.left + 52.f, y - 2.5f,
                fmaxf(5.f, bw * vals[i] / (float)STAT_MAX), 5.f}, 2.5f, cs[i]);
        snprintf(buf, sizeof(buf), "%d", vals[i]);
        texte(app, POLICE_GRAS, buf, 13, (sfVector2f){r.left + r.width - 18.f, y},
            cs[i], ALIGNE_DROITE);
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
    tronquer(app, POLICE_GRAS, 19, p->nom, r.width - 28.f, buf);
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

static const marque_t *chercher_marque(const marque_t *marques, int nb, int id)
{
    for (int k = 0; k < nb; k++) {
        if (marques[k].id == id)
            return &marques[k];
    }
    return NULL;
}

/** Grille de cartes centrée (5 par ligne). Renvoie l'id cliqué ou -1. */
int grille_cartes(app_t *app, grille_t *g, const int *ids, int n, float y0,
    const marque_t *marques, int nb_marques)
{
    int clique = -1;
    float a = app->alpha;
    int id = 0;
    int nb_ligne = 0;
    float x = 0.f;
    float y = 0.f;
    float e = 0.f;
    int s = 0;

    for (int i = 0; i < n; i++) {
        id = ids[i];
        nb_ligne = n - (i / 5) * 5 < 5 ? n - (i / 5) * 5 : 5;
        x = LARGEUR / 2.f - (nb_ligne * CARTE_L + (nb_ligne - 1) * ECART) / 2.f
            + (i % 5) * (CARTE_L + ECART);
        y = y0 + (i / 5) * (CARTE_H + ECART);
        if (!g->place[id])
            g->pos[id] = (sfVector2f){x, y};
        g->place[id] = 1;
        g->pos[id].x = approcher(g->pos[id].x, x, 12.f, app->dt);
        g->pos[id].y = approcher(g->pos[id].y, y, 12.f, app->dt);
        x = g->pos[id].x;
        y = g->pos[id].y;
        e = sortie_rebond(progression(app->t_scene, 0.15f + i * 0.05f, 0.55f));
        s = survole(app, (sfFloatRect){x, y, CARTE_L, CARTE_H});
        if (s)
            app->curseur_voulu = CURSEUR_MAIN;
        if (s && app->in.clic)
            clique = id;
        g->survol[id] = approcher(g->survol[id], s ? 1.f : 0.f, 14.f, app->dt);
        maj_ressort(&g->lift[id], s ? -10.f : 0.f, 280.f, 18.f, app->dt);
        app->alpha = a * progression(app->t_scene, 0.15f + i * 0.05f, 0.3f);
        mini_carte(app, &app->equipe.data[id], (sfFloatRect){x, y + g->lift[id].x
            + 40.f * (1.f - e), CARTE_L, CARTE_H}, g->survol[id],
            chercher_marque(marques, nb_marques, id));
    }
    app->alpha = a;
    return clique;
}
