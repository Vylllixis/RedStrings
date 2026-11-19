#include "engine/ai_archetype.h"

#include <SDL3/SDL_stdinc.h> /* SDL_strtok_r : strtok_r est POSIX, absent de MSVC */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(char *err, size_t cap, uint32_t line, const char *msg)
{
    if (err != NULL && cap > 0U) {
        (void)snprintf(err, cap, "ligne %u : %s", line, msg);
    }
}

static bool to_float(const char *s, float *out)
{
    char *end = NULL;
    const float v = strtof(s, &end);
    if (end == s || *end != '\0' || !isfinite(v)) {
        return false;
    }
    *out = v;
    return true;
}

static bool to_u32(const char *s, uint32_t *out)
{
    char *end = NULL;
    const unsigned long v = strtoul(s, &end, 10);
    if (end == s || *end != '\0' || s[0] == '-' || v > 1000000UL) {
        return false;
    }
    *out = (uint32_t)v;
    return true;
}

static bool copy_name(char *dst, const char *src)
{
    const size_t n = strlen(src);
    if (n == 0U || n >= ENG_ARCH_NAME) {
        return false;
    }
    memcpy(dst, src, n + 1U);
    return true;
}

static bool apply(EngArchetype *a, const char *key, char *tok[], int nt, char *err, size_t cap,
                  uint32_t line)
{
    float f = 0.0F;
    uint32_t u = 0;
#define NEED(n)                                                                                    \
    if (nt != (n) + 1) {                                                                           \
        fail(err, cap, line, "nombre de valeurs incorrect pour la clé");                           \
        return false;                                                                              \
    }
    if (strcmp(key, "hp") == 0) {
        NEED(1)
        if (!to_float(tok[1], &f) || f <= 0.0F) {
            fail(err, cap, line, "`hp` attend un nombre > 0");
            return false;
        }
        a->hp = f;
    } else if (strcmp(key, "speed") == 0 || strcmp(key, "radius") == 0 ||
               strcmp(key, "sight") == 0 || strcmp(key, "hearing") == 0 ||
               strcmp(key, "standoff") == 0) {
        NEED(1)
        if (!to_float(tok[1], &f) || f <= 0.0F) {
            fail(err, cap, line, "valeur > 0 attendue");
            return false;
        }
        float *dst = key[0] == 's' && key[1] == 'p'   ? &a->speed
                     : key[0] == 'r'                  ? &a->radius
                     : key[0] == 's' && key[1] == 'i' ? &a->sight
                     : key[0] == 'h'                  ? &a->hearing
                                                      : &a->standoff;
        *dst = f;
    } else if (strcmp(key, "fov") == 0) {
        NEED(1)
        if (!to_float(tok[1], &f) || f <= 0.0F || f > 360.0F) {
            fail(err, cap, line, "`fov` attend des degrés dans ]0 ; 360]");
            return false;
        }
        a->fov_deg = f;
    } else if (strcmp(key, "flee_below") == 0) {
        NEED(1)
        if (!to_float(tok[1], &f) || f < 0.0F || f >= 1.0F) {
            fail(err, cap, line, "`flee_below` attend une fraction dans [0 ; 1[");
            return false;
        }
        a->flee_below = f;
    } else if (strcmp(key, "memory") == 0 || strcmp(key, "alert") == 0) {
        NEED(1)
        if (!to_u32(tok[1], &u)) {
            fail(err, cap, line, "entier de ticks attendu");
            return false;
        }
        if (key[0] == 'm') {
            a->memory = u;
        } else {
            a->alert = u;
        }
    } else if (strcmp(key, "light") == 0 || strcmp(key, "heavy") == 0) {
        NEED(1)
        if (!copy_name(key[0] == 'l' ? a->light : a->heavy, tok[1])) {
            fail(err, cap, line, "nom d'attaque invalide");
            return false;
        }
    } else if (strcmp(key, "plan") == 0) {
        NEED(6)
        if (a->plan_count >= ENG_ARCH_PLANS) {
            fail(err, cap, line, "trop de `plan`");
            return false;
        }
        EngArchPlan *p = &a->plans[a->plan_count];
        if ((strcmp(tok[1], "light") != 0 && strcmp(tok[1], "heavy") != 0) ||
            !to_float(tok[2], &p->range_min) || !to_float(tok[3], &p->range_max) ||
            !to_u32(tok[4], &p->cooldown) || !to_u32(tok[5], &p->weight) ||
            !to_u32(tok[6], &p->presses) || p->range_min < 0.0F || p->range_max <= p->range_min ||
            p->weight == 0U || p->presses == 0U || p->presses > 3U) {
            fail(err, cap, line,
                 "`plan` : genre portée_min portée_max recharge poids appuis (1 à 3)");
            return false;
        }
        p->heavy = tok[1][0] == 'h';
        a->plan_count++;
    } else if (strcmp(key, "phase") == 0) {
        NEED(5)
        if (a->phase_count >= ENG_ARCH_PHASES) {
            fail(err, cap, line, "trop de `phase`");
            return false;
        }
        EngArchPhase *p = &a->phases[a->phase_count];
        if (!to_float(tok[1], &p->hp_below) || p->hp_below <= 0.0F || p->hp_below >= 1.0F ||
            !copy_name(p->light, tok[2]) || !copy_name(p->heavy, tok[3]) ||
            !to_float(tok[4], &p->speed) || p->speed <= 0.0F ||
            !to_float(tok[5], &p->cooldown_scale) || p->cooldown_scale <= 0.0F) {
            fail(err, cap, line,
                 "`phase` : fraction_PV attaque_légère attaque_lourde vitesse recharge");
            return false;
        }
        if (a->phase_count > 0U && p->hp_below >= a->phases[a->phase_count - 1U].hp_below) {
            fail(err, cap, line, "les `phase` doivent descendre en PV");
            return false;
        }
        a->phase_count++;
    } else {
        fail(err, cap, line, "clé inconnue");
        return false;
    }
#undef NEED
    return true;
}

