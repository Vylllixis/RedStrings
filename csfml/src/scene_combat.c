/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"

#define CARTE_AL 400.f
#define CARTE_AH 410.f
#define GAUCHE_X 110.f
#define DROITE_X 770.f
#define CARTE_Y 118.f
#define T_IMPACT 0.32f
#define ELAN 220.f

/* ------------------------------------------------------------------ */
/*  Sélection de l'attaquant puis du défenseur                         */
/* ------------------------------------------------------------------ */

void entrerSelection(app_t *app)
{
    selection_t *s = &app->selection;

    memset(s, 0, sizeof(selection_t));
    s->idAtk = -1;
    s->idDef = -1;
    s->validation = -1.f;
}

static void etape(app_t *app, sfVector2f c, const char *num, const char *label,
    sfColor col, float actif)
{
    lueur(app, c, (sfVector2f){44.f, 44.f}, avecAlpha(col, 0.35f * actif));
    cercle(app, c, 15.f, melanger(C_SURFACE_HAUTE, col, actif), avecAlpha(col, 0.4f
        + 0.6f * actif), 2.f);
    texte(app, num[0] == '1' || num[0] == '2' ? POLICE_GRAS : POLICE_SYMBOLES, num,
        13, c, actif > 0.5f ? C_BLANC : C_TEXTE_DOUX, ALIGNE_CENTRE);
    texte(app, POLICE_GRAS, label, 15, (sfVector2f){c.x + 26.f, c.y},
        melanger(C_TEXTE_FAIBLE, C_TEXTE, actif), ALIGNE_GAUCHE);
}

static void etapes(app_t *app, selection_t *s, float a)
{
    float p = 0.f;

    s->etapeAnim = approcher(s->etapeAnim, s->idDef >= 0 ? 2.f : (float)s->etape,
        10.f, app->dt);
    p = bornerf(s->etapeAnim, 0.f, 1.f);
    app->alpha = a * progression(app->tScene, 0.15f, 0.4f);
    rectArrondi(app, (sfFloatRect){1010.f, 121.f, 50.f, 2.f}, 1.f,
        avecAlpha(C_BLANC, 0.1f));
    rectArrondi(app, (sfFloatRect){1010.f, 121.f, 50.f * p, 2.f}, 1.f,
        melanger(C_ROUGE, C_BLEU, p));
    etape(app, (sfVector2f){880.f, 122.f}, s->etape > 0 ? "✔" : "1", "Attaquant",
        C_ROUGE, 1.f);
    etape(app, (sfVector2f){1084.f, 122.f}, s->idDef >= 0 ? "✔" : "2", "Défenseur",
        C_BLEU, p);
    app->alpha = a;
}

static void choisir(app_t *app, selection_t *s, int id)
{
    sfVector2f c = {s->grille.pos[id].x + 108.f, s->grille.pos[id].y + 118.f};

    if (s->etape == 0) {
        s->idAtk = id;
        s->etape = 1;
        emettre(app, c, 30, C_ROUGE, 420.f, 3.5f);
    } else if (id == s->idAtk) {
        s->idAtk = -1;
        s->etape = 0;
    } else {
        s->idDef = id;
        s->validation = 0.6f;
        emettre(app, c, 30, C_BLEU, 420.f, 3.5f);
    }
}

void sceneSelection(app_t *app)
{
    selection_t *s = &app->selection;
    int ids[MAX_PERSONNAGES];
    marque_t marques[2] = {{s->idAtk, C_ROUGE, "ATTAQUANT"},
        {s->idDef, C_BLEU, "DÉFENSEUR"}};
    int id = -1;

    if (app->in.touches[sfKeyEscape] && s->etape == 1 && s->validation < 0.f) {
        s->idAtk = -1;
        s->etape = 0;
    } else if (app->in.touches[sfKeyEscape]) {
        changerScene(app, SCENE_MENU);
    }
    for (int i = 0; i < app->nbPersos; i++)
        ids[i] = i;
    enTete(app, &s->retour, "Combat", s->etape == 0 ? "Choisis l'attaquant."
        : "Choisis maintenant le défenseur (clique à nouveau sur l'attaquant pour annuler).");
    etapes(app, s, app->alpha);
    id = grilleCartes(app, &s->grille, ids, app->nbPersos, 204.f, marques, 2);
    if (id >= 0 && s->validation < 0.f)
        choisir(app, s, id);
    if (s->validation >= 0.f) {
        s->validation -= app->dt;
        if (s->validation < 0.f)
            changerScene(app, SCENE_ARENE);
    }
}

