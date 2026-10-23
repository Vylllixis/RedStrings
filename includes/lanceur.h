/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Lanceur de tests : chaque test tourne dans un processus fils, et chaque
 * vérification (V_CMP, V_VRAI…) note ce qui a été obtenu et ce qui était
 * attendu. Le rapport dit, pour chaque test, où il est écrit, combien de
 * vérifications il fait et, en cas d'échec, laquelle a échoué, avec les
 * valeurs, la ligne de code et la cause probable d'un plantage.
 */

#ifndef LANCEUR_H
    #define LANCEUR_H

    #include <stddef.h>

    /* Lance une fonction de test, l'affiche sous son propre nom. */
    #define LANCER(f) lanceur_test(#f, f, __FILE__, __LINE__)

/** Exécute test dans un processus fils (un plantage n'arrête donc pas les
 *  autres tests), affiche son résultat détaillé et le compte. `fichier` et
 *  `ligne` désignent l'appel de LANCER ; la définition du test est retrouvée
 *  dans le fichier. */
void lanceur_test(const char *nom, void (*test)(void), const char *fichier, int ligne);

/** Affiche le bilan : barre, totaux, vérifications, temps, liste des échecs.
 *  Renvoie 0 si tout passe, 1 sinon (à renvoyer par main). */
int lanceur_bilan(void);

/* ---------------------- valeurs et vérifications ------------------- */

typedef enum { VAL_ENTIER, VAL_NATUREL, VAL_REEL, VAL_TEXTE, VAL_ADRESSE } genre_val_t;

/** Une valeur de n'importe quel type, gardée pour être affichée en cas d'échec. */
typedef struct {
    genre_val_t genre;
    long long entier;
    unsigned long long naturel;
    double reel;
    const void *adresse;        /**< texte ou pointeur */
} val_t;

val_t val_entier(long long v);
val_t val_naturel(unsigned long long v);
val_t val_reel(double v);
val_t val_texte(const char *v);
val_t val_adresse(const void *v);

    /* Convertit une expression de type quelconque en val_t. */
    #define VAL(x) _Generic((x), \
        char: val_entier, signed char: val_entier, short: val_entier, int: val_entier, \
        long: val_entier, long long: val_entier, _Bool: val_naturel, \
        unsigned char: val_naturel, unsigned short: val_naturel, unsigned int: val_naturel, \
        unsigned long: val_naturel, unsigned long long: val_naturel, \
        float: val_reel, double: val_reel, \
        char *: val_texte, const char *: val_texte, default: val_adresse)(x)

    /* Où se trouve la vérification en cours. */
    #define POS __FILE__, __LINE__, __func__

    /** a op b (==, !=, <, <=, >, >=) ; affiche les deux valeurs si faux. */
    #define V_CMP(a, op, b) lanceur_cmp(POS, #a " " #op " " #b, VAL(a), #op, VAL(b), 0)
    /** Comme V_CMP, mais le test s'arrête en cas d'échec (le suite en dépend). */
    #define V_CMP_BLOQUANT(a, op, b) lanceur_cmp(POS, #a " " #op " " #b, VAL(a), #op, VAL(b), 1)
    /** L'expression doit valoir autre chose que 0 / NULL. */
    #define V_VRAI(e) lanceur_valeur(POS, #e, VAL(e), 1)
    /** L'expression doit valoir 0 / NULL. */
    #define V_FAUX(e) lanceur_valeur(POS, #e, VAL(e), 0)
    /** Expression booléenne composée (|| ou ?:) : seule sa valeur est connue. */
    #define V_BOOL(e) lanceur_bool(POS, #e, (e) ? 1 : 0)
    /** Deux textes égaux (a) ou différents (b) ; affiche les deux si faux. */
    #define V_TEXTE_EGAL(a, b) lanceur_textes(POS, #a " = " #b, val_texte(a), val_texte(b), 1)
    #define V_TEXTE_DIFF(a, b) lanceur_textes(POS, #a " ≠ " #b, val_texte(a), val_texte(b), 0)
    /** Deux nombres à virgule égaux à 0,0005 près. */
    #define V_PROCHE(a, b) lanceur_proche(POS, #a " ≈ " #b, VAL(a), VAL(b))

void lanceur_cmp(const char *fichier, int ligne, const char *fonction, const char *expr,
    val_t a, const char *op, val_t b, int bloquant);
void lanceur_valeur(const char *fichier, int ligne, const char *fonction, const char *expr,
    val_t v, int attendu_vrai);
void lanceur_bool(const char *fichier, int ligne, const char *fonction, const char *expr, int vrai);
void lanceur_textes(const char *fichier, int ligne, const char *fonction, const char *expr,
    val_t a, val_t b, int egaux);
void lanceur_proche(const char *fichier, int ligne, const char *fonction, const char *expr,
    val_t a, val_t b);

#endif /* LANCEUR_H */
