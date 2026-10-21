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

static const float LAME[] = {-0.08f, 0.3f, -0.08f, -0.72f, 0.f, -0.94f,
    0.08f, -0.72f, 0.08f, 0.3f};
static const float GARDE[] = {-0.34f, 0.3f, 0.34f, 0.3f, 0.34f, 0.42f,
    -0.34f, 0.42f};
static const float POIGNEE[] = {-0.055f, 0.42f, 0.055f, 0.42f, 0.055f, 0.74f,
    -0.055f, 0.74f};
static const float POINTE[] = {0.88f, -0.88f, 0.739f, -0.541f, 0.541f, -0.739f};
static const float PLUME_A[] = {-0.378f, 0.378f, -0.236f, 0.236f, -0.215f, 0.399f,
    -0.342f, 0.526f};
static const float PLUME_B[] = {-0.378f, 0.378f, -0.236f, 0.236f, -0.399f, 0.215f,
    -0.526f, 0.342f};

const char *nom_classe(classe_t c)
{
    if (c == MAGE)
        return "Mage";
    if (c == ARCHER)
        return "Archer";
    return "Guerrier";
}

const char *nom_classe_maj(classe_t c)
{
    if (c == MAGE)
        return "MAGE";
    if (c == ARCHER)
        return "ARCHER";
    return "GUERRIER";
}

sfColor couleur_classe(classe_t c)
{
    if (c == MAGE)
        return C_MAGE;
    if (c == ARCHER)
        return C_ARCHER;
    return C_GUERRIER;
}

static void epee(app_t *app, sfVector2f c, float t, float angle, sfColor col)
{
    polygone(app, LAME, 5, c, t, angle, col);
    polygone(app, GARDE, 4, c, t, angle, col);
    polygone(app, POIGNEE, 4, c, t, angle, col);
    cercle(app, (sfVector2f){c.x - 0.84f * sinf(angle) * t,
        c.y + 0.84f * cosf(angle) * t}, 0.1f * t, col, C_TRANSPARENT, 0.f);
}

static void etoile(app_t *app, sfVector2f c, float t, float angle, sfColor col)
{
    float pts[16];
    float a = 0.f;

    for (int k = 0; k < 8; k++) {
        a = k * PI / 4.f - PI / 2.f;
        pts[2 * k] = cosf(a) * (k % 2 ? 0.3f : 1.f);
        pts[2 * k + 1] = sinf(a) * (k % 2 ? 0.3f : 1.f);
    }
    polygone(app, pts, 8, c, t, angle, col);
}

static sfVector2f point(sfVector2f c, float t, float x, float y)
{
    return (sfVector2f){c.x + x * t, c.y + y * t};
}

static void arc(app_t *app, sfVector2f c, float t, sfColor col)
{
    sfVector2f bezier[24];
    sfVector2f corde[3] = {point(c, t, 0.38f, 0.78f), point(c, t, -0.42f, 0.42f),
        point(c, t, -0.78f, -0.38f)};
    sfVector2f fleche[2] = {point(c, t, -0.42f, 0.42f), point(c, t, 0.66f, -0.66f)};
    float u = 0.f;

    for (int i = 0; i < 24; i++) {
        u = i / 23.f;
        bezier[i] = point(c, t,
            (1 - u) * (1 - u) * 0.38f + 2 * (1 - u) * u * 0.507f + u * u * -0.78f,
            (1 - u) * (1 - u) * 0.78f + 2 * (1 - u) * u * -0.507f + u * u * -0.38f);
    }
    ligne_epaisse(app, corde, 3, fmaxf(1.f, 0.035f * t), avec_alpha(col, 0.75f), 0);
    ligne_epaisse(app, bezier, 24, 0.11f * t, col, 0);
    ligne_epaisse(app, fleche, 2, 0.075f * t, col, 0);
    polygone(app, POINTE, 3, c, t, 0.f, col);
    polygone(app, PLUME_A, 4, c, t, 0.f, col);
    polygone(app, PLUME_B, 4, c, t, 0.f, col);
}