/* ------------------------------------------------------------------ */
/*  Arène                                                              */
/* ------------------------------------------------------------------ */

void entrerArene(app_t *app)
{
    arene_t *ar = &app->arene;
    personnage_t *p = NULL;

    memset(ar, 0, sizeof(arene_t));
    ar->ids[0] = app->selection.idAtk;
    ar->ids[1] = app->selection.idDef;
    for (int i = 0; i < app->nbPersos; i++) {
        p = &app->persos[i];
        ar->pvAffiche[i] = p->pv;
        ar->pvFantome[i] = p->pv;
        ar->pvMax[i] = p->pv > 0 ? p->pv : 1;
        ar->ko[i] = p->pv == 0 ? 10.f : -1.f;
    }
    ar->tAttaque = -1.f;
    ar->echange = -1.f;
}

static int peutAttaquer(app_t *app, arene_t *ar)
{
    return ar->tAttaque < 0.f && ar->echange < 0.f
        && app->persos[ar->ids[0]].pv > 0 && app->persos[ar->ids[1]].pv > 0;
}

/** Instant du coup : même calcul que attaquer() + toute la mise en scène. */
static void impact(app_t *app, arene_t *ar)
{
    personnage_t *a = &app->persos[ar->ids[0]];
    personnage_t *d = &app->persos[ar->ids[1]];
    int idD = ar->ids[1];
    sfVector2f point = {DROITE_X + 120.f, CARTE_Y + 150.f};
    char msg[96];

    ar->atk = a->atk;
    ar->def = d->def;
    ar->degats = appliquerAttaque(a, d);
    ar->pvRestants = d->pv;
    ar->dernierAtk = ar->ids[0];
    ar->dernierDef = idD;
    ar->aAttaque = 1;
    ar->tResultat = 0.f;
    ar->tPop = 0.f;
    ar->tour++;
    ar->vs = 1.f;
    ar->flash[idD] = 1.f;
    ar->recul[idD] = 1.f;
    ar->delaiFantome[idD] = 0.45f;
    app->secousse = 0.35f + 0.65f * fminf(1.f, ar->degats / 40.f);
    app->hitstop = 0.07f;
    emettre(app, point, ar->degats > 0 ? 30 + ar->degats : 18,
        ar->degats > 0 ? couleurClasse(a->classe) : C_BLEU, 700.f, 4.5f);
    emettre(app, point, 14, C_BLANC, 500.f, 3.f);
    if (d->pv > 0)
        return;
    ar->ko[idD] = 0.f;
    app->secousse = 1.f;
    app->hitstop = 0.14f;
    emettre(app, (sfVector2f){DROITE_X + CARTE_AL / 2.f, CARTE_Y + 200.f}, 110, C_ROUGE,
        950.f, 5.5f);
    snprintf(msg, sizeof(msg), "%s est mort", d->name);
    notifier(app, "☠", msg, C_ROUGE);
}

static void inverser(arene_t *ar)
{
    int tmp = ar->ids[0];

    ar->ids[0] = ar->ids[1];
    ar->ids[1] = tmp;
    ar->echange = 0.f;
}

