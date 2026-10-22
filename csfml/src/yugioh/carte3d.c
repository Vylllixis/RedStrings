/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Une carte dans l'espace : la face (ou le dos) est plaquée sur une grille
 * projetée en perspective, ce qui permet de la retourner, de l'incliner vers
 * la souris et de la faire basculer. Par-dessus : ombre, halo, reflet
 * holographique qui la traverse, lumière qui court le long du cadre.
 */

#include <math.h>
#include "personnage.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_types.h"
#include "gui_yugioh.h"
#include "yugioh_interne.h"

#define PERSPECTIVE 1100.f
#define GRILLE_X 10
#define GRILLE_Y 14
#define NB_POINTS ((GRILLE_X + 1) * (GRILLE_Y + 1))
#define NB_SOMMETS (GRILLE_X * GRILLE_Y * 4)

carte3d_t carte3d_pose(sfVector2f centre, float largeur)
{
    carte3d_t k = {0};

    k.centre = centre;
    k.largeur = largeur;
    k.alpha = 1.f;
    return k;
}

/** Rotation autour de Y (retournement), puis X (bascule), puis Z (inclinaison). */
static sfVector3f tourner(const carte3d_t *k, float x, float y)
{
    float x1 = x * cosf(k->rot_y);
    float z1 = -x * sinf(k->rot_y);
    float y2 = y * cosf(k->rot_x) - z1 * sinf(k->rot_x);
    float z2 = y * sinf(k->rot_x) + z1 * cosf(k->rot_x);

    return (sfVector3f){x1 * cosf(k->rot_z) - y2 * sinf(k->rot_z),
        x1 * sinf(k->rot_z) + y2 * cosf(k->rot_z), z2};
}

/** Point (u, v) de la carte (0..1, origine en haut à gauche) projeté à l'écran. */
sfVector2f carte3d_point(const carte3d_t *k, float u, float v)
{
    float l = k->largeur;
    float h = l * FACE_H / FACE_L;
    sfVector3f p = tourner(k, (u - 0.5f) * l, (v - 0.5f) * h);
    float f = PERSPECTIVE / fmaxf(PERSPECTIVE + p.z, 1.f);

    return (sfVector2f){k->centre.x + p.x * f, k->centre.y + p.y * f};
}

/** La face est tournée vers nous si la normale pointe vers la caméra. */
int carte3d_face_visible(const carte3d_t *k)
{
    return cosf(k->rot_y) * cosf(k->rot_x) > 0.f;
}

static void grille(const carte3d_t *k, sfVector2f p[NB_POINTS])
{
    for (int j = 0; j <= GRILLE_Y; j++)
        for (int i = 0; i <= GRILLE_X; i++)
            p[j * (GRILLE_X + 1) + i] = carte3d_point(k, (float)i / GRILLE_X,
                (float)j / GRILLE_Y);
}

/** Les quatre sommets du carreau (i, j) dans l'ordre des sfQuads. */
static void carreau(int i, int j, int idx[4])
{
    idx[0] = j * (GRILLE_X + 1) + i;
    idx[1] = idx[0] + 1;
    idx[2] = idx[1] + GRILLE_X + 1;
    idx[3] = idx[0] + GRILLE_X + 1;
}

/** Ombre portée : la silhouette de la carte, décalée et élargie, en couches. */
static void ombre_portee(app_t *app, const carte3d_t *k, float a)
{
    sfVector2f coins[4] = {carte3d_point(k, 0.f, 0.f), carte3d_point(k, 1.f, 0.f),
        carte3d_point(k, 1.f, 1.f), carte3d_point(k, 0.f, 1.f)};
    sfVertex v[4];
    float e = 0.f;

    for (int n = 4; n >= 1; n--) {
        e = 1.f + 0.035f * n * k->ombre;
        for (int i = 0; i < 4; i++)
            v[i] = (sfVertex){{k->centre.x + (coins[i].x - k->centre.x) * e,
                k->centre.y + (coins[i].y - k->centre.y) * e + 7.f * n * k->ombre},
                avec_alpha(C_NOIR, 0.09f * k->ombre * a * (1.f - n / 5.f)), {0, 0}};
        cible_primitives(app, v, 4, sfQuads, NULL);
    }
}

