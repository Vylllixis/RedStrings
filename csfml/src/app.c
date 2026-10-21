/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "gui.h"

#define DUREE_SORTIE 0.22f
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

static void init_poussieres(app_t *app)
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

static int creer_ressources(app_t *app)
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

int creer_app(app_t *app)
{
    sfContextSettings reglages = {0, 0, 8, 1, 1, sfContextDefault, sfFalse};
    sfVideoMode mode = {LARGEUR, HAUTEUR, 32};

    memset(app, 0, sizeof(app_t));
    roster_init(&app->equipe);
    srand(time(NULL));
    app->fenetre = sfRenderWindow_create(mode, "Redstrings",
        sfTitlebar | sfClose, &reglages);
    if (app->fenetre == NULL)
        return 1;
    sfRenderWindow_setVerticalSyncEnabled(app->fenetre, sfTrue);
    if (creer_ressources(app) != 0)
        return 1;
    app->additif.blendMode = sfBlendAdd;
    app->additif.transform = sfTransform_Identity;
    app->additif.texture = NULL;
    app->additif.shader = NULL;
    init_poussieres(app);
    app->alpha = 1.f;
    app->t_transition = -1.f;
    app->ouvert = 1;
    app->scene = SCENE_MENU;
    entrer_menu(app);
    return 0;
}

void detruire_app(app_t *app)
{
    if (app->fenetre)
        sfRenderWindow_destroy(app->fenetre);
    for (int i = 0; i < NB_POLICES; i++) {
        if (app->polices[i])
            sfFont_destroy(app->polices[i]);
    }
    for (int i = 0; i < NB_CURSEURS; i++) {
        if (app->curseurs[i])
            sfCursor_destroy(app->curseurs[i]);
    }
    if (app->texte)
        sfText_destroy(app->texte);
    if (app->forme)
        sfConvexShape_destroy(app->forme);
    if (app->cercle)
        sfCircleShape_destroy(app->cercle);
    if (app->sprite)
        sfSprite_destroy(app->sprite);
    if (app->tex_lueur)
        sfTexture_destroy(app->tex_lueur);
    if (app->tex_vignette)
        sfTexture_destroy(app->tex_vignette);
    if (app->horloge)
        sfClock_destroy(app->horloge);
    if (app->vue)
        sfView_destroy(app->vue);
    roster_liberer(&app->equipe);
}

static void traiter_evenement(app_t *app, sfEvent *e)
{
    if (e->type == sfEvtClosed)
        app->ouvert = 0;
    if (e->type == sfEvtKeyPressed && e->key.code >= 0 && e->key.code < sfKeyCount) {
        app->in.touches[e->key.code] = 1;
        app->in.shift = e->key.shift;
    }
    if (e->type == sfEvtTextEntered && e->text.unicode >= 32
        && e->text.unicode != 127 && app->in.nb_texte < 32)
        app->in.texte[app->in.nb_texte++] = e->text.unicode;
    if (e->type == sfEvtMouseButtonPressed && e->mouseButton.button == sfMouseLeft) {
        app->in.clic = 1;
        app->in.bas = 1;
    }
    if ((e->type == sfEvtMouseButtonReleased && e->mouseButton.button == sfMouseLeft)
        || e->type == sfEvtLostFocus)
        app->in.bas = 0;
    if (e->type == sfEvtMouseLeft)
        app->in.dehors = 1;
    if (e->type == sfEvtMouseEntered || e->type == sfEvtMouseMoved)
        app->in.dehors = 0;
}

void evenements(app_t *app)
{
    sfEvent e;

    app->in.clic = 0;
    app->in.nb_texte = 0;
    memset(app->in.touches, 0, sizeof(app->in.touches));
    while (sfRenderWindow_pollEvent(app->fenetre, &e))
        traiter_evenement(app, &e);
    app->in.souris = sfRenderWindow_mapPixelToCoords(app->fenetre,
        sfMouse_getPositionRenderWindow(app->fenetre),
        sfRenderWindow_getDefaultView(app->fenetre));
    if (app->in.dehors)
        app->in.souris = (sfVector2f){-9999.f, -9999.f};
}