static void majArene(app_t *app, arene_t *ar)
{
    float dt = app->dtJeu;
    int id = 0;

    if (ar->tAttaque >= 0.f) {
        ar->tAttaque += dt;
        if (!ar->impact && ar->tAttaque >= T_IMPACT) {
            ar->impact = 1;
            impact(app, ar);
        }
        if (ar->tAttaque > 0.95f)
            ar->tAttaque = -1.f;
    }
    if (ar->echange >= 0.f) {
        ar->echange += dt / 0.65f;
        if (ar->echange >= 1.f)
            ar->echange = -1.f;
    }
    for (int k = 0; k < 2; k++) {
        id = ar->ids[k];
        ar->pvAffiche[id] = approcher(ar->pvAffiche[id], app->persos[id].pv, 10.f, dt);
        ar->delaiFantome[id] -= dt;
        if (ar->delaiFantome[id] <= 0.f)
            ar->pvFantome[id] = approcher(ar->pvFantome[id], ar->pvAffiche[id], 3.5f, dt);
        ar->flash[id] = approcher(ar->flash[id], 0.f, 7.f, dt);
        ar->recul[id] = approcher(ar->recul[id], 0.f, 5.f, dt);
        if (ar->ko[id] >= 0.f)
            ar->ko[id] += dt;
    }
    ar->vs = approcher(ar->vs, 0.f, 6.f, dt);
    ar->tPop += dt;
    ar->tResultat += dt;
}

/** Décalage horizontal de l'attaquant : recul, ruée, retour. */
static float elan(float t)
{
    if (t < 0.f)
        return 0.f;
    if (t < 0.2f)
        return -26.f * sortieCubique(t / 0.2f);
    if (t < T_IMPACT)
        return lerpf(-26.f, ELAN, entreeCubique((t - 0.2f) / (T_IMPACT - 0.2f)));
    return ELAN * (1.f - sortieCubique(progression(t, T_IMPACT, 0.55f)));
}

static sfVector2f positionSlot(app_t *app, arene_t *ar, int slot)
{
    float x = slot == 0 ? GAUCHE_X : DROITE_X;
    float y = CARTE_Y + sinf(app->temps * 1.5f + slot * 1.3f) * 4.f;
    float e = sortieCubique(progression(app->tScene, 0.1f + slot * 0.08f, 0.6f));
    float r = ar->recul[ar->ids[1]];

    if (ar->echange >= 0.f) {
        x = lerpf(slot == 0 ? DROITE_X : GAUCHE_X, x, entreeSortieCubique(ar->echange));
        y += (slot == 0 ? -46.f : 46.f) * sinf(PI * ar->echange);
    }
    if (slot == 0)
        x += elan(ar->tAttaque);
    else
        x += r * 36.f + sinf(app->temps * 70.f) * 10.f * r * r;
    x += (slot == 0 ? -140.f : 140.f) * (1.f - e);
    return (sfVector2f){x, y};
}

static void puce(app_t *app, sfFloatRect r, const char *label, const char *val,
    sfColor c)
{
    float cy = r.top + r.height / 2.f;

    rectArrondiBord(app, r, 14.f, avecAlpha(c, 0.1f), avecAlpha(c, 0.25f));
    etiquette(app, label, (sfVector2f){r.left + 16.f, cy}, melanger(c, C_BLANC, 0.3f),
        ALIGNE_GAUCHE);
    texte(app, POLICE_GRAS, val, 20, (sfVector2f){r.left + r.width - 16.f, cy}, C_TEXTE,
        ALIGNE_DROITE);
}

static sfColor couleurPv(float ratio)
{
    if (ratio > 0.5f)
        return C_VERT;
    if (ratio > 0.25f)
        return melanger(C_OR, C_VERT, (ratio - 0.25f) / 0.25f);
    return melanger(C_ROUGE, C_OR, ratio / 0.25f);
}

/** Barre de vie avec « barre fantôme » dorée qui se vide en retard. */
static void barrePv(app_t *app, arene_t *ar, int id, sfFloatRect br)
{
    float ratio = bornerf(ar->pvAffiche[id] / ar->pvMax[id], 0.f, 1.f);
    float fantome = bornerf(ar->pvFantome[id] / ar->pvMax[id], 0.f, 1.f);
    sfColor c = couleurPv(ratio);

    rectArrondi(app, br, 7.f, avecAlpha(C_BLANC, 0.07f));
    if (fantome > 0.005f)
        rectArrondi(app, (sfFloatRect){br.left, br.top, fmaxf(14.f, br.width * fantome),
            br.height}, 7.f, avecAlpha(C_OR, 0.85f));
    if (ratio <= 0.005f)
        return;
    rectArrondi(app, (sfFloatRect){br.left, br.top, fmaxf(14.f, br.width * ratio),
        br.height}, 7.f, c);
    rectArrondi(app, (sfFloatRect){br.left + 4.f, br.top + 2.f, fmaxf(6.f, br.width
        * ratio - 8.f), 4.f}, 2.f, avecAlpha(C_BLANC, 0.25f));
    lueur(app, (sfVector2f){br.left + br.width * ratio, br.top + br.height / 2.f},
        (sfVector2f){30.f, 22.f}, avecAlpha(c, 0.5f));
}

