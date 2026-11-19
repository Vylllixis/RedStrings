#include "engine/anim_graph.h"

#include <SDL3/SDL_stdinc.h> /* SDL_strtok_r : strtok_r est POSIX, absent de MSVC */
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/log.h"

#define MAX_LINE 512U
#define MAX_TOKENS 32U

typedef struct Parser {
    EngAnimGraphDef *def;
    char *err;
    size_t err_capacity;
    uint32_t line;
    bool failed;
    bool has_start;
} Parser;

static void fail(Parser *p, const char *fmt, ...)
{
    if (p->failed) {
        return; /* on garde la première erreur */
    }
    p->failed = true;
    if (p->err == NULL || p->err_capacity == 0) {
        return;
    }
    int n = snprintf(p->err, p->err_capacity, "ligne %u : ", (unsigned)p->line);
    if (n < 0 || (size_t)n >= p->err_capacity) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    /* faux positif : args est initialisé par va_start juste au-dessus */
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    (void)vsnprintf(p->err + n, p->err_capacity - (size_t)n, fmt, args);
    va_end(args);
}

uint32_t eng_animgraph_find_param(const EngAnimGraphDef *def, const char *name)
{
    for (uint32_t i = 0; def != NULL && name != NULL && i < def->param_count; i++) {
        if (strcmp(def->params[i].name, name) == 0) {
            return i;
        }
    }
    return UINT32_MAX;
}

uint32_t eng_animgraph_find_state(const EngAnimGraphDef *def, const char *name)
{
    for (uint32_t i = 0; def != NULL && name != NULL && i < def->state_count; i++) {
        if (strcmp(def->states[i].name, name) == 0) {
            return i;
        }
    }
    return UINT32_MAX;
}

uint32_t eng_animgraph_find_clip(const EngAnimGraphDef *def, const char *name)
{
    for (uint32_t i = 0; def != NULL && name != NULL && i < def->clip_count; i++) {
        if (strcmp(def->clips[i].name, name) == 0) {
            return i;
        }
    }
    return UINT32_MAX;
}

static bool name_ok(const char *s)
{
    const size_t n = strlen(s);
    if (n == 0 || n >= ENG_ANIMGRAPH_MAX_NAME) {
        return false;
    }
    for (size_t i = 0; i < n; i++) {
        const unsigned char c = (unsigned char)s[i];
        if (!isalnum(c) && c != '_' && c != '-' && c != '.') {
            return false;
        }
    }
    return true;
}

static bool number(const char *tok, float *out)
{
    char *end = NULL;
    const float v = strtof(tok, &end);
    if (end == tok || *end != '\0' || !isfinite(v)) {
        return false;
    }
    *out = v;
    return true;
}

/* Un nom est-il valide ET libre ? (`what` : « paramètre », « clip »...) */
static bool new_name(Parser *p, const char *tok, const char *what)
{
    if (!name_ok(tok)) {
        fail(p, "nom de %s invalide « %s » (1 à %u caractères parmi lettres, chiffres, _ - .)",
             what, tok, ENG_ANIMGRAPH_MAX_NAME - 1U);
        return false;
    }
    return true;
}

static void do_param(Parser *p, char **t, uint32_t n)
{
    EngAnimGraphDef *d = p->def;
    if (n < 2 || n > 3) {
        fail(p, "param NOM [valeur_initiale]");
        return;
    }
    if (!new_name(p, t[1], "paramètre")) {
        return;
    }
    if (eng_animgraph_find_param(d, t[1]) != UINT32_MAX) {
        fail(p, "paramètre « %s » déjà défini", t[1]);
        return;
    }
    if (d->param_count >= ENG_ANIMGRAPH_MAX_PARAMS) {
        fail(p, "trop de paramètres (maximum %u)", (unsigned)ENG_ANIMGRAPH_MAX_PARAMS);
        return;
    }
    float v = 0.0F;
    if (n == 3 && !number(t[2], &v)) {
        fail(p, "valeur initiale invalide « %s »", t[2]);
        return;
    }
    EngAnimGraphParam *q = &d->params[d->param_count++];
    snprintf(q->name, sizeof q->name, "%s", t[1]);
    q->initial = v;
}

