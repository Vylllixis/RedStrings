#include "engine/anim_events.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/log.h"

#define MAX_LINE 256U
#define MAX_TOKENS 8U

static const char *const TYPE_NAMES[ENG_ANIMEVENT_TYPE_COUNT] = {
    "hitbox_on", "hitbox_off", "sound", "effect", "cancel_open", "cancel_close"};

const char *eng_animevent_type_name(EngAnimEventType t)
{
    return ((unsigned)t < (unsigned)ENG_ANIMEVENT_TYPE_COUNT) ? TYPE_NAMES[t] : "?";
}

typedef struct Parser {
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
    if (n == 0 || n >= ENG_ANIMEVENT_MAX_NAME) {
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

static void parse_line(Parser *p, EngAnimEventSet *out, char *const tok[], size_t n)
{
    if (strcmp(tok[0], "event") != 0) {
        fail(p, "mot inconnu « %s » (`event CLIP TEMPS TYPE [ARG]` attendu)", tok[0]);
        return;
    }
    if (n < 4 || n > 5) {
        fail(p, "`event CLIP TEMPS TYPE [ARG]` attendu");
        return;
    }
    if (out->count >= ENG_ANIMEVENT_MAX) {
        fail(p, "plus de %u événements", (unsigned)ENG_ANIMEVENT_MAX);
        return;
    }
    if (!valid_name(tok[1])) {
        fail(p, "nom de clip invalide « %s »", tok[1]);
        return;
    }
    char *end = NULL;
    errno = 0;
    const float time = strtof(tok[2], &end);
    if (errno != 0 || end == tok[2] || *end != '\0' || !isfinite(time) || time < 0.0F ||
        time > 3600.0F) {
        fail(p, "temps dans [0 ; 3600] s attendu, reçu « %s »", tok[2]);
        return;
    }
    int type = -1;
    for (int i = 0; i < ENG_ANIMEVENT_TYPE_COUNT; i++) {
        if (strcmp(tok[3], TYPE_NAMES[i]) == 0) {
            type = i;
        }
    }
    if (type < 0) {
        fail(p, "type inconnu « %s »", tok[3]);
        return;
    }
    const bool needs_arg = type == ENG_ANIMEVENT_SOUND || type == ENG_ANIMEVENT_EFFECT;
    if (needs_arg && n != 5) {
        fail(p, "`%s` demande un argument (nom)", tok[3]);
        return;
    }
    if (!needs_arg && n != 4) {
        fail(p, "`%s` n'accepte pas d'argument", tok[3]);
        return;
    }
    if (needs_arg && !valid_name(tok[4])) {
        fail(p, "argument invalide « %s »", tok[4]);
        return;
    }
    EngAnimEvent *e = &out->events[out->count++];
    snprintf(e->clip, sizeof e->clip, "%s", tok[1]);
    e->time = time;
    e->type = (EngAnimEventType)type;
    if (needs_arg) {
        snprintf(e->arg, sizeof e->arg, "%s", tok[4]);
    }
}

/* Tri stable par (clip, temps). */
static int cmp_event(const EngAnimEvent *a, const EngAnimEvent *b)
{
    const int c = strcmp(a->clip, b->clip);
    if (c != 0) {
        return c;
    }
    if (a->time < b->time) {
        return -1;
    }
    return (a->time > b->time) ? 1 : 0;
}

static void sort_events(EngAnimEventSet *s)
{
    for (uint32_t i = 1; i < s->count; i++) { /* insertion : stable, 128 éléments au plus */
        const EngAnimEvent v = s->events[i];
        uint32_t j = i;
        while (j > 0 && cmp_event(&s->events[j - 1], &v) > 0) {
            s->events[j] = s->events[j - 1];
            j--;
        }
        s->events[j] = v;
    }
}

bool eng_animevents_parse(const char *text, EngAnimEventSet *out, char *err, size_t err_capacity)
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
    Parser p = {.err = err, .err_capacity = err_capacity};
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
        parse_line(&p, out, tok, n);
    }
    if (p.failed) {
        memset(out, 0, sizeof *out);
        return false;
    }
    sort_events(out);
    return true;
}

uint32_t eng_animevents_collect(const EngAnimEventSet *set, const char *clip, float duration,
                                float t0, float t1, bool loop, uint32_t *out, uint32_t max)
{
    if (set == NULL || clip == NULL || !(duration > 0.0F) || !isfinite(t0) || !isfinite(t1) ||
        !isfinite(duration)) {
        return 0;
    }
    if (t0 < 0.0F) {
        t0 = 0.0F;
    }
    bool close_end = false; /* intervalle fermé à t1 : fin d'un clip non bouclé */
    if (!loop && t1 >= duration) {
        t1 = duration;
        close_end = true;
    }
    if (t1 <= t0) {
        return 0;
    }
    uint32_t total = 0;
    /* cycles traversés : [t0 ; t1[ touche les cycles floor(t0/d) .. floor(t1/d) */
    const double d = (double)duration;
    const double first = floor((double)t0 / d);
    const double last = loop ? floor((double)t1 / d) : 0.0; /* sans boucle : un seul passage */
    if (last - first > 1024.0) {
        return 0; /* pas absurde (plus de 1024 cycles d'un coup) : refusé plutôt que de boucler */
    }
    for (int64_t ki = (int64_t)first; ki <= (int64_t)last; ki++) {
        const double k = (double)ki;
        for (uint32_t i = 0; i < set->count; i++) {
            const EngAnimEvent *e = &set->events[i];
            if (strcmp(e->clip, clip) != 0) {
                continue;
            }
            if (e->time > duration) {
                continue; /* événement au-delà de ce clip : ignoré */
            }
            const double at = k * d + (double)e->time;
            const bool in = (at >= (double)t0) && (at < (double)t1 || (close_end && at == t1));
            if (!in) {
                continue;
            }
            if (total < max && out != NULL) {
                out[total] = i;
            }
            total++;
        }
    }
    return total;
}

/* ── Fichier surveillé ── */

static bool file_load(EngAnimEventFile *file, EngArena *scratch, EngAnimEventSet *set, char *err,
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
        ok = eng_animevents_parse((const char *)data.data, set, err, err_cap);
    }
    eng_arena_reset_to(scratch, mark);
    return ok;
}

bool eng_animeventfile_open(EngAnimEventFile *file, EngArena *scratch, const char *path)
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
        ENG_LOG_ERROR("événements %s : %s", path, file->error);
        return false;
    }
    file->valid = true;
    file->version = 1;
    return true;
}

bool eng_animeventfile_poll(EngAnimEventFile *file, EngArena *scratch, double now_seconds)
{
    if (file == NULL || scratch == NULL || !eng_file_watch_poll(&file->watch, now_seconds)) {
        return false;
    }
    EngAnimEventSet next;
    char err[sizeof file->error];
    if (!file_load(file, scratch, &next, err, sizeof err)) {
        memcpy(file->error, err, sizeof err);
        ENG_LOG_ERROR("événements %s : %s (l'ancienne version reste active)", file->path, err);
        return false;
    }
    file->set = next;
    file->valid = true;
    file->version++;
    file->error[0] = '\0';
    ENG_LOG_INFO("événements %s rechargés (version %u)", file->path, (unsigned)file->version);
    return true;
}