bool eng_arch_parse(const char *text, EngArchSet *out, char *err, size_t err_capacity)
{
    if (text == NULL || out == NULL) {
        fail(err, err_capacity, 0, "entrée invalide");
        return false;
    }
    memset(out, 0, sizeof *out);
    EngArchetype *cur = NULL;
    uint32_t line_no = 0;
    const char *p = text;
    while (*p != '\0') {
        char line[256];
        size_t n = 0;
        while (*p != '\0' && *p != '\n') {
            if (n + 1U < sizeof line) {
                line[n++] = *p;
            } else {
                fail(err, err_capacity, line_no + 1U, "ligne trop longue");
                return false;
            }
            p++;
        }
        if (*p == '\n') {
            p++;
        }
        line[n] = '\0';
        line_no++;
        char *hash = strchr(line, '#');
        if (hash != NULL) {
            *hash = '\0';
        }
        char *tok[10];
        int nt = 0;
        char *save = NULL;
        for (char *t = SDL_strtok_r(line, " \t\r", &save); t != NULL;
             t = SDL_strtok_r(NULL, " \t\r", &save)) {
            if (nt >= 10) {
                fail(err, err_capacity, line_no, "trop de valeurs");
                return false;
            }
            tok[nt++] = t;
        }
        if (nt == 0) {
            continue;
        }
        if (strcmp(tok[0], "archetype") == 0) {
            if (cur != NULL || nt != 2 || out->count >= ENG_ARCH_MAX ||
                strlen(tok[1]) >= ENG_ARCH_NAME) {
                fail(err, err_capacity, line_no,
                     "`archetype NOM` invalide (ou `end` manquant, ou trop d'archétypes)");
                return false;
            }
            if (eng_arch_find(out, tok[1]) != UINT32_MAX) {
                fail(err, err_capacity, line_no, "nom d'archétype déjà utilisé");
                return false;
            }
            cur = &out->a[out->count++];
            memset(cur, 0, sizeof *cur);
            memcpy(cur->name, tok[1], strlen(tok[1]) + 1U);
            cur->hp = 60.0F;
            cur->speed = 1.0F;
            cur->radius = 0.4F;
            cur->sight = 12.0F;
            cur->fov_deg = 110.0F;
            cur->hearing = 8.0F;
            cur->memory = 300;
            cur->alert = 30;
            cur->standoff = 4.0F;
        } else if (strcmp(tok[0], "end") == 0) {
            if (cur == NULL) {
                fail(err, err_capacity, line_no, "`end` sans `archetype`");
                return false;
            }
            if (cur->light[0] == '\0' || cur->heavy[0] == '\0' || cur->plan_count == 0U) {
                fail(err, err_capacity, line_no, "il faut `light`, `heavy` et au moins un `plan`");
                return false;
            }
            cur = NULL;
        } else {
            if (cur == NULL) {
                fail(err, err_capacity, line_no, "clé hors d'un bloc `archetype`");
                return false;
            }
            if (!apply(cur, tok[0], tok, nt, err, err_capacity, line_no)) {
                return false;
            }
        }
    }
    if (cur != NULL) {
        fail(err, err_capacity, line_no, "`end` manquant");
        return false;
    }
    if (out->count == 0U) {
        fail(err, err_capacity, line_no, "aucun archétype");
        return false;
    }
    return true;
}

