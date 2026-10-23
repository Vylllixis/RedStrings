/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ menu_titre.c ───────────────────────────────────────────────────────
** │ But         Menu : logo animé, fil rouge, rappel des touches et
** │             crédit de la direction visuelle.
** │ Contenu     menu_fil_rouge, cadre_logo, menu_titre, menu_aide_menu,
** │             menu_credit_claude
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include "my_string.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "menu_interne.h"

#define TITRE_Y 112.f

#define TAILLE_TITRE 84

/** Fil rouge qui se déroule depuis le centre, avec une perle qui le parcourt. */
void menu_fil_rouge(app_t *app, float y, float demi, float p)
{
    sfVector2f pts[64];
    float u = 0.f;
    float x = 0.f;
    int perle = 0;

    if (p <= 0.f)
        return;
    for (int i = 0; i < 64; i++) {
        u = i / 63.f;
        x = LARGEUR / 2.f - demi * p + 2.f * demi * p * u;
        pts[i] = (sfVector2f){x, y + sinf(x * 0.035f + app->temps * 3.f) * 4.f
            * sinf(PI * u)};
    }
    ligne_epaisse(app, pts, 64, 12.f, avec_alpha(C_ROUGE, 0.12f), 1);
    ligne_epaisse(app, pts, 64, 2.5f, C_ROUGE, 0);
    cercle(app, pts[0], 4.f, C_ROUGE, C_TRANSPARENT, 0.f);
    cercle(app, pts[63], 4.f, C_ROUGE, C_TRANSPARENT, 0.f);
    perle = (int)(fmodf(app->temps * 0.35f, 1.f) * 63.f);
    lueur(app, pts[perle], (sfVector2f){26.f, 26.f}, avec_alpha(C_ROUGE, 0.8f * p));
    lueur(app, pts[perle], (sfVector2f){7.f, 7.f}, avec_alpha(C_BLANC, 0.9f * p));
}

/** Surtitre et « ailes » lumineuses qui encadrent le logo. */
static void cadre_logo(app_t *app, float t)
{
    float e = sortie_cubique(progression(t, 0.f, 1.2f));
    float a = app->alpha;
    sfVector2f aile[2];
    float place_aile = app->ecran.width / 2.f - 330.f - 60.f;

    app->alpha = a * e;
    losange(app, (sfVector2f){LARGEUR / 2.f - 138.f, 44.f}, 3.f, C_ROUGE);
    losange(app, (sfVector2f){LARGEUR / 2.f + 138.f, 44.f}, 3.f, C_ROUGE);
    etiquette(app, "COMBAT AU TOUR PAR TOUR", (sfVector2f){LARGEUR / 2.f, 44.f},
        melanger(C_ROUGE, C_TEXTE_DOUX, 0.35f), ALIGNE_CENTRE);
    for (int k = 0; k < 2; k++) {
        aile[0] = (sfVector2f){LARGEUR / 2.f + (k ? 1.f : -1.f) * (330.f + 40.f * (1.f - e)),
            TITRE_Y + 4.f};
        aile[1] = (sfVector2f){aile[0].x + (k ? 1.f : -1.f) * fminf(220.f, place_aile) * e,
            TITRE_Y + 4.f};
        ligne_epaisse(app, aile, 2, 1.f, avec_alpha(C_ROUGE, 0.45f), 0);
        losange(app, aile[0], 4.f, C_ROUGE);
        losange(app, aile[1], 2.5f, avec_alpha(C_ROUGE, 0.6f));
    }
    app->alpha = a;
}