static void do_clip(Parser *p, char **t, uint32_t n)
{
    EngAnimGraphDef *d = p->def;
    if (n < 3) {
        fail(p, "clip NOM chemin.anim [loop|once] [speed=F]");
        return;
    }
    if (!new_name(p, t[1], "clip")) {
        return;
    }
    if (eng_animgraph_find_clip(d, t[1]) != UINT32_MAX) {
        fail(p, "clip « %s » déjà défini", t[1]);
        return;
    }
    if (strlen(t[2]) >= ENG_ANIMGRAPH_MAX_PATH) {
        fail(p, "chemin trop long (maximum %u caractères)", ENG_ANIMGRAPH_MAX_PATH - 1U);
        return;
    }
    if (d->clip_count >= ENG_ANIMGRAPH_MAX_CLIPS) {
        fail(p, "trop de clips (maximum %u)", (unsigned)ENG_ANIMGRAPH_MAX_CLIPS);
        return;
    }
    bool loop = true;
    float speed = 1.0F;
    for (uint32_t i = 3; i < n; i++) {
        if (strcmp(t[i], "loop") == 0) {
            loop = true;
        } else if (strcmp(t[i], "once") == 0) {
            loop = false;
        } else if (strncmp(t[i], "speed=", 6) == 0) {
            if (!number(t[i] + 6, &speed) || speed < 0.0F || speed > 8.0F) {
                fail(p, "vitesse invalide « %s » (0 à 8)", t[i] + 6);
                return;
            }
        } else {
            fail(p, "option de clip inconnue « %s »", t[i]);
            return;
        }
    }
    EngAnimGraphClip *c = &d->clips[d->clip_count++];
    snprintf(c->name, sizeof c->name, "%s", t[1]);
    snprintf(c->path, sizeof c->path, "%s", t[2]);
    c->loop = loop;
    c->speed = speed;
}

static void do_state(Parser *p, char **t, uint32_t n)
{
    EngAnimGraphDef *d = p->def;
    if (n < 4) {
        fail(p, "state NOM clip CLIP  |  state NOM blend PARAM CLIP:POS ...");
        return;
    }
    if (!new_name(p, t[1], "état")) {
        return;
    }
    if (strcmp(t[1], "any") == 0) {
        fail(p, "« any » est réservé aux transitions");
        return;
    }
    if (eng_animgraph_find_state(d, t[1]) != UINT32_MAX) {
        fail(p, "état « %s » déjà défini", t[1]);
        return;
    }
    if (d->state_count >= ENG_ANIMGRAPH_MAX_STATES) {
        fail(p, "trop d'états (maximum %u)", (unsigned)ENG_ANIMGRAPH_MAX_STATES);
        return;
    }
    EngAnimGraphState s;
    memset(&s, 0, sizeof s);
    snprintf(s.name, sizeof s.name, "%s", t[1]);
    if (strcmp(t[2], "clip") == 0) {
        if (n != 4) {
            fail(p, "state NOM clip CLIP : un seul clip");
            return;
        }
        s.kind = ENG_ANIMSTATE_CLIP;
        s.clip = eng_animgraph_find_clip(d, t[3]);
        if (s.clip == UINT32_MAX) {
            fail(p, "clip inconnu « %s » (à déclarer avant l'état)", t[3]);
            return;
        }
    } else if (strcmp(t[2], "blend") == 0) {
        s.kind = ENG_ANIMSTATE_BLEND;
        s.param = eng_animgraph_find_param(d, t[3]);
        if (s.param == UINT32_MAX) {
            fail(p, "paramètre inconnu « %s »", t[3]);
            return;
        }
        if (n < 5) {
            fail(p, "un espace de mélange demande au moins un échantillon CLIP:POS");
            return;
        }
        if (n - 4U > ENG_ANIMGRAPH_MAX_SAMPLES) {
            fail(p, "trop d'échantillons (maximum %u)", (unsigned)ENG_ANIMGRAPH_MAX_SAMPLES);
            return;
        }
        for (uint32_t i = 4; i < n; i++) {
            char *colon = strchr(t[i], ':');
            if (colon == NULL) {
                fail(p, "échantillon « %s » : attendu CLIP:POSITION", t[i]);
                return;
            }
            *colon = '\0';
            const uint32_t clip = eng_animgraph_find_clip(d, t[i]);
            float pos = 0.0F;
            if (clip == UINT32_MAX) {
                fail(p, "clip inconnu « %s »", t[i]);
                return;
            }
            if (!d->clips[clip].loop) {
                fail(p, "le clip « %s » est `once` : un espace de mélange exige des cycles (loop)",
                     t[i]);
                return;
            }
            if (!number(colon + 1, &pos)) {
                fail(p, "position invalide « %s »", colon + 1);
                return;
            }
            if (s.sample_count > 0 && !(pos > s.sample_position[s.sample_count - 1U])) {
                fail(p, "les positions doivent être strictement croissantes");
                return;
            }
            s.sample_clip[s.sample_count] = clip;
            s.sample_position[s.sample_count] = pos;
            s.sample_count++;
        }
    } else {
        fail(p, "type d'état inconnu « %s » (clip ou blend)", t[2]);
        return;
    }
    d->states[d->state_count++] = s;
}