static void entrer_scene(app_t *app, scene_t s)
{
    app->scene = s;
    app->t_scene = 0.f;
    if (s == SCENE_MENU)
        entrer_menu(app);
    if (s == SCENE_CREATION)
        entrer_creation(app);
    if (s == SCENE_LISTE)
        entrer_liste(app);
    if (s == SCENE_SELECTION)
        entrer_selection(app);
    if (s == SCENE_ARENE)
        entrer_arene(app);
}

void changer_scene(app_t *app, scene_t scene)
{
    if (app->t_transition >= 0.f)
        return;
    app->scene_suivante = scene;
    app->t_transition = 0.f;
}

/** Fondu + glissement : la scène sort vers le haut, la suivante arrive du bas. */
static void maj_transition(app_t *app, float *alpha, float *decalage)
{
    float p = 0.f;

    if (app->t_transition >= 0.f) {
        app->t_transition += app->dt;
        p = progression(app->t_transition, 0.f, DUREE_SORTIE);
        if (p < 1.f) {
            *alpha = 1.f - p;
            *decalage = -16.f * entree_cubique(p);
            return;
        }
        entrer_scene(app, app->scene_suivante);
        app->t_transition = -1.f;
    }
    *alpha = progression(app->t_scene, 0.f, 0.25f);
    *decalage = 16.f * (1.f - sortie_cubique(progression(app->t_scene, 0.f, 0.45f)));
}

static void dessiner_scene(app_t *app)
{
    if (app->scene == SCENE_MENU)
        scene_menu(app);
    if (app->scene == SCENE_CREATION)
        scene_creation(app);
    if (app->scene == SCENE_LISTE)
        scene_liste(app);
    if (app->scene == SCENE_SELECTION)
        scene_selection(app);
    if (app->scene == SCENE_ARENE)
        scene_arene(app);
    if (app->scene == SCENE_AU_REVOIR)
        scene_au_revoir(app);
}

static void maj_curseur(app_t *app)
{
    if (app->curseur_voulu == app->curseur_actuel
        || app->curseurs[app->curseur_voulu] == NULL)
        return;
    sfRenderWindow_setMouseCursor(app->fenetre, app->curseurs[app->curseur_voulu]);
    app->curseur_actuel = app->curseur_voulu;
}

void maj_app(app_t *app, float dt)
{
    float alpha = 1.f;
    float decalage = 0.f;

    app->dt = bornerf(dt, 0.f, 1.f / 30.f);
    app->temps += app->dt;
    app->t_scene += app->dt;
    app->dt_jeu = app->hitstop > 0.f ? app->dt * 0.06f : app->dt;
    app->hitstop -= app->dt;
    app->secousse = approcher(app->secousse, 0.f, 7.f, app->dt);
    app->curseur_voulu = CURSEUR_FLECHE;
    maj_transition(app, &alpha, &decalage);
    app->souris_bloquee = app->t_transition >= 0.f;
    if (app->souris_bloquee) {
        app->in.clic = 0;
        app->in.nb_texte = 0;
        memset(app->in.touches, 0, sizeof(app->in.touches));
    }
    sfRenderWindow_clear(app->fenetre, C_FOND);
    sfRenderWindow_setView(app->fenetre, sfRenderWindow_getDefaultView(app->fenetre));
    app->alpha = 1.f;
    dessiner_fond(app);
    sfView_setCenter(app->vue, (sfVector2f){
        LARGEUR / 2.f + sinf(app->temps * 83.f) * 12.f * app->secousse,
        HAUTEUR / 2.f + decalage + cosf(app->temps * 97.f) * 9.f * app->secousse});
    sfRenderWindow_setView(app->fenetre, app->vue);
    app->alpha = alpha;
    dessiner_scene(app);
    app->alpha = 1.f;
    maj_particules(app);
    dessiner_particules(app);
    sfRenderWindow_setView(app->fenetre, sfRenderWindow_getDefaultView(app->fenetre));
    dessiner_toasts(app);
    maj_curseur(app);
}