static void face_texturee(app_t *app, classe_t c, const carte3d_t *k,
    const sfVector2f p[NB_POINTS], float a)
{
    int face = carte3d_face_visible(k);
    const sfRenderTexture *rt = face ? app->yugioh.face[c] : app->yugioh.dos;
    const sfTexture *tex = rt ? sfRenderTexture_getTexture(rt) : NULL;
    float lum = 0.62f + 0.38f * fabsf(cosf(k->rot_y) * cosf(k->rot_x));
    sfColor col = melanger((sfColor){255, 255, 255, 255}, (sfColor){118, 118, 132, 255},
        k->gris);
    sfRenderStates etats = {sfBlendAlpha, sfTransform_Identity, tex, NULL};
    sfVertex v[NB_SOMMETS];
    int idx[4];
    int n = 0;
    float u = 0.f;

    if (tex == NULL)
        return;
    col = (sfColor){(sfUint8)(col.r * lum), (sfUint8)(col.g * lum),
        (sfUint8)(col.b * lum), (sfUint8)(255.f * a)};
    for (int j = 0; j < GRILLE_Y; j++) {
        for (int i = 0; i < GRILLE_X; i++) {
            carreau(i, j, idx);
            for (int s = 0; s < 4; s++) {
                u = (float)(idx[s] % (GRILLE_X + 1)) / GRILLE_X;
                v[n++] = (sfVertex){p[idx[s]], col, {(face ? u : 1.f - u) * FACE_L,
                    (float)(idx[s] / (GRILLE_X + 1)) / GRILLE_Y * FACE_H}};
            }
        }
    }
    cible_primitives(app, v, NB_SOMMETS, sfQuads, &etats);
}

/** Teinte arc-en-ciel du film holographique (h de 0 à 1). */
static sfColor irise(float h, float a)
{
    float r = 0.5f + 0.5f * cosf(2.f * PI * (h + 0.f));
    float g = 0.5f + 0.5f * cosf(2.f * PI * (h + 0.33f));
    float b = 0.5f + 0.5f * cosf(2.f * PI * (h + 0.67f));

    return (sfColor){(sfUint8)(160.f + 95.f * r), (sfUint8)(160.f + 95.f * g),
        (sfUint8)(160.f + 95.f * b), (sfUint8)bornerf(a * 255.f, 0.f, 255.f)};
}

/**
 * Reflet holographique : une bande irisée traverse la carte en diagonale à
 * intervalles réguliers, et un point de lumière suit son inclinaison.
 */
static void reflet(app_t *app, const carte3d_t *k, const sfVector2f p[NB_POINTS],
    float a)
{
    float s = fmodf(app->temps * 0.32f + k->centre.x * 0.0011f, 1.8f) - 0.4f;
    float su = 0.5f - sinf(k->rot_y) * 0.9f;
    float sv = 0.5f + sinf(k->rot_x) * 0.9f;
    sfVertex v[NB_SOMMETS];
    int idx[4];
    int n = 0;
    float u = 0.f;
    float w = 0.f;
    float d = 0.f;
    float f = 0.f;

    for (int j = 0; j < GRILLE_Y; j++) {
        for (int i = 0; i < GRILLE_X; i++) {
            carreau(i, j, idx);
            for (int c = 0; c < 4; c++) {
                u = (float)(idx[c] % (GRILLE_X + 1)) / GRILLE_X;
                w = (float)(idx[c] / (GRILLE_X + 1)) / GRILLE_Y;
                d = (u * 0.75f + w * 0.45f) / 1.2f - s;
                f = 0.85f * expf(-d * d / 0.005f) + 0.45f * expf(-((u - su) * (u - su)
                    + (w - sv) * (w - sv)) / 0.07f) + 0.05f;
                v[n++] = (sfVertex){p[idx[c]], irise(u * 0.9f + w * 0.6f + k->rot_y * 0.8f
                    + app->temps * 0.05f, f * k->reflet * a), {0, 0}};
            }
        }
    }
    cible_primitives(app, v, NB_SOMMETS, sfQuads, &app->additif);
}

void carte3d(app_t *app, classe_t c, const carte3d_t *k)
{
    sfVector2f p[NB_POINTS];
    float a = bornerf(k->alpha, 0.f, 1.f);
    float h = k->largeur * FACE_H / FACE_L;
    sfColor halo = k->halo;

    if (!yugioh_actif(app) || (unsigned)c >= NB_CLASSES || a * app->alpha < 0.004f)
        return;
    grille(k, p);
    if (k->ombre > 0.f)
        ombre_portee(app, k, a);
    if (halo.a > 0) {
        halo.a = (sfUint8)(halo.a * a);
        lueur(app, k->centre, (sfVector2f){k->largeur * 1.05f, h * 0.85f}, halo);
    }
    face_texturee(app, c, k, p, a);
    if (k->reflet > 0.f)
        reflet(app, k, p, a);
    if (k->bord > 0.f)
        carte3d_bord_lumineux(app, k, a);
}
