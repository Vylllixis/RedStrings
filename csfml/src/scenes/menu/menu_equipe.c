/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ menu_equipe.c ──────────────────────────────────────────────────────
** │ But         Menu : aperçu de l'équipe, une case par personnage.
** │ Contenu     emplacement, menu_panneau_equipe
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include <stdio.h>
#include "personnage.h"
#include "sauvegarde.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_emblemes.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "gui_ui.h"
#include "menu_interne.h"

#define EQ_RECT ((sfFloatRect){700.f, 262.f, 430.f, 326.f})

/** Emplacement d'équipe : personnage réel, ou place libre (la suivante pulse). */
static void emplacement(app_t *app, int i, sfVector2f c, float e)
{
    personnage_t *p = NULL;
    float *h = app->menu.survol_equipe;
    sfColor col;
    float mort = 0.f;
    char nom[32];

    if (i >= app->equipe.len) {
        hexagone(app, c, 26.f * e, 0.f, avec_alpha(C_BLANC, 0.02f),
            avec_alpha(C_BLANC, 0.08f));
        if (i == app->equipe.len) {
            hexagone(app, c, 26.f * e, 0.f, C_TRANSPARENT, avec_alpha(C_CYAN,
                0.35f + 0.35f * sinf(app->temps * 3.f)));
            texte(app, POLICE_GRAS, "+", 22, c, avec_alpha(C_CYAN, 0.8f), ALIGNE_CENTRE);
        }
        return;
    }
    p = &app->equipe.data[i];
    mort = est_ko(p) ? 1.f : 0.f;
    col = griser(couleur_classe(p->classe), mort);
    h[i] = approcher(h[i], survole(app, (sfFloatRect){c.x - 32.f, c.y - 32.f, 64.f,
        76.f}) ? 1.f : 0.f, 14.f, app->dt);
    c.y -= 4.f * h[i];
    lueur(app, c, (sfVector2f){70.f + 20.f * h[i], 70.f + 20.f * h[i]},
        avec_alpha(col, (0.18f + 0.2f * h[i]) * (1.f - 0.6f * mort)));
    hexagone(app, c, (28.f + 2.f * h[i]) * e, 0.f, melanger((sfColor){14, 16, 32, 255},
        col, 0.2f), avec_alpha(col, 0.6f + 0.4f * h[i]));
    embleme(app, p->classe, c, 13.f * e, melanger(col, C_BLANC, 0.5f));
    tronquer(app, POLICE_GRAS, 12, p->nom, 72.f, nom, sizeof(nom));
    texte(app, POLICE_GRAS, nom, 12, (sfVector2f){c.x, c.y + 44.f},
        melanger(C_TEXTE_DOUX, C_TEXTE, h[i]), ALIGNE_CENTRE);
    if (mort > 0.f)
        pastille(app, "K.O.", (sfVector2f){c.x, c.y - 30.f}, C_ROUGE, 1);
}

/** Panneau de l'équipe : compte, état, emplacements et sauvegarde. */
void menu_panneau_equipe(app_t *app, float t)
{
    sfFloatRect r = EQ_RECT;
    float a = app->alpha;
    float e = sortie_cubique(progression(t, 0.55f, 0.6f));
    float sw = (r.width - 48.f - 9 * 4.f) / MAX_PERSONNAGES;
    float x = 0.f;
    char buf[48];

    r.left += 40.f * (1.f - e);
    app->alpha = a * progression(t, 0.55f, 0.4f);
    ombre_biseau(app, r, 22.f, 1.2f);
    panneau(app, r, 22.f, (sfColor){24, 29, 52, 225}, (sfColor){12, 14, 28, 235},
        C_BORD);
    crochets(app, (sfFloatRect){r.left - 6.f, r.top - 6.f, r.width + 12.f, r.height
        + 12.f}, 18.f, avec_alpha(C_ROUGE, 0.5f));
    etiquette(app, "ÉQUIPE", (sfVector2f){r.left + 24.f, r.top + 30.f}, C_TEXTE_DOUX,
        ALIGNE_GAUCHE);
    snprintf(buf, sizeof(buf), "/ %d", MAX_PERSONNAGES);
    x = r.left + r.width - 24.f - texte(app, POLICE_NORMALE, buf, 15, (sfVector2f){
        r.left + r.width - 24.f, r.top + 32.f}, C_TEXTE_FAIBLE, ALIGNE_DROITE) - 6.f;
    snprintf(buf, sizeof(buf), "%d", app->equipe.len);
    texte(app, POLICE_GRAS, buf, 28, (sfVector2f){x, r.top + 30.f}, C_TEXTE,
        ALIGNE_DROITE);
    snprintf(buf, sizeof(buf), "%d en forme  •  %d K.O.", app->equipe.len - menu_nb_ko(app),
        menu_nb_ko(app));
    texte(app, POLICE_NORMALE, app->equipe.len ? buf : "Aucun personnage recruté", 13,
        (sfVector2f){r.left + 24.f, r.top + 56.f}, C_TEXTE_FAIBLE, ALIGNE_GAUCHE);
    for (int i = 0; i < MAX_PERSONNAGES; i++)
        panneau(app, (sfFloatRect){r.left + 24.f + i * (sw + 4.f), r.top + 74.f, sw, 4.f},
            0.f, i < app->equipe.len ? couleur_classe(app->equipe.data[i].classe)
            : avec_alpha(C_BLANC, 0.08f), i < app->equipe.len
            ? couleur_classe(app->equipe.data[i].classe) : avec_alpha(C_BLANC, 0.08f),
            C_TRANSPARENT);
    for (int i = 0; i < MAX_PERSONNAGES; i++)
        emplacement(app, i, (sfVector2f){r.left + 57.f + (i % 5) * 79.f,
            r.top + 126.f + (i / 5) * 86.f}, sortie_rebond(progression(t, 0.7f + i * 0.04f,
            0.45f)));
    x = r.left + 24.f;
    for (int k = 0; k < 2; k++) {
        touche_clavier(app, k ? "5" : "4", (sfVector2f){x + 13.f, r.top + r.height - 24.f},
            C_VERT, 0.f);
        x += 34.f + texte(app, POLICE_NORMALE, k ? "Charger" : "Sauvegarder", 13,
            (sfVector2f){x + 34.f, r.top + r.height - 24.f}, C_TEXTE_DOUX,
            ALIGNE_GAUCHE) + 22.f;
    }
    texte(app, POLICE_NORMALE, FICHIER_SAUVEGARDE, 12, (sfVector2f){r.left + r.width
        - 24.f, r.top + r.height - 24.f}, C_TEXTE_FAIBLE, ALIGNE_DROITE);
    app->alpha = a;
}
