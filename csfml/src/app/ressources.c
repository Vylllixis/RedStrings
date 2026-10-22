/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Ressources : polices, textures de halo et de vignette, poussières d'ambiance.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "my_string.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_types.h"
#include "app_interne.h"

#define TAILLE_LUEUR 128

static const char *FICHIERS_POLICES[NB_POLICES] = {
    "NotoSans-Bold.ttf",
    "NotoSans-Regular.ttf",
    "DejaVuSans-Bold.ttf"
};

/** Cherche la police à côté de l'exécutable, puis depuis le dossier courant. */
static sfFont *charger_police(const char *nom)
{
    const char *formats[] = {"%s/csfml/assets/fonts/%s", "%s/assets/fonts/%s",
        "%s/../csfml/assets/fonts/%s"};
    char exe[1024] = {0};
    char chemin[1200];
    char *slash = NULL;
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);

    if (n <= 0)
        my_strcpy(exe, ".");
    slash = strrchr(exe, '/');
    if (n > 0 && slash)
        *slash = '\0';
    for (int i = 0; i < 3; i++) {
        if (snprintf(chemin, sizeof(chemin), formats[i], exe, nom)
            < (int)sizeof(chemin) && access(chemin, R_OK) == 0)
            return sfFont_createFromFile(chemin);
    }
    for (int i = 0; i < 3; i++) {
        if (snprintf(chemin, sizeof(chemin), formats[i], ".", nom)
            < (int)sizeof(chemin) && access(chemin, R_OK) == 0)
            return sfFont_createFromFile(chemin);
    }
    return NULL;
}

static float lisser(float a, float b, float x)
{
    float t = bornerf((x - a) / (b - a), 0.f, 1.f);

    return t * t * (3.f - 2.f * t);
}

/** Texture de dégradé radial blanc → transparent, base de tous les halos. */
static sfTexture *creer_tex_lueur(void)
{
    sfImage *img = sfImage_create(TAILLE_LUEUR, TAILLE_LUEUR);
    sfTexture *tex = NULL;
    float dx = 0.f;
    float dy = 0.f;
    float d = 0.f;

    for (int y = 0; y < TAILLE_LUEUR; y++) {
        for (int x = 0; x < TAILLE_LUEUR; x++) {
            dx = (x + 0.5f) / (TAILLE_LUEUR / 2.f) - 1.f;
            dy = (y + 0.5f) / (TAILLE_LUEUR / 2.f) - 1.f;
            d = sqrtf(dx * dx + dy * dy);
            sfImage_setPixel(img, x, y, (sfColor){255, 255, 255,
                (sfUint8)(d >= 1.f ? 0.f : powf(1.f - d, 2.2f) * 255.f)});
        }
    }
    tex = sfTexture_createFromImage(img, NULL);
    sfTexture_setSmooth(tex, sfTrue);
    sfImage_destroy(img);
    return tex;
}

static sfTexture *creer_tex_vignette(void)
{
    sfImage *img = sfImage_create(256, 256);
    sfTexture *tex = NULL;
    float dx = 0.f;
    float dy = 0.f;

    for (int y = 0; y < 256; y++) {
        for (int x = 0; x < 256; x++) {
            dx = (x + 0.5f) / 128.f - 1.f;
            dy = (y + 0.5f) / 128.f - 1.f;
            sfImage_setPixel(img, x, y, (sfColor){4, 5, 12,
                (sfUint8)(lisser(0.45f, 1.45f, sqrtf(dx * dx + dy * dy)) * 210.f)});
        }
    }
    tex = sfTexture_createFromImage(img, NULL);
    sfTexture_setSmooth(tex, sfTrue);
    sfImage_destroy(img);
    return tex;
}

void app_init_poussieres(app_t *app)
{
    for (int i = 0; i < NB_POUSSIERES; i++) {
        app->poussieres[i].x = aleatoire(0.f, LARGEUR);
        app->poussieres[i].y = aleatoire(0.f, HAUTEUR);
        app->poussieres[i].vitesse = aleatoire(6.f, 22.f);
        app->poussieres[i].phase = aleatoire(0.f, 2.f * PI);
        app->poussieres[i].taille = aleatoire(1.f, 3.2f);
    }
    for (int i = 0; i < NB_ETOILES; i++) {
        app->etoiles[i].x = aleatoire(0.f, LARGEUR);
        app->etoiles[i].y = aleatoire(0.f, HAUTEUR);
        app->etoiles[i].vitesse = aleatoire(1.f, 5.f);
        app->etoiles[i].phase = aleatoire(0.f, 2.f * PI);
        app->etoiles[i].taille = aleatoire(1.f, 2.2f);
    }
}

int app_creer_ressources(app_t *app)
{
    for (int i = 0; i < NB_POLICES; i++) {
        app->polices[i] = charger_police(FICHIERS_POLICES[i]);
        if (app->polices[i] == NULL) {
            fprintf(stderr, "redstrings_csfml : police introuvable (%s)\n",
                FICHIERS_POLICES[i]);
            return 1;
        }
    }
    app->texte = sfText_create();
    app->forme = sfConvexShape_create();
    app->cercle = sfCircleShape_create();
    app->sprite = sfSprite_create();
    app->tex_lueur = creer_tex_lueur();
    app->tex_vignette = creer_tex_vignette();
    app->horloge = sfClock_create();
    app->vue = sfView_createFromRect((sfFloatRect){0, 0, LARGEUR, HAUTEUR});
    app->curseurs[CURSEUR_FLECHE] = sfCursor_createFromSystem(sfCursorArrow);
    app->curseurs[CURSEUR_MAIN] = sfCursor_createFromSystem(sfCursorHand);
    app->curseurs[CURSEUR_TEXTE] = sfCursor_createFromSystem(sfCursorText);
    return 0;
}
