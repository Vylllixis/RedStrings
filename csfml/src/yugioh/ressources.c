/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ ressources.c ───────────────────────────────────────────────────────
** │ But         Ressources du mode Yu-Gi-Oh! : illustrations
** │             (csfml/assets/yugioh/<code>.jpg) et faces imprimées une
** │             fois pour toutes dans des textures. Rien n'est chargé en
** │             mode normal.
** │ Tolérance   une illustration absente n'empêche pas de jouer : la face
** │             montre alors l'emblème de la classe
** │ Contenu     yugioh_actif, charger_illustration, imprimer,
** │             yugioh_charger, yugioh_liberer
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include "personnage.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_types.h"
#include "gui_yugioh.h"

int yugioh_actif(const app_t *app)
{
    return app->mode == MODE_YUGIOH;
}

static sfTexture *charger_illustration(classe_t c)
{
    char relatif[64];
    char chemin[1200];
    sfTexture *t = NULL;

    snprintf(relatif, sizeof(relatif), "yugioh/%lu.jpg", carte_de_classe(c)->code);
    if (chemin_ressource(relatif, chemin, sizeof(chemin)) != 0)
        return NULL;
    t = sfTexture_createFromFile(chemin, NULL);
    if (t != NULL) {
        sfTexture_setSmooth(t, sfTrue);
        sfTexture_generateMipmap(t);
    }
    return t;
}

/*
** ╭─ imprimer ───────────────────────────────────────────────────────────
** │ Dessine une face (ou le dos si `c` vaut -1) dans une nouvelle
** │ texture.
** │
** │ 1. app->cible = la texture : toutes les primitives (cible.c)
** │    dessinent alors dedans au lieu de la fenêtre
** │ 2. dessiner_dos() ou dessiner_face(), puis app->cible = NULL
** │ 3. display, lissage et mipmaps pour un rendu net à toute taille
** │ Renvoie NULL si la texture ne peut pas être créée.
** ╰──────────────────────────────────────────────────────────────────────
*/
static sfRenderTexture *imprimer(app_t *app, int c)
{
    sfContextSettings reglages = {0, 0, 4, 1, 1, sfContextDefault, sfFalse};
    sfRenderTexture *rt = sfRenderTexture_createWithSettings(FACE_L, FACE_H, &reglages);

    if (rt == NULL)
        return NULL;
    sfRenderTexture_clear(rt, C_TRANSPARENT);
    app->cible = rt;
    if (c < 0)
        dessiner_dos(app);
    else
        dessiner_face(app, (classe_t)c);
    app->cible = NULL;
    sfRenderTexture_display(rt);
    sfRenderTexture_setSmooth(rt, sfTrue);
    sfRenderTexture_generateMipmap(rt);
    return rt;
}

/*
** ╭─ yugioh_charger ─────────────────────────────────────────────────────
** │ Prépare le mode Yu-Gi-Oh! au lancement.
** │
** │ 1. charge l'illustration de chaque classe ; une manquante n'est pas
** │    grave (message sur stderr, la face montrera l'emblème)
** │ 2. imprime le dos commun, puis la face de chaque classe, dans des
** │    textures : le dessin se fait une seule fois
** │ 3. renvoie -1 si une texture ne peut pas être créée
** ╰──────────────────────────────────────────────────────────────────────
*/
int yugioh_charger(app_t *app)
{
    yugioh_t *y = &app->yugioh;

    for (int c = 0; c < NB_CLASSES; c++) {
        y->art[c] = charger_illustration((classe_t)c);
        y->nb_illustrations += y->art[c] != NULL;
    }
    if (y->nb_illustrations < NB_CLASSES)
        fprintf(stderr, "redstrings_csfml : %d illustration(s) sur %d dans "
            "csfml/assets/yugioh/ (voir telecharger.sh) ; les autres cartes "
            "montrent l'emblème de leur classe\n", y->nb_illustrations, NB_CLASSES);
    y->dos = imprimer(app, -1);
    if (y->dos == NULL)
        return -1;
    for (int c = 0; c < NB_CLASSES; c++) {
        y->face[c] = imprimer(app, c);
        if (y->face[c] == NULL)
            return -1;
    }
    return 0;
}

void yugioh_liberer(app_t *app)
{
    yugioh_t *y = &app->yugioh;

    for (int c = 0; c < NB_CLASSES; c++) {
        if (y->art[c] != NULL)
            sfTexture_destroy(y->art[c]);
        if (y->face[c] != NULL)
            sfRenderTexture_destroy(y->face[c]);
        y->art[c] = NULL;
        y->face[c] = NULL;
    }
    if (y->dos != NULL)
        sfRenderTexture_destroy(y->dos);
    y->dos = NULL;
    y->nb_illustrations = 0;
}
