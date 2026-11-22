#include "engine/attack_data.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/log.h"

#define MAX_LINE 512U
#define MAX_TOKENS 16U

typedef struct Parser {
    EngAttackSet *set;
    char *err;
    size_t err_capacity;
    uint32_t line;
    bool failed;
} Parser;

static void fail(Parser *p, const char *fmt, ...)
{
    if (p->failed) {
        return;
    }
    p->failed = true;
    if (p->err == NULL || p->err_capacity == 0) {
        return;
    }
    const int n = snprintf(p->err, p->err_capacity, "ligne %u : ", (unsigned)p->line);
    if (n < 0 || (size_t)n >= p->err_capacity) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    (void)vsnprintf(p->err + n, p->err_capacity - (size_t)n, fmt, args);
    va_end(args);
}

static bool valid_name(const char *s)
{
    const size_t n = strlen(s);
    if (n == 0 || n >= ENG_ATTACK_MAX_NAME) {
        return false;
    }
    for (size_t i = 0; i < n; i++) {
        const char c = s[i];
        if (!(isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.')) {
            return false;
        }
    }
    return true;
}

static bool parse_uint(const char *s, uint32_t max, uint32_t *out)
{
    if (s[0] == '\0' || s[0] == '-' || s[0] == '+') {
        return false;
    }
    char *end = NULL;
    errno = 0;
    const unsigned long v = strtoul(s, &end, 10);
    if (errno != 0 || *end != '\0' || v > max) {
        return false;
    }
    *out = (uint32_t)v;
    return true;
}

static bool parse_float(const char *s, float *out)
{
    char *end = NULL;
    errno = 0;
    const float v = strtof(s, &end);
    if (errno != 0 || end == s || *end != '\0' || !isfinite(v)) {
        return false;
    }
    *out = v;
    return true;
}

/* Valeurs lues pour l'attaque en cours, avec « a-t-on vu cette clé ». */
typedef struct Seen {
    bool startup, active, recovery, damage, posture, stamina, hitstop, cancel_from, next, anim;
} Seen;

static void unresolved_next(EngAttack *a)
{
    a->next = ENG_ATTACK_NONE;
}

/* Un `next NOM` est mémorisé par nom (passe 2) : on garde le nom dans un tableau parallèle. */
typedef struct NextName {
    char name[ENG_ATTACK_MAX_NAME];
    uint32_t line;
    bool used;
} NextName;

static size_t tokenize(char *line, char *tok[], size_t max)
{
    size_t n = 0;
    char *c = line;
    while (*c != '\0') {
        while (*c == ' ' || *c == '\t' || *c == '\r') {
            c++;
        }
        if (*c == '\0') {
            break;
        }
        if (n == max) {
            return max + 1;
        }
        tok[n++] = c;
        while (*c != '\0' && *c != ' ' && *c != '\t' && *c != '\r') {
            c++;
        }
        if (*c != '\0') {
            *c++ = '\0';
        }
    }
    return n;
}

static void do_key(Parser *p, EngAttack *a, Seen *seen, NextName *nn, char *const tok[], size_t n)
{
    const char *k = tok[0];
    uint32_t u = 0;
    float f = 0.0F;
#define ONE_ARG()                                                                                  \
    if (n != 2) {                                                                                  \
        fail(p, "`%s` attend une seule valeur", k);                                                \
        return;                                                                                    \
    }
#define DUP(flag)                                                                                  \
    if ((flag)) {                                                                                  \
        fail(p, "`%s` déclaré deux fois", k);                                                      \
        return;                                                                                    \
    }                                                                                              \
    (flag) = true;
    if (strcmp(k, "startup") == 0 || strcmp(k, "active") == 0 || strcmp(k, "recovery") == 0) {
        ONE_ARG()
        const uint32_t min = (strcmp(k, "recovery") == 0) ? 0U : 1U;
        if (!parse_uint(tok[1], ENG_ATTACK_MAX_TICKS, &u) || u < min) {
            fail(p, "`%s` : entier de %u à %u attendu, reçu « %s »", k, (unsigned)min,
                 (unsigned)ENG_ATTACK_MAX_TICKS, tok[1]);
            return;
        }
        if (k[0] == 's') {
            DUP(seen->startup)
            a->startup = u;
        } else if (k[0] == 'a') {
            DUP(seen->active)
            a->active = u;
        } else {
            DUP(seen->recovery)
            a->recovery = u;
        }
    } else if (strcmp(k, "damage") == 0 || strcmp(k, "posture") == 0 || strcmp(k, "stamina") == 0) {
        ONE_ARG()
        if (!parse_float(tok[1], &f) || f < 0.0F || f > 100000.0F) {
            fail(p, "`%s` : nombre >= 0 attendu, reçu « %s »", k, tok[1]);
            return;
        }
        if (k[0] == 'd') {
            DUP(seen->damage)
            a->damage = f;
        } else if (k[0] == 'p') {
            DUP(seen->posture)
            a->posture = f;
        } else {
            DUP(seen->stamina)
            a->stamina = f;
        }
    } else if (strcmp(k, "hitstop") == 0) {
        ONE_ARG()
        if (!parse_uint(tok[1], ENG_ATTACK_MAX_HITSTOP, &u)) {
            fail(p, "`hitstop` : entier de 0 à %u attendu, reçu « %s »",
                 (unsigned)ENG_ATTACK_MAX_HITSTOP, tok[1]);
            return;
        }
        DUP(seen->hitstop)
        a->hitstop = u;
    } else if (strcmp(k, "cancel_from") == 0) {
        ONE_ARG()
        if (!parse_uint(tok[1], ENG_ATTACK_MAX_TICKS, &u)) {
            fail(p, "`cancel_from` : entier >= 0 attendu, reçu « %s »", tok[1]);
            return;
        }
        DUP(seen->cancel_from)
        a->cancel_from = u;
    } else if (strcmp(k, "next") == 0) {
        ONE_ARG()
        if (!valid_name(tok[1])) {
            fail(p, "`next` : nom invalide « %s »", tok[1]);
            return;
        }
        DUP(seen->next)
        snprintf(nn->name, sizeof nn->name, "%s", tok[1]);
        nn->line = p->line;
        nn->used = true;
    } else if (strcmp(k, "anim") == 0) {
        if (n < 2 || n > 4) {
            fail(p, "`anim CLIP [VITESSE [DÉCALAGE]]` attendu");
            return;
        }
        if (!valid_name(tok[1])) {
            fail(p, "`anim` : nom de clip invalide « %s »", tok[1]);
            return;
        }
        float speed = 1.0F;
        float offset = 0.0F;
        if (n >= 3 && (!parse_float(tok[2], &speed) || !(speed > 0.0F) || speed > 10.0F)) {
            fail(p, "`anim` : vitesse dans ]0 ; 10] attendue, reçu « %s »", tok[2]);
            return;
        }
        if (n == 4 && (!parse_float(tok[3], &offset) || offset < -60.0F || offset > 60.0F)) {
            fail(p, "`anim` : décalage dans [-60 ; 60] s attendu, reçu « %s »", tok[3]);
            return;
        }
        DUP(seen->anim)
        snprintf(a->anim, sizeof a->anim, "%s", tok[1]);
        a->anim_speed = speed;
        a->anim_offset = offset;
    } else if (strcmp(k, "hitbox") == 0) {
        if (n != 9) {
            fail(p, "`hitbox OS ax ay az bx by bz RAYON` attendu (9 mots)");
            return;
        }
        if (a->hitbox_count >= ENG_ATTACK_MAX_HITBOXES) {
            fail(p, "plus de %u hitboxes pour « %s »", (unsigned)ENG_ATTACK_MAX_HITBOXES, a->name);
            return;
        }
        if (!valid_name(tok[1])) {
            fail(p, "`hitbox` : nom d'os invalide « %s »", tok[1]);
            return;
        }
        EngAttackHitbox *h = &a->hitboxes[a->hitbox_count];
        snprintf(h->joint, sizeof h->joint, "%s", tok[1]);
        for (size_t i = 0; i < 3; i++) {
            if (!parse_float(tok[2 + i], &h->a[i]) || !parse_float(tok[5 + i], &h->b[i])) {
                fail(p, "`hitbox` : nombre invalide dans les coordonnées");
                return;
            }
        }
        if (!parse_float(tok[8], &h->radius) || !(h->radius > 0.0F) || h->radius > 10.0F) {
            fail(p, "`hitbox` : rayon dans ]0 ; 10] m attendu, reçu « %s »", tok[8]);
            return;
        }
        a->hitbox_count++;
    } else {
        fail(p, "mot inconnu « %s »", k);
    }
#undef ONE_ARG
#undef DUP
}

bool eng_attackset_parse(const char *text, EngAttackSet *out, char *err, size_t err_capacity)
{
    if (out == NULL) {
        return false;
    }
    memset(out, 0, sizeof *out);
    if (err != NULL && err_capacity > 0) {
        err[0] = '\0';
    }
    if (text == NULL) {
        return false;
    }
    NextName nexts[ENG_ATTACK_MAX];
    memset(nexts, 0, sizeof nexts);
    Parser p = {.set = out, .err = err, .err_capacity = err_capacity};
    EngAttack *cur = NULL;
    Seen seen = {0};
    const char *c = text;
    while (*c != '\0' && !p.failed) {
        char line[MAX_LINE];
        size_t len = 0;
        while (c[len] != '\0' && c[len] != '\n') {
            len++;
        }
        p.line++;
        if (len >= MAX_LINE) {
            fail(&p, "ligne trop longue (%u caractères maximum)", MAX_LINE - 1U);
            break;
        }
        memcpy(line, c, len);
        line[len] = '\0';
        c += len + (c[len] == '\n' ? 1U : 0U);
        char *hash = strchr(line, '#');
        if (hash != NULL) {
            *hash = '\0';
        }
        char *tok[MAX_TOKENS];
        const size_t n = tokenize(line, tok, MAX_TOKENS);
        if (n == 0) {
            continue;
        }
        if (n > MAX_TOKENS) {
            fail(&p, "trop de mots sur la ligne");
            break;
        }
        if (strcmp(tok[0], "attack") == 0) {
            if (cur != NULL) {
                fail(&p, "`attack` dans une attaque non terminée (`end` manquant)");
                break;
            }
            if (n != 2 || !valid_name(tok[1])) {
                fail(&p, "`attack NOM` attendu (A-Z a-z 0-9 _ - ., %u caractères maximum)",
                     ENG_ATTACK_MAX_NAME - 1U);
                break;
            }
            if (out->count >= ENG_ATTACK_MAX) {
                fail(&p, "plus de %u attaques", (unsigned)ENG_ATTACK_MAX);
                break;
            }
            if (eng_attackset_find(out, tok[1]) != ENG_ATTACK_NONE) {
                fail(&p, "attaque « %s » déclarée deux fois", tok[1]);
                break;
            }
            cur = &out->attacks[out->count];
            snprintf(cur->name, sizeof cur->name, "%s", tok[1]);
            unresolved_next(cur);
            cur->cancel_from = UINT32_MAX; /* « non donné » : fixé à `end` */
            cur->anim_speed = 1.0F;
            seen = (Seen){0};
        } else if (strcmp(tok[0], "end") == 0) {
            if (cur == NULL || n != 1) {
                fail(&p, "`end` sans `attack`");
                break;
            }
            if (!seen.startup || !seen.active || !seen.recovery || !seen.damage) {
                fail(&p, "« %s » : `startup`, `active`, `recovery` et `damage` sont obligatoires",
                     cur->name);
                break;
            }
            if (cur->hitbox_count == 0) {
                fail(&p, "« %s » : au moins une `hitbox` est obligatoire", cur->name);
                break;
            }
            if (!seen.cancel_from) {
                cur->cancel_from = cur->recovery;
            } else if (cur->cancel_from > cur->recovery) {
                fail(&p, "« %s » : `cancel_from` (%u) dépasse `recovery` (%u)", cur->name,
                     (unsigned)cur->cancel_from, (unsigned)cur->recovery);
                break;
            }
            out->count++;
            cur = NULL;
        } else if (cur == NULL) {
            fail(&p, "« %s » hors d'une attaque (`attack NOM` attendu avant)", tok[0]);
            break;
        } else {
            do_key(&p, cur, &seen, &nexts[out->count], tok, n);
        }
    }
    if (!p.failed && cur != NULL) {
        p.line++;
        fail(&p, "« %s » : `end` manquant", cur->name);
    }
    if (!p.failed && out->count == 0) {
        p.line = 1;
        fail(&p, "aucune attaque définie");
    }
    for (uint32_t i = 0; !p.failed && i < out->count; i++) {
        if (nexts[i].used) {
            const uint32_t t = eng_attackset_find(out, nexts[i].name);
            if (t == ENG_ATTACK_NONE) {
                p.line = nexts[i].line;
                fail(&p, "`next` : attaque inconnue « %s »", nexts[i].name);
            } else {
                out->attacks[i].next = t;
            }
        }
    }
    if (p.failed) {
        memset(out, 0, sizeof *out);
        return false;
    }
    return true;
}

uint32_t eng_attackset_find(const EngAttackSet *set, const char *name)
{
    for (uint32_t i = 0; set != NULL && name != NULL && i < set->count; i++) {
        if (strcmp(set->attacks[i].name, name) == 0) {
            return i;
        }
    }
    return ENG_ATTACK_NONE;
}

uint32_t eng_attack_total(const EngAttack *a)
{
    return a->startup + a->active + a->recovery;
}

EngAttackPhase eng_attack_phase(const EngAttack *a, uint32_t tick)
{
    if (tick < a->startup) {
        return ENG_ATTACK_STARTUP;
    }
    if (tick < a->startup + a->active) {
        return ENG_ATTACK_ACTIVE;
    }
    if (tick < eng_attack_total(a)) {
        return ENG_ATTACK_RECOVERY;
    }
    return ENG_ATTACK_DONE;
}

bool eng_attack_is_active(const EngAttack *a, uint32_t tick)
{
    return eng_attack_phase(a, tick) == ENG_ATTACK_ACTIVE;
}

bool eng_attack_can_cancel(const EngAttack *a, uint32_t tick)
{
    return tick >= a->startup + a->active + a->cancel_from && tick < eng_attack_total(a);
}

/* ── Fichier surveillé ── */

static bool file_load(EngAttackFile *file, EngArena *scratch, EngAttackSet *set, char *err,
                      size_t err_cap)
{
    const EngArenaMark mark = eng_arena_mark(scratch);
    EngFileData data;
    bool ok = false;
    if (!eng_file_read_all(scratch, file->path, &data)) {
        (void)snprintf(err, err_cap, "fichier illisible : %.200s", file->path);
    } else if (memchr(data.data, '\0', data.size) != NULL) {
        (void)snprintf(err, err_cap, "fichier binaire (octet nul) : %.200s", file->path);
    } else {
        ok = eng_attackset_parse((const char *)data.data, set, err, err_cap);
    }
    eng_arena_reset_to(scratch, mark);
    return ok;
}

bool eng_attackfile_open(EngAttackFile *file, EngArena *scratch, const char *path)
{
    if (file == NULL) {
        return false;
    }
    memset(file, 0, sizeof *file);
    if (scratch == NULL || path == NULL || strlen(path) >= sizeof file->path) {
        return false;
    }
    snprintf(file->path, sizeof file->path, "%s", path);
    if (!eng_file_watch_init(&file->watch, file->path, ENG_FILE_WATCH_DEFAULT_INTERVAL)) {
        return false;
    }
    if (!file_load(file, scratch, &file->set, file->error, sizeof file->error)) {
        ENG_LOG_ERROR("attaques %s : %s", path, file->error);
        return false;
    }
    file->valid = true;
    file->version = 1;
    return true;
}

bool eng_attackfile_poll(EngAttackFile *file, EngArena *scratch, double now_seconds)
{
    if (file == NULL || scratch == NULL || !eng_file_watch_poll(&file->watch, now_seconds)) {
        return false;
    }
    EngAttackSet next;
    char err[sizeof file->error];
    if (!file_load(file, scratch, &next, err, sizeof err)) {
        memcpy(file->error, err, sizeof err);
        ENG_LOG_ERROR("attaques %s : %s (l'ancienne version reste active)", file->path, err);
        return false;
    }
    file->set = next;
    file->valid = true;
    file->version++;
    file->error[0] = '\0';
    ENG_LOG_INFO("attaques %s rechargées (version %u)", file->path, (unsigned)file->version);
    return true;
}