static void tamponKo(app_t *app, arene_t *ar, int id, sfFloatRect r, float mort)
{
    float s = ar->ko[id];
    float e = sortieRebond(progression(s, 0.05f, 0.4f));
    float a = app->alpha;
    sfVector2f c = {r.left + r.width / 2.f, r.top + r.height * 0.42f};

    rectArrondi(app, r, 28.f, avecAlpha((sfColor){6, 6, 14, 255}, 0.5f * mort));
    app->alpha = a * progression(s, 0.05f, 0.15f);
    lueur(app, c, (sfVector2f){200.f, 110.f}, avecAlpha(C_ROUGE, 0.3f));
    app->rotation = -12.f;
    app->contour = 3.f;
    texteEchelle(app, POLICE_GRAS, "K.O.", 80, c, C_ROUGE, ALIGNE_CENTRE,
        lerpf(2.4f, 1.f, e));
    app->rotation = 0.f;
    app->contour = 0.f;
    app->alpha = a;
}

static void carteArene(app_t *app, arene_t *ar, int slot, sfVector2f pos)
{
    int id = ar->ids[slot];
    personnage_t *p = &app->persos[id];
    float mort = ar->ko[id] >= 0.f ? sortieCubique(bornerf(ar->ko[id] / 0.7f, 0.f, 1.f)) : 0.f;
    sfColor c = griser(couleurClasse(p->classe), mort * 0.85f);
    sfColor role = slot == 0 ? C_ROUGE : C_BLEU;
    sfFloatRect r = {pos.x, pos.y, CARTE_AL, CARTE_AH};
    float cx = r.left + r.width / 2.f;
    float pw = (r.width - 64.f - 12.f) / 2.f;
    char buf[64];

    lueur(app, (sfVector2f){cx, r.top + r.height / 2.f}, (sfVector2f){r.width * 0.8f,
        r.height * 0.65f}, avecAlpha(c, 0.12f));
    ombre(app, r, 28.f, 1.4f);
    rectArrondiBord(app, r, 28.f, C_SURFACE, melanger(C_BORD, avecAlpha(role, 0.6f), 0.7f));
    lueur(app, (sfVector2f){cx, r.top + 90.f}, (sfVector2f){r.width * 0.5f, 120.f},
        avecAlpha(c, 0.15f));
    avatar(app, p, (sfVector2f){cx, r.top + 104.f}, 62.f, mort);
    tronquer(app, POLICE_GRAS, 30, p->name, r.width - 60.f, buf);
    texte(app, POLICE_GRAS, buf, 30, (sfVector2f){cx, r.top + 200.f}, C_TEXTE, ALIGNE_CENTRE);
    pastille(app, nomClasseMaj(p->classe), (sfVector2f){cx, r.top + 240.f}, c, 0);
    etiquette(app, "POINTS DE VIE", (sfVector2f){r.left + 32.f, r.top + 290.f},
        C_TEXTE_DOUX, ALIGNE_GAUCHE);
    snprintf(buf, sizeof(buf), "%d / %d", (int)roundf(ar->pvAffiche[id]),
        (int)ar->pvMax[id]);
    texte(app, POLICE_GRAS, buf, 18, (sfVector2f){r.left + r.width - 32.f, r.top + 290.f},
        C_TEXTE, ALIGNE_DROITE);
    barrePv(app, ar, id, (sfFloatRect){r.left + 32.f, r.top + 308.f, r.width - 64.f, 14.f});
    snprintf(buf, sizeof(buf), "%d", p->atk);
    puce(app, (sfFloatRect){r.left + 32.f, r.top + 344.f, pw, 46.f}, "ATTAQUE", buf, C_ROUGE);
    snprintf(buf, sizeof(buf), "%d %%", p->def);
    puce(app, (sfFloatRect){r.left + 44.f + pw, r.top + 344.f, pw, 46.f}, "DÉFENSE", buf,
        C_BLEU);
    rectArrondi(app, r, 28.f, avecAlpha(C_BLANC, 0.6f * ar->flash[id]));
    if (mort > 0.f)
        tamponKo(app, ar, id, r, mort);
    pastille(app, slot == 0 ? "ATTAQUANT" : "DÉFENSEUR", (sfVector2f){cx, r.top}, role, 1);
}

