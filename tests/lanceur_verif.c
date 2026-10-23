/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ lanceur_verif.c ────────────────────────────────────────────────────
** │ But         Les vérifications des tests : comparer, noter la valeur
** │             obtenue et la valeur attendue, et garder la trace du
** │             premier échec (fichier, ligne, expression).
** │ Contenu     val_*, lanceur_cmp, lanceur_valeur, lanceur_bool,
** │             lanceur_textes, lanceur_proche
** ╰──────────────────────────────────────────────────────────────────────
*/

#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "lanceur_interne.h"

#define TOLERANCE 0.0005L
#define TEXTE_MAX_AFFICHE 70
#define TEXTE_VAL 90          /* une valeur affichée : assez court pour tenir deux fois dans TEXTE_LONG */

rapport_t *g_rapport;

val_t val_entier(long long v)
{
    return (val_t){VAL_ENTIER, v, 0, 0.0, NULL};
}

val_t val_naturel(unsigned long long v)
{
    return (val_t){VAL_NATUREL, 0, v, 0.0, NULL};
}

val_t val_reel(double v)
{
    return (val_t){VAL_REEL, 0, 0, v, NULL};
}

val_t val_texte(const char *v)
{
    return (val_t){VAL_TEXTE, 0, 0, 0.0, v};
}

val_t val_adresse(const void *v)
{
    return (val_t){VAL_ADRESSE, 0, 0, 0.0, v};
}

/* Texte d'une valeur, pour le rapport : les chaînes sont entre guillemets, NULL est écrit. */
static void texte_valeur(val_t v, char *dst, size_t taille)
{
    const char *s = v.adresse;
    size_t n = 0;

    if (v.genre == VAL_ENTIER)
        snprintf(dst, taille, "%lld", v.entier);
    else if (v.genre == VAL_NATUREL)
        snprintf(dst, taille, "%llu", v.naturel);
    else if (v.genre == VAL_REEL)
        snprintf(dst, taille, "%.6g", v.reel);
    else if (v.adresse == NULL)
        snprintf(dst, taille, "NULL");
    else if (v.genre == VAL_ADRESSE)
        snprintf(dst, taille, "%p (pointeur non nul)", v.adresse);
    else {
        snprintf(dst, taille, "\"");
        n = 1;
        for (size_t i = 0; s[i] != '\0' && i < TEXTE_MAX_AFFICHE && n + 6 < taille; i++)
            n += (size_t)snprintf(dst + n, taille - n, s[i] == '\n' ? "\\n" : "%c", s[i]);
        snprintf(dst + n, taille - n, "%s\"", strlen(s) > TEXTE_MAX_AFFICHE ? "…" : "");
    }
}

/* Valeur numérique d'une valeur ; une adresse devient son nombre. */
static long double nombre(val_t v)
{
    if (v.genre == VAL_ENTIER)
        return (long double)v.entier;
    if (v.genre == VAL_NATUREL)
        return (long double)v.naturel;
    if (v.genre == VAL_REEL)
        return (long double)v.reel;
    return (long double)(uintptr_t)v.adresse;
}

static int relation(long double x, const char *op, long double y)
{
    if (strcmp(op, "==") == 0)
        return !(x < y) && !(x > y);
    if (strcmp(op, "!=") == 0)
        return (x < y) || (x > y);
    if (strcmp(op, "<") == 0)
        return x < y;
    if (strcmp(op, "<=") == 0)
        return !(x > y);
    if (strcmp(op, ">") == 0)
        return x > y;
    return !(x < y);
}

static const char *mots_relation(const char *op)
{
    static const char *const ops[] = {"==", "!=", "<", "<=", ">", ">="};
    static const char *const mots[] = {"égal à", "différent de", "strictement inférieur à",
        "inférieur ou égal à", "strictement supérieur à", "supérieur ou égal à"};

    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        if (strcmp(op, ops[i]) == 0)
            return mots[i];
    }
    return op;
}

static void copier(char *dst, size_t taille, const char *src)
{
    snprintf(dst, taille, "%s", src);
}

/* Compte la vérification et retient où elle est : utile si le test plante juste après. */
static void noter_passage(const char *fichier, int ligne, const char *fonction)
{
    if (g_rapport == NULL)
        return;
    g_rapport->nb_verif++;
    copier(g_rapport->dernier_fichier, TEXTE_COURT, fichier);
    g_rapport->derniere_ligne = ligne;
    copier(g_rapport->derniere_fonction, sizeof(g_rapport->derniere_fonction), fonction);
}