static bool state_ref(Parser *p, const char *tok, bool allow_any, uint32_t *out)
{
    if (allow_any && strcmp(tok, "any") == 0) {
        *out = ENG_ANIMGRAPH_ANY;
        return true;
    }
    const uint32_t i = eng_animgraph_find_state(p->def, tok);
    if (i == UINT32_MAX) {
        fail(p, "état inconnu « %s » (à déclarer avant la transition)", tok);
        return false;
    }
    *out = i;
    return true;
}

static bool parse_fade(Parser *p, const char *tok, float *out)
{
    if (!number(tok, out) || *out < 0.0F || *out > ENG_ANIMGRAPH_MAX_FADE) {
        fail(p, "durée de fondu invalide « %s » (0 à %g)", tok, (double)ENG_ANIMGRAPH_MAX_FADE);
        return false;
    }
    return true;
}

static void do_transition(Parser *p, char **t, uint32_t n)
{
    EngAnimGraphDef *d = p->def;
    if (n < 6 || strcmp(t[2], "->") != 0 || strcmp(t[4], "when") != 0) {
        fail(p, "transition DE -> VERS when ended|PARAM OP VALEUR [fade S]");
        return;
    }
    if (d->transition_count >= ENG_ANIMGRAPH_MAX_TRANSITIONS) {
        fail(p, "trop de transitions (maximum %u)", (unsigned)ENG_ANIMGRAPH_MAX_TRANSITIONS);
        return;
    }
    EngAnimGraphTransition tr;
    memset(&tr, 0, sizeof tr);
    if (!state_ref(p, t[1], true, &tr.from) || !state_ref(p, t[3], false, &tr.to)) {
        return;
    }
    if (tr.from == tr.to) {
        fail(p, "transition d'un état vers lui-même");
        return;
    }
    uint32_t i = 5;
    if (strcmp(t[5], "ended") == 0) {
        tr.cond = ENG_ANIMCOND_ENDED;
        i = 6;
    } else {
        if (n < 8) {
            fail(p, "condition : PARAM OP VALEUR");
            return;
        }
        tr.param = eng_animgraph_find_param(d, t[5]);
        if (tr.param == UINT32_MAX) {
            fail(p, "paramètre inconnu « %s »", t[5]);
            return;
        }
        if (strcmp(t[6], ">") == 0) {
            tr.cond = ENG_ANIMCOND_GT;
        } else if (strcmp(t[6], "<") == 0) {
            tr.cond = ENG_ANIMCOND_LT;
        } else if (strcmp(t[6], ">=") == 0) {
            tr.cond = ENG_ANIMCOND_GE;
        } else if (strcmp(t[6], "<=") == 0) {
            tr.cond = ENG_ANIMCOND_LE;
        } else {
            fail(p, "opérateur inconnu « %s » (> < >= <=)", t[6]);
            return;
        }
        if (!number(t[7], &tr.threshold)) {
            fail(p, "valeur invalide « %s »", t[7]);
            return;
        }
        i = 8;
    }
    if (i < n) {
        if (strcmp(t[i], "fade") != 0 || i + 2U != n || !parse_fade(p, t[i + 1U], &tr.fade)) {
            fail(p, "fin de transition inattendue (attendu : fade SECONDES)");
            return;
        }
    }
    d->transitions[d->transition_count++] = tr;
}

static void do_root_motion(Parser *p, char **t, uint32_t n)
{
    if (n != 2) {
        fail(p, "root_motion ARTICULATION");
        return;
    }
    if (p->def->has_root_motion) {
        fail(p, "root_motion déjà défini");
        return;
    }
    if (!new_name(p, t[1], "articulation")) {
        return;
    }
    p->def->has_root_motion = true;
    snprintf(p->def->root_joint, sizeof p->def->root_joint, "%s", t[1]);
}

static void do_start(Parser *p, char **t, uint32_t n)
{
    if (n != 2) {
        fail(p, "start NOM");
        return;
    }
    if (p->has_start) {
        fail(p, "start déjà défini");
        return;
    }
    uint32_t s = 0;
    if (!state_ref(p, t[1], false, &s)) {
        return;
    }
    p->def->start = s;
    p->has_start = true;
}

