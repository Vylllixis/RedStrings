/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ ressources.c ───────────────────────────────────────────────────────
** │ But         Ressources de l'application : polices, textures de halo
** │             et de vignette, poussières d'ambiance.
** │ Contenu     chemin_ressource, charger_police, creer_tex_lueur,
** │             creer_tex_vignette, app_init_poussieres,
** │             app_creer_ressources
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_types.h"
#include "app_interne.h"

#define TAILLE_LUEUR 128
#define TAILLE_VIGNETTE 256

static const char *FICHIERS_POLICES[NB_POLICES] = {
    "NotoSans-Bold.ttf",
    "NotoSans-Regular.ttf",
    "DejaVuSans-Bold.ttf"
};

/** Dossiers de ressources, relatifs à la base (exécutable, puis dossier courant). */
static const char *DOSSIERS_RESSOURCES[] = {"csfml/assets", "assets", "../csfml/assets"};
#define NB_DOSSIERS (int)(sizeof(DOSSIERS_RESSOURCES) / sizeof(DOSSIERS_RESSOURCES[0]))

static int chercher_depuis(const char *base, const char *relatif, char *chemin,
    size_t taille)
{
    int n = 0;

    for (int i = 0; i < NB_DOSSIERS; i++) {
        n = snprintf(chemin, taille, "%s/%s/%s", base, DOSSIERS_RESSOURCES[i], relatif);
        if (n > 0 && (size_t)n < taille && access(chemin, R_OK) == 0)
            return 0;
    }
    return -1;
}

/*
** ╭─ chemin_ressource ───────────────────────────────────────────────────
** │ Trouve le chemin d'un fichier de ressource (« fonts/X.ttf », «
** │ yugioh/X.jpg »).
** │
** │ 1. cherche à côté de l'exécutable (son dossier est lu dans
** │    /proc/self/exe)
** │ 2. sinon cherche depuis le dossier courant
** │ Le programme marche donc quel que soit l'endroit d'où on le lance.
** │ Renvoie 0 si le fichier est trouvé.
** ╰──────────────────────────────────────────────────────────────────────
*/
int chemin_ressource(const char *relatif, char *chemin, size_t taille)
{
    char exe[1024] = {0};
    char *slash = NULL;
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);

    if (n <= 0)
        snprintf(exe, sizeof(exe), ".");
    slash = strrchr(exe, '/');
    if (n > 0 && slash)
        *slash = '\0';
    if (chercher_depuis(exe, relatif, chemin, taille) == 0)
        return 0;
    return chercher_depuis(".", relatif, chemin, taille);
}

static sfFont *charger_police(const char *nom)
{
    char relatif[128];
    char chemin[1200];

    snprintf(relatif, sizeof(relatif), "fonts/%s", nom);
    if (chemin_ressource(relatif, chemin, sizeof(chemin)) != 0)
        return NULL;
    return sfFont_createFromFile(chemin);
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

    if (img == NULL)
        return NULL;
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
    sfImage_destroy(img);
    if (tex != NULL)
        sfTexture_setSmooth(tex, sfTrue);
    return tex;
}

static sfTexture *creer_tex_vignette(void)
{
    sfImage *img = sfImage_create(TAILLE_VIGNETTE, TAILLE_VIGNETTE);
    sfTexture *tex = NULL;
    float dx = 0.f;
    float dy = 0.f;

    if (img == NULL)
        return NULL;
    for (int y = 0; y < TAILLE_VIGNETTE; y++) {
        for (int x = 0; x < TAILLE_VIGNETTE; x++) {
            dx = (x + 0.5f) / (TAILLE_VIGNETTE / 2.f) - 1.f;
            dy = (y + 0.5f) / (TAILLE_VIGNETTE / 2.f) - 1.f;
            sfImage_setPixel(img, x, y, (sfColor){4, 5, 12,
                (sfUint8)(lisser(0.45f, 1.45f, sqrtf(dx * dx + dy * dy)) * 210.f)});
        }
    }
    tex = sfTexture_createFromImage(img, NULL);
    sfImage_destroy(img);
    if (tex != NULL)
        sfTexture_setSmooth(tex, sfTrue);
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

/*
** ╭─ app_creer_ressources ───────────────────────────────────────────────
** │ Crée d'un coup les ressources SFML réutilisées à chaque image :
** │ polices, objets de dessin (texte, forme, cercle, sprite), textures de
** │ halo et de vignette, horloge, vues, curseurs.
** │
** │ Chaque création est vérifiée : à la moindre erreur, un message sur
** │ stderr et la fonction renvoie 1 (le programme s'arrête proprement).
** │ Renvoie 0 si tout est prêt.
** ╰──────────────────────────────────────────────────────────────────────
*/
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
    app->vue_fixe = sfView_createFromRect((sfFloatRect){0, 0, LARGEUR, HAUTEUR});
    app->vue_zoom = sfView_createFromRect((sfFloatRect){0, 0, LARGEUR, HAUTEUR});
    app->curseurs[CURSEUR_FLECHE] = sfCursor_createFromSystem(sfCursorArrow);
    app->curseurs[CURSEUR_MAIN] = sfCursor_createFromSystem(sfCursorHand);
    app->curseurs[CURSEUR_TEXTE] = sfCursor_createFromSystem(sfCursorText);
    if (!app->texte || !app->forme || !app->cercle || !app->sprite || !app->tex_lueur
        || !app->tex_vignette || !app->horloge || !app->vue || !app->vue_fixe || !app->vue_zoom) {
        fprintf(stderr, "redstrings_csfml : ressource graphique impossible à créer\n");
        return 1;
    }
    return 0;
}
