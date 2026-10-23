/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ utf8.c ─────────────────────────────────────────────────────────────
** │ But         Texte UTF-8 : décodage et encodage stricts, sans dépendre
** │             de SFML.
** │ Contenu     point_valide, longueur_sequence, decoder_un,
** │             utf8_encoder, utf8_decoder, utf8_nb_caracteres,
** │             utf8_supprimer_dernier, utf8_majuscules
** │ Testé par   csfml/tests/tests.c
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "gui_utf8.h"

#define POINT_MAX 0x10FFFF
#define SUBSTITUT_DEBUT 0xD800
#define SUBSTITUT_FIN 0xDFFF

/* Plus petit point de code qui justifie une séquence de cette longueur. */
static const uint32_t MIN_PAR_LONGUEUR[5] = {0, 0, 0x80, 0x800, 0x10000};

static int point_valide(uint32_t p)
{
    return p <= POINT_MAX && (p < SUBSTITUT_DEBUT || p > SUBSTITUT_FIN);
}

/** Longueur annoncée par l'octet de tête, 0 s'il ne peut pas en être un. */
static int longueur_sequence(unsigned char c)
{
    if (c < 0x80)
        return 1;
    if ((c & 0xE0) == 0xC0)
        return 2;
    if ((c & 0xF0) == 0xE0)
        return 3;
    if ((c & 0xF8) == 0xF0)
        return 4;
    return 0;
}

/*
** ╭─ decoder_un ─────────────────────────────────────────────────────────
** │ Décode UN caractère UTF-8 au début de `u`.
** │
** │ 1. longueur_sequence() lit l'octet de tête : le caractère fait 1 à 4
** │    octets
** │ 2. chaque octet de suite doit être de la forme 10xxxxxx (6 bits
** │    utiles chacun)
** │ 3. refuse les encodages trop longs (MIN_PAR_LONGUEUR) et les points
** │    de code invalides
** │ Renvoie la longueur lue, ou 0 si la séquence est invalide.
** ╰──────────────────────────────────────────────────────────────────────
*/
static int decoder_un(const unsigned char *u, uint32_t *point)
{
    int len = longueur_sequence(u[0]);
    uint32_t p = 0;

    if (len == 0)
        return 0;
    p = len == 1 ? u[0] : (uint32_t)(u[0] & (0xFF >> (len + 1)));
    for (int i = 1; i < len; i++) {
        if ((u[i] & 0xC0) != 0x80)
            return 0;
        p = (p << 6) | (uint32_t)(u[i] & 0x3F);
    }
    if (p < MIN_PAR_LONGUEUR[len] || !point_valide(p))
        return 0;
    *point = p;
    return len;
}

int utf8_encoder(uint32_t point, char sortie[4])
{
    if (sortie == NULL || !point_valide(point))
        return 0;
    if (point < 0x80) {
        sortie[0] = (char)point;
        return 1;
    }
    if (point < 0x800) {
        sortie[0] = (char)(0xC0 | (point >> 6));
        sortie[1] = (char)(0x80 | (point & 0x3F));
        return 2;
    }
    if (point < 0x10000) {
        sortie[0] = (char)(0xE0 | (point >> 12));
        sortie[1] = (char)(0x80 | ((point >> 6) & 0x3F));
        sortie[2] = (char)(0x80 | (point & 0x3F));
        return 3;
    }
    sortie[0] = (char)(0xF0 | (point >> 18));
    sortie[1] = (char)(0x80 | ((point >> 12) & 0x3F));
    sortie[2] = (char)(0x80 | ((point >> 6) & 0x3F));
    sortie[3] = (char)(0x80 | (point & 0x3F));
    return 4;
}

/*
** ╭─ utf8_decoder ───────────────────────────────────────────────────────
** │ Décode toute la chaîne en points de code.
** │
** │ Un octet invalide est sauté au lieu d'arrêter le décodage. Il n'écrit
** │ jamais plus de max - 1 points de code, plus le 0 final : pas de
** │ débordement. Renvoie le nombre de points de code écrits.
** ╰──────────────────────────────────────────────────────────────────────
*/
int utf8_decoder(const char *s, uint32_t *dst, int max)
{
    if (s == NULL || dst == NULL || max <= 0)
        return 0;

    const unsigned char *u = (const unsigned char *)s;
    uint32_t p = 0;
    int len = 0;
    int n = 0;

    while (*u && n < max - 1) {
        len = decoder_un(u, &p);
        if (len == 0) {
            u++;
            continue;
        }
        dst[n++] = p;
        u += len;
    }
    dst[n] = 0;
    return n;
}

int utf8_nb_caracteres(const char *s)
{
    int n = 0;

    if (s == NULL)
        return 0;
    for (int i = 0; s[i] != '\0'; i++)
        n += ((unsigned char)s[i] & 0xC0) != 0x80;
    return n;
}

void utf8_supprimer_dernier(char *s)
{
    int len = 0;

    if (s == NULL)
        return;
    while (s[len] != '\0')
        len++;
    if (len == 0)
        return;
    len--;
    while (len > 0 && ((unsigned char)s[len] & 0xC0) == 0x80)
        len--;
    s[len] = '\0';
}

void utf8_majuscules(char *s)
{
    if (s == NULL)
        return;

    unsigned char *u = (unsigned char *)s;

    for (int i = 0; u[i] != '\0'; i++) {
        if (u[i] >= 'a' && u[i] <= 'z')
            u[i] -= 32;
        else if (u[i] == 0xC3 && u[i + 1] >= 0xA0 && u[i + 1] <= 0xBE
            && u[i + 1] != 0xB7)
            u[++i] -= 0x20;
    }
}