static void do_line(Parser *p, char *line)
{
    char *hash = strchr(line, '#');
    if (hash != NULL) {
        *hash = '\0';
    }
    char *tokens[MAX_TOKENS];
    uint32_t n = 0;
    char *save = NULL;
    for (char *tok = SDL_strtok_r(line, " \t\r", &save); tok != NULL;
         tok = SDL_strtok_r(NULL, " \t\r", &save)) {
        if (n >= MAX_TOKENS) {
            fail(p, "ligne trop longue (plus de %u éléments)", (unsigned)MAX_TOKENS);
            return;
        }
        tokens[n++] = tok;
    }
    if (n == 0) {
        return;
    }
    if (strcmp(tokens[0], "param") == 0) {
        do_param(p, tokens, n);
    } else if (strcmp(tokens[0], "clip") == 0) {
        do_clip(p, tokens, n);
    } else if (strcmp(tokens[0], "state") == 0) {
        do_state(p, tokens, n);
    } else if (strcmp(tokens[0], "transition") == 0) {
        do_transition(p, tokens, n);
    } else if (strcmp(tokens[0], "root_motion") == 0) {
        do_root_motion(p, tokens, n);
    } else if (strcmp(tokens[0], "start") == 0) {
        do_start(p, tokens, n);
    } else {
        fail(p, "instruction inconnue « %s »", tokens[0]);
    }
}

bool eng_animgraph_parse(const char *text, EngAnimGraphDef *out, char *err, size_t err_capacity)
{
    if (err != NULL && err_capacity > 0) {
        err[0] = '\0';
    }
    if (out == NULL) {
        return false;
    }
    memset(out, 0, sizeof *out);
    if (text == NULL) {
        if (err != NULL && err_capacity > 0) {
            (void)snprintf(err, err_capacity, "texte absent");
        }
        return false;
    }
    Parser p = {.def = out, .err = err, .err_capacity = err_capacity};
    const char *cursor = text;
    while (*cursor != '\0' && !p.failed) {
        p.line++;
        const char *eol = strchr(cursor, '\n');
        const size_t len = eol != NULL ? (size_t)(eol - cursor) : strlen(cursor);
        if (len >= MAX_LINE) {
            fail(&p, "ligne trop longue (maximum %u caractères)", MAX_LINE - 1U);
            break;
        }
        char buffer[MAX_LINE];
        memcpy(buffer, cursor, len);
        buffer[len] = '\0';
        do_line(&p, buffer);
        cursor += len;
        if (*cursor == '\n') {
            cursor++;
        }
    }
    if (!p.failed && (out->state_count == 0 || !p.has_start)) {
        p.line = 0;
        if (out->state_count == 0) {
            if (err != NULL && err_capacity > 0) {
                (void)snprintf(err, err_capacity, "aucun état défini");
            }
        } else if (err != NULL && err_capacity > 0) {
            (void)snprintf(err, err_capacity, "`start NOM` manquant");
        }
        p.failed = true;
    }
    if (p.failed) {
        memset(out, 0, sizeof *out);
        return false;
    }
    return true;
}

/* ── Fichier surveillé ── */

static bool file_load(EngAnimGraphFile *file, EngArena *scratch, EngAnimGraphDef *def, char *err,
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
        ok = eng_animgraph_parse((const char *)data.data, def, err, err_cap);
    }
    eng_arena_reset_to(scratch, mark);
    return ok;
}

bool eng_animgraph_file_open(EngAnimGraphFile *file, EngArena *scratch, const char *path)
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
    if (!file_load(file, scratch, &file->def, file->error, sizeof file->error)) {
        ENG_LOG_ERROR("machine d'animation %s : %s", path, file->error);
        return false;
    }
    file->valid = true;
    file->version = 1;
    return true;
}

bool eng_animgraph_file_poll(EngAnimGraphFile *file, EngArena *scratch, double now_seconds)
{
    if (file == NULL || scratch == NULL || !eng_file_watch_poll(&file->watch, now_seconds)) {
        return false;
    }
    EngAnimGraphDef next;
    char err[sizeof file->error];
    if (!file_load(file, scratch, &next, err, sizeof err)) {
        memcpy(file->error, err, sizeof err);
        ENG_LOG_ERROR("machine d'animation %s : %s (l'ancienne version reste active)", file->path,
                      err);
        return false;
    }
    file->def = next;
    file->valid = true;
    file->version++;
    file->error[0] = '\0';
    ENG_LOG_INFO("machine d'animation %s rechargée (version %u)", file->path,
                 (unsigned)file->version);
    return true;
}