/** Traînée de l'attaquant pendant la ruée. */
static void trainee(app_t *app, arene_t *ar, sfVector2f pos)
{
    sfColor c = couleurClasse(app->persos[ar->ids[0]].classe);
    float t = ar->tAttaque;
    float dx = 0.f;

    if (t < 0.22f || t > 0.42f)
        return;
    for (int i = 1; i <= 3; i++) {
        dx = elan(t - i * 0.02f) - elan(t);
        rectArrondi(app, (sfFloatRect){pos.x + dx, pos.y, CARTE_AL, CARTE_AH}, 28.f,
            avecAlpha(c, 0.07f / i));
    }
}

/** Effet propre à la classe de l'attaquant au moment de l'impact. */
static void effetImpact(app_t *app, arene_t *ar)
{
    float t = ar->tPop;
    float p = sortieCubique(progression(t, 0.f, 0.12f));
    float f = 1.f - progression(t, 0.1f, 0.3f);
    sfVector2f c = {DROITE_X + 130.f, CARTE_Y + 170.f};
    Classe_t cl = app->persos[ar->dernierAtk].classe;
    sfColor col = couleurClasse(cl);
    sfVector2f l1[2] = {{c.x - 110.f * p, c.y - 110.f * p}, {c.x + 110.f * p, c.y + 110.f * p}};
    sfVector2f l2[2] = {{c.x + 100.f * p, c.y - 100.f * p}, {c.x - 100.f * p, c.y + 100.f * p}};
    sfVector2f fl[2] = {{c.x - 520.f + 400.f * p, c.y}, {c.x, c.y}};

    if (!ar->aAttaque || f <= 0.f)
        return;
    if (cl == GUERRIER) {
        ligneEpaisse(app, l1, 2, 26.f, avecAlpha(col, 0.35f * f), 1);
        ligneEpaisse(app, l1, 2, 5.f, avecAlpha(C_BLANC, f), 1);
        ligneEpaisse(app, l2, 2, 22.f, avecAlpha(col, 0.3f * f), 1);
        ligneEpaisse(app, l2, 2, 4.f, avecAlpha(C_BLANC, f), 1);
    } else if (cl == MAGE) {
        cercle(app, c, 30.f + 150.f * sortieCubique(progression(t, 0.f, 0.4f)),
            C_TRANSPARENT, avecAlpha(col, f), 5.f * f + 1.f);
        lueur(app, c, (sfVector2f){200.f * p, 200.f * p}, avecAlpha(col, 0.5f * f));
    } else {
        ligneEpaisse(app, fl, 2, 18.f, avecAlpha(col, 0.35f * f), 1);
        ligneEpaisse(app, fl, 2, 3.f, avecAlpha(C_BLANC, f), 1);
    }
}

static void popDegats(app_t *app, arene_t *ar)
{
    float t = ar->tPop;
    float s = t < 0.16f ? lerpf(0.3f, 1.3f, sortieCubique(t / 0.16f))
        : lerpf(1.3f, 1.f, sortieCubique(progression(t, 0.16f, 0.3f)));
    float a = app->alpha;
    sfVector2f pos = {DROITE_X + CARTE_AL / 2.f + 110.f, CARTE_Y + 120.f
        - 90.f * sortieCubique(progression(t, 0.f, 1.2f))};
    char buf[16];

    if (!ar->aAttaque || t > 1.4f)
        return;
    app->alpha = a * (1.f - progression(t, 0.85f, 0.45f));
    snprintf(buf, sizeof(buf), "-%d", ar->degats);
    lueur(app, pos, (sfVector2f){150.f, 90.f}, avecAlpha(ar->degats > 0 ? C_ROUGE : C_BLEU,
        0.35f));
    app->contour = 4.f;
    texteEchelle(app, POLICE_GRAS, ar->degats > 0 ? buf : "BLOQUÉ", ar->degats > 0 ? 78 : 52,
        pos, ar->degats > 0 ? C_ROUGE : C_BLEU, ALIGNE_CENTRE, s);
    app->contour = 0.f;
    app->alpha = a;
}