/** « REDSTRINGS » : chaque lettre tombe avec un léger rebond puis ondule. */
void menu_titre(app_t *app, float t)
{
    const char *mot = "REDSTRINGS";
    float avances[10];
    float total = 0.f;
    float x = 0.f;
    float y = 0.f;
    float e = 0.f;
    float d = 0.f;
    float a = app->alpha;
    char lettre[2] = {0, 0};

    for (int i = 0; i < 10; i++) {
        avances[i] = sfFont_getGlyph(app->polices[POLICE_GRAS], mot[i], TAILLE_TITRE,
            sfFalse, 0.f).advance + 6.f;
        total += avances[i];
    }
    cadre_logo(app, t);
    x = LARGEUR / 2.f - total / 2.f;
    lueur(app, (sfVector2f){LARGEUR / 2.f, TITRE_Y}, (sfVector2f){560.f, 150.f},
        avec_alpha(C_ROUGE, 0.12f * progression(t, 0.f, 0.8f)));
    app->contour = 2.f;
    for (int i = 0; i < 10; i++) {
        d = 0.05f + i * 0.045f;
        e = sortie_rebond(progression(t, d, 0.6f));
        y = TITRE_Y + 46.f * (1.f - e) + sinf(app->temps * 2.2f - i * 0.5f) * 3.f;
        lettre[0] = mot[i];
        app->alpha = a * progression(t, d, 0.25f);
        if (i < 3)
            lueur(app, (sfVector2f){x + avances[i] / 2.f, y}, (sfVector2f){64.f, 64.f},
                avec_alpha(C_ROUGE, 0.26f));
        texte_echelle(app, POLICE_GRAS, lettre, TAILLE_TITRE, (sfVector2f){x
            + avances[i] / 2.f, y}, i < 3 ? C_ROUGE : C_TEXTE, ALIGNE_CENTRE,
            0.7f + 0.3f * e);
        x += avances[i];
    }
    app->contour = 0.f;
    app->alpha = a;
    menu_fil_rouge(app, TITRE_Y + 60.f, 300.f, sortie_cubique(progression(t, 0.45f, 0.9f)));
    app->alpha = a * progression(t, 0.7f, 0.4f);
    texte(app, POLICE_NORMALE, "Crée tes personnages, puis fais-les s'affronter.", 18,
        (sfVector2f){LARGEUR / 2.f, TITRE_Y + 100.f + 10.f * (1.f - app->alpha)},
        C_TEXTE_DOUX, ALIGNE_CENTRE);
    app->alpha = a;
}

/** Rappel des touches en bas de l'écran. */
void menu_aide_menu(app_t *app, float t)
{
    static const char *touches[5] = {"↑", "↓", "Entrée", "Échap", "F2"};
    static const char *actions[5] = {"", "choisir", "valider", "quitter", "audio"};
    float a = app->alpha;
    float total = 0.f;
    float x = 0.f;
    float w = 0.f;

    for (int i = 0; i < 5; i++)
        total += largeur_touche(app, touches[i]) + 8.f + (actions[i][0]
            ? largeur_texte(app, POLICE_NORMALE, actions[i], 13) + 24.f : 0.f);
    x = LARGEUR / 2.f - total / 2.f;
    app->alpha = a * progression(t, 1.f, 0.4f);
    for (int i = 0; i < 5; i++) {
        w = largeur_touche(app, touches[i]);
        touche_clavier(app, touches[i], (sfVector2f){x + w / 2.f, 628.f}, C_CYAN, 0.f);
        x += w + 8.f;
        if (actions[i][0])
            x += texte(app, POLICE_NORMALE, actions[i], 13, (sfVector2f){x, 628.f},
                C_TEXTE_FAIBLE, ALIGNE_GAUCHE) + 24.f;
    }
    app->alpha = a;
}

/** Crédit animé : la direction visuelle a été réalisée avec Claude. */
void menu_credit_claude(app_t *app, float t)
{
    static const char *phrase = "Direction visuelle et animations réalisées avec Claude";
    float a = app->alpha;
    float apparition = progression(t, 1.4f, 0.6f);
    float frappe = progression(t, 1.9f, 1.6f);
    float balayage = fmodf(t - 3.8f, 6.f) / 1.4f;
    char buf[96];
    int n = 0;
    int cible = (int)(frappe * (float)my_strlen(phrase));
    float largeur = largeur_texte(app, POLICE_NORMALE, phrase, 13);
    float x = LARGEUR / 2.f - (largeur + 44.f) / 2.f;
    sfVector2f c = {x + 11.f, 684.f};

    while (phrase[n] != '\0' && (n < cible || ((unsigned char)phrase[n] & 0xC0) == 0x80))
        n++;
    for (int i = 0; i < n; i++)
        buf[i] = phrase[i];
    buf[n] = '\0';
    app->alpha = a * apparition;
    lueur(app, c, (sfVector2f){26.f, 26.f}, avec_alpha(C_ROUGE, 0.22f
        + 0.08f * sinf(t * 2.4f)));
    hexagone(app, c, 9.f, t * 0.8f, avec_alpha(C_FOND, 0.9f), C_ROUGE);
    hexagone(app, c, 3.6f, -t * 0.8f, C_ROUGE, C_ROUGE);
    arc_cercle(app, c, 14.f, 1.5f, (sfVector2f){t * 2.f, t * 2.f + 1.6f},
        avec_alpha(C_CYAN, 0.8f), 1);
    texte(app, POLICE_NORMALE, buf, 13, (sfVector2f){x + 28.f, 684.f},
        C_TEXTE_DOUX, ALIGNE_GAUCHE);
    if (balayage > 0.f && balayage < 1.f && n >= cible)
        lueur(app, (sfVector2f){x + 28.f + largeur * balayage, 684.f},
            (sfVector2f){34.f, 14.f}, avec_alpha(C_TEXTE, 0.16f));
    app->alpha = a;
}