/* Retient un échec (les ECHECS_MAX premiers en détail) ; un échec bloquant arrête le test. */
static void echec(const char *fichier, int ligne, const char *fonction, const char *expr,
    const char *obtenu, const char *attendu, int bloquant)
{
    echec_verif_t *e = NULL;

    if (g_rapport == NULL) {
        fprintf(stderr, "%s:%d: %s : vérification hors d'un test\n", fichier, ligne, expr);
        return;
    }
    if (g_rapport->nb_echecs < ECHECS_MAX) {
        e = &g_rapport->echecs[g_rapport->nb_echecs];
        copier(e->fichier, TEXTE_COURT, fichier);
        e->ligne = ligne;
        copier(e->fonction, sizeof(e->fonction), fonction);
        copier(e->expr, TEXTE_LONG, expr);
        copier(e->obtenu, TEXTE_LONG, obtenu);
        copier(e->attendu, TEXTE_LONG, attendu);
    }
    g_rapport->nb_echecs++;
    if (bloquant) {
        g_rapport->bloque = 1;
        _exit(1);
    }
}

void lanceur_cmp(const char *fichier, int ligne, const char *fonction, const char *expr,
    val_t a, const char *op, val_t b, int bloquant)
{
    char obtenu[TEXTE_VAL];
    char valeur_b[TEXTE_VAL];
    char attendu[TEXTE_LONG];

    noter_passage(fichier, ligne, fonction);
    if (relation(nombre(a), op, nombre(b)))
        return;
    texte_valeur(a, obtenu, sizeof(obtenu));
    texte_valeur(b, valeur_b, sizeof(valeur_b));
    snprintf(attendu, sizeof(attendu), "%s %s", mots_relation(op), valeur_b);
    echec(fichier, ligne, fonction, expr, obtenu, attendu, bloquant);
}

void lanceur_valeur(const char *fichier, int ligne, const char *fonction, const char *expr,
    val_t v, int attendu_vrai)
{
    char obtenu[TEXTE_LONG];

    noter_passage(fichier, ligne, fonction);
    if ((nombre(v) < 0.L || nombre(v) > 0.L) == (attendu_vrai != 0))
        return;
    texte_valeur(v, obtenu, sizeof(obtenu));
    echec(fichier, ligne, fonction, expr, obtenu,
        attendu_vrai ? "une valeur non nulle (vrai)" : "0 / NULL (faux)", 0);
}

void lanceur_bool(const char *fichier, int ligne, const char *fonction, const char *expr, int vrai)
{
    noter_passage(fichier, ligne, fonction);
    if (vrai)
        return;
    echec(fichier, ligne, fonction, expr, "faux", "vrai", 0);
}

void lanceur_textes(const char *fichier, int ligne, const char *fonction, const char *expr,
    val_t a, val_t b, int egaux)
{
    const char *x = a.adresse;
    const char *y = b.adresse;
    int identiques = x == y || (x != NULL && y != NULL && strcmp(x, y) == 0);
    char obtenu[TEXTE_VAL];
    char valeur_b[TEXTE_VAL];
    char attendu[TEXTE_LONG];

    noter_passage(fichier, ligne, fonction);
    if (identiques == (egaux != 0))
        return;
    texte_valeur(a, obtenu, sizeof(obtenu));
    texte_valeur(b, valeur_b, sizeof(valeur_b));
    snprintf(attendu, sizeof(attendu), "%s %s", egaux ? "identique à" : "différent de", valeur_b);
    echec(fichier, ligne, fonction, expr, obtenu, attendu, 0);
}

void lanceur_proche(const char *fichier, int ligne, const char *fonction, const char *expr,
    val_t a, val_t b)
{
    long double ecart = nombre(a) - nombre(b);
    char obtenu[TEXTE_VAL];
    char valeur_b[TEXTE_VAL];
    char attendu[TEXTE_LONG];

    noter_passage(fichier, ligne, fonction);
    if (ecart < TOLERANCE && ecart > -TOLERANCE)
        return;
    texte_valeur(a, obtenu, sizeof(obtenu));
    texte_valeur(b, valeur_b, sizeof(valeur_b));
    snprintf(attendu, sizeof(attendu), "proche de %s (écart %.6g)", valeur_b, (double)ecart);
    echec(fichier, ligne, fonction, expr, obtenu, attendu, 0);
}