/** Emblème vectoriel de la classe (épées, étoile, arc). */
void embleme(app_t *app, classe_t c, sfVector2f centre, float taille,
    sfColor couleur)
{
    float t = app->temps;

    if (c == GUERRIER) {
        epee(app, centre, taille, -0.72f, couleur);
        epee(app, centre, taille, 0.72f, couleur);
    } else if (c == MAGE) {
        etoile(app, point(centre, taille, -0.08f, 0.08f), taille * 0.85f,
            sinf(t * 0.8f) * 0.12f, couleur);
        etoile(app, point(centre, taille, 0.62f, -0.6f), taille * 0.3f,
            t * 0.9f, couleur);
        cercle(app, point(centre, taille, -0.62f, -0.56f), 0.08f * taille,
            avec_alpha(couleur, 0.5f + 0.5f * sinf(t * 3.f)), C_TRANSPARENT, 0.f);
    } else {
        arc(app, centre, taille, couleur);
    }
}

void avatar(app_t *app, const personnage_t *p, sfVector2f centre, float r,
    float mort)
{
    sfColor c = griser(couleur_classe(p->classe), mort);

    lueur(app, centre, (sfVector2f){r * 2.4f, r * 2.4f},
        avec_alpha(c, 0.32f * (1.f - mort * 0.6f)));
    cercle(app, centre, r, melanger((sfColor){16, 18, 34, 255}, c, 0.22f),
        avec_alpha(c, 0.9f), r > 30.f ? 3.f : 2.f);
    cercle(app, centre, r * 0.82f, C_TRANSPARENT, avec_alpha(c, 0.22f), 1.2f);
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

    ombre(app, r, 28.f, 1.4f);
    rect_arrondi_bord(app, r, 28.f, C_SURFACE, C_BORD);
    lueur(app, (sfVector2f){cx, r.top + 80.f}, (sfVector2f){r.width * 0.55f, 140.f},
        avec_alpha(c, 0.16f));
    rect_arrondi(app, (sfFloatRect){cx - 44.f, r.top, 88.f, 3.f}, 1.5f, c);
    avatar(app, p, (sfVector2f){cx, r.top + 104.f}, 60.f, p->pv == 0 ? 1.f : 0.f);
    tronquer(app, POLICE_GRAS, 32, p->nom[0] ? p->nom : "Sans nom",
        r.width - 60.f, buf);
    texte(app, POLICE_GRAS, buf, 32, (sfVector2f){cx, r.top + 200.f},
        p->nom[0] ? C_TEXTE : C_TEXTE_FAIBLE, ALIGNE_CENTRE);
    pastille(app, nom_classe_maj(p->classe), (sfVector2f){cx, r.top + 244.f}, c, 0);
    if (p->pv == 0)
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
                fmaxf(5.f, bw * vals[i] / 99.f), 5.f}, 2.5f, cs[i]);
        snprintf(buf, sizeof(buf), "%d", vals[i]);
        texte(app, POLICE_GRAS, buf, 13, (sfVector2f){r.left + r.width - 18.f, y},
            cs[i], ALIGNE_DROITE);
    }
}

void mini_carte(app_t *app, const personnage_t *p, sfFloatRect r,
    float survol, const marque_t *marque)
{
    float mort = p->pv == 0 ? 1.f : 0.f;
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
    ombre(app, r, 22.f, 0.9f + 0.6f * survol);
    rect_arrondi_bord(app, r, 22.f, melanger(C_SURFACE, C_SURFACE_HAUTE, survol * 0.7f),
        bord);
    if (marque)
        rect_arrondi_bord(app, (sfFloatRect){r.left + 1.5f, r.top + 1.5f, r.width - 3.f,
            r.height - 3.f}, 20.5f, C_TRANSPARENT, avec_alpha(marque->couleur, 0.7f));
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
        mini_carte(app, &app->persos[id], (sfFloatRect){x, y + g->lift[id].x
            + 40.f * (1.f - e), CARTE_L, CARTE_H}, g->survol[id],
            chercher_marque(marques, nb_marques, id));
    }
    app->alpha = a;
    return clique;
}