static void medaillon(app_t *app, arene_t *ar)
{
    sfVector2f c = {LARGEUR / 2.f, 300.f};
    float e = sortieRebond(progression(app->tScene, 0.35f, 0.5f));
    float pulse = 0.5f + 0.5f * sinf(app->temps * 2.5f);
    float s = e * (1.f + 0.25f * ar->vs);
    char buf[24];

    lueur(app, c, (sfVector2f){130.f * s, 130.f * s}, avecAlpha(C_ROUGE, 0.22f + 0.15f
        * pulse + 0.45f * ar->vs));
    cercle(app, c, 44.f * s, C_SURFACE_HAUTE, avecAlpha(C_ROUGE, 0.6f + 0.4f * pulse), 2.5f);
    texteEchelle(app, POLICE_GRAS, "VS", 30, c, C_TEXTE, ALIGNE_CENTRE, s);
    snprintf(buf, sizeof(buf), "TOUR %d", ar->tour + 1);
    pastille(app, buf, (sfVector2f){LARGEUR / 2.f, 128.f}, C_TEXTE_DOUX, 0);
}

/** Détail du dernier coup : le même tableau que la boîte de attaquer(). */
static void resultat(app_t *app, arene_t *ar)
{
    static const char *labels[4] = {"Attaque", "Réduction", "Dégâts", "PV restants"};
    const sfColor cs[4] = {C_TEXTE, C_BLEU, C_ROUGE, C_VERT};
    float t = ar->tResultat;
    float e = sortieCubique(progression(t, 0.f, 0.4f));
    float a = app->alpha;
    sfFloatRect r = {530.f, 372.f + 20.f * (1.f - e), 220.f, 150.f};
    char vals[4][16];

    if (!ar->aAttaque) {
        app->alpha = a * (0.55f + 0.45f * sinf(app->temps * 3.f));
        texte(app, POLICE_NORMALE, "Appuie sur Espace", 15, (sfVector2f){LARGEUR / 2.f,
            420.f}, C_TEXTE_DOUX, ALIGNE_CENTRE);
        texte(app, POLICE_NORMALE, "pour attaquer", 15, (sfVector2f){LARGEUR / 2.f, 442.f},
            C_TEXTE_DOUX, ALIGNE_CENTRE);
        app->alpha = a;
        return;
    }
    snprintf(vals[0], 16, "%d", ar->atk);
    snprintf(vals[1], 16, "-%d %%", ar->def);
    snprintf(vals[2], 16, "%d", (int)roundf(ar->degats * sortieCubique(progression(t,
        0.15f, 0.5f))));
    snprintf(vals[3], 16, "%d", ar->pvRestants);
    app->alpha = a * e;
    ombre(app, r, 18.f, 1.f);
    rectArrondiBord(app, r, 18.f, C_SURFACE, C_BORD);
    for (int i = 0; i < 4; i++) {
        app->alpha = a * e * progression(t, 0.08f + i * 0.07f, 0.3f);
        if (i > 0)
            rectArrondi(app, (sfFloatRect){r.left + 16.f, r.top + 12.f + i * 32.f,
                r.width - 32.f, 1.f}, 0.f, avecAlpha(C_BLANC, 0.06f));
        texte(app, POLICE_NORMALE, labels[i], 14, (sfVector2f){r.left + 18.f,
            r.top + 28.f + i * 32.f}, C_TEXTE_DOUX, ALIGNE_GAUCHE);
        texte(app, POLICE_GRAS, vals[i], i == 2 ? 20 : 16, (sfVector2f){r.left + r.width
            - 18.f, r.top + 28.f + i * 32.f}, cs[i], ALIGNE_DROITE);
    }
    app->alpha = a;
}