uint32_t eng_arch_find(const EngArchSet *set, const char *name)
{
    if (set == NULL || name == NULL) {
        return UINT32_MAX;
    }
    for (uint32_t i = 0; i < set->count; i++) {
        if (strcmp(set->a[i].name, name) == 0) {
            return i;
        }
    }
    return UINT32_MAX;
}

static bool check_chain(const EngAttackSet *atk, const char *arch, const char *first, bool chain,
                        char *err, size_t cap)
{
    uint32_t idx = eng_attackset_find(atk, first);
    if (idx == UINT32_MAX) {
        if (err != NULL && cap > 0U) {
            (void)snprintf(err, cap, "archétype %s : attaque `%s` inconnue", arch, first);
        }
        return false;
    }
    for (uint32_t guard = 0; guard < ENG_ATTACK_MAX && idx != UINT32_MAX && idx < atk->count;
         guard++) {
        const EngAttack *a = &atk->attacks[idx];
        if (a->startup < ENG_ARCH_MIN_TELEGRAPH) {
            if (err != NULL && cap > 0U) {
                (void)snprintf(err, cap,
                               "archétype %s : `%s` a une anticipation de %u ticks (< %u = 300 ms)",
                               arch, a->name, a->startup, ENG_ARCH_MIN_TELEGRAPH);
            }
            return false;
        }
        if (!chain) {
            break;
        }
        idx = a->next == ENG_ATTACK_NONE ? UINT32_MAX : a->next;
    }
    return true;
}

bool eng_arch_validate(const EngArchSet *set, const EngAttackSet *attacks, char *err,
                       size_t err_capacity)
{
    if (set == NULL || attacks == NULL) {
        return false;
    }
    for (uint32_t i = 0; i < set->count; i++) {
        const EngArchetype *a = &set->a[i];
        if (!check_chain(attacks, a->name, a->light, true, err, err_capacity) ||
            !check_chain(attacks, a->name, a->heavy, false, err, err_capacity)) {
            return false;
        }
        for (uint32_t k = 0; k < a->phase_count; k++) {
            if (!check_chain(attacks, a->name, a->phases[k].light, true, err, err_capacity) ||
                !check_chain(attacks, a->name, a->phases[k].heavy, false, err, err_capacity)) {
                return false;
            }
        }
    }
    return true;
}