static void messageBas(app_t *app, arene_t *ar)
{
    personnage_t *a = &app->persos[ar->ids[0]];
    personnage_t *d = &app->persos[ar->ids[1]];
    sfVector2f pos = {LARGEUR / 2.f, 690.f};
    char buf[128];

    if (a->pv == 0)
        snprintf(buf, sizeof(buf), "%s est K.O. et ne peut plus attaquer  ·  R pour inverser",
            a->name);
    else if (d->pv == 0)
        snprintf(buf, sizeof(buf), "%s est mort", d->name);
    else if (ar->aAttaque)
        snprintf(buf, sizeof(buf), "%s a infligé %d dégât%s à %s",
            app->persos[ar->dernierAtk].name, ar->degats, ar->degats > 1 ? "s" : "",
            app->persos[ar->dernierDef].name);
    else
        snprintf(buf, sizeof(buf), "Dégâts = attaque × (100 - défense) / 100");
    if (d->pv == 0 && a->pv > 0) {
        texte(app, POLICE_SYMBOLES, "☠", 16, (sfVector2f){pos.x - largeurTexte(app,
            POLICE_NORMALE, buf, 15) / 2.f - 14.f, pos.y}, C_ROUGE, ALIGNE_CENTRE);
        texte(app, POLICE_NORMALE, buf, 15, pos, C_ROUGE, ALIGNE_CENTRE);
        return;
    }
    texte(app, POLICE_NORMALE, buf, 15, pos, C_TEXTE_FAIBLE, ALIGNE_CENTRE);
}

static void lancerAttaque(arene_t *ar)
{
    ar->tAttaque = 0.f;
    ar->impact = 0;
}

static void commandes(app_t *app, arene_t *ar)
{
    int peut = peutAttaquer(app, ar);
    int libre = ar->tAttaque < 0.f && ar->echange < 0.f;
    float a = app->alpha;
    float dy = 24.f * (1.f - sortieCubique(progression(app->tScene, 0.4f, 0.5f)));

    if (app->in.touches[sfKeyEscape])
        changerScene(app, SCENE_MENU);
    if ((app->in.touches[sfKeySpace] || app->in.touches[sfKeyEnter]) && peut)
        lancerAttaque(ar);
    else if (app->in.touches[sfKeyR] && libre)
        inverser(ar);
    app->alpha = a * progression(app->tScene, 0.4f, 0.3f);
    if (bouton(app, &ar->retour, (sfFloatRect){GAUCHE_X, 586.f + dy, 220.f, 58.f}, "Menu",
        "Échap", NULL, C_TEXTE_DOUX, 1))
        changerScene(app, SCENE_MENU);
    if (boutonPlein(app, &ar->attaquer, (sfFloatRect){490.f, 578.f + dy, 300.f, 72.f},
        "Attaquer", "Espace", C_ROUGE, peut || ar->tAttaque >= 0.f) && peut)
        lancerAttaque(ar);
    if (bouton(app, &ar->inverser, (sfFloatRect){DROITE_X + CARTE_AL - 220.f, 586.f + dy,
        220.f, 58.f}, "Inverser", "R", NULL, C_BLEU, libre) && libre)
        inverser(ar);
    messageBas(app, ar);
    app->alpha = a;
}

void sceneArene(app_t *app)
{
    arene_t *ar = &app->arene;
    sfVector2f pos[2];
    float a = app->alpha;

    majArene(app, ar);
    pos[0] = positionSlot(app, ar, 0);
    pos[1] = positionSlot(app, ar, 1);
    app->alpha = a * progression(app->tScene, 0.05f, 0.35f);
    etiquette(app, "ARÈNE", (sfVector2f){LARGEUR / 2.f, 56.f}, C_TEXTE_FAIBLE, ALIGNE_CENTRE);
    medaillon(app, ar);
    resultat(app, ar);
    app->alpha = a * progression(app->tScene, 0.15f, 0.35f);
    carteArene(app, ar, 1, pos[1]);
    trainee(app, ar, pos[0]);
    app->alpha = a * progression(app->tScene, 0.1f, 0.35f);
    carteArene(app, ar, 0, pos[0]);
    app->alpha = a;
    effetImpact(app, ar);
    popDegats(app, ar);
    commandes(app, ar);
}
