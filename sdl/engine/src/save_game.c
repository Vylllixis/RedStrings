#include "engine/save_game.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define HEADER 16U
#define MAX_WORLD_CELLS ENG_WORLD_STATE_CAP

void eng_save_defaults(EngSaveGame *s)
{
    if (s == NULL) {
        return;
    }
    memset(s, 0, sizeof *s);
    eng_prog_init(&s->progress);
    s->hp = 100.0F;
    s->hp_max = 100.0F;
    s->stamina = 100.0F;
    s->posture = 100.0F;
}

/* ── écriture ── */

typedef struct Writer {
    uint8_t *p;
    size_t cap;
    size_t n;
    bool ok;
} Writer;

static void w_bytes(Writer *w, const void *src, size_t len)
{
    if (!w->ok || w->n + len > w->cap) {
        w->ok = false;
        return;
    }
    memcpy(w->p + w->n, src, len);
    w->n += len;
}

static void w_u32(Writer *w, uint32_t v)
{
    const uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    w_bytes(w, b, 4);
}

static void w_f32(Writer *w, float f)
{
    uint32_t u = 0;
    memcpy(&u, &f, 4);
    w_u32(w, u);
}

static void w_f64(Writer *w, double d)
{
    uint64_t u = 0;
    memcpy(&u, &d, 8);
    w_u32(w, (uint32_t)u);
    w_u32(w, (uint32_t)(u >> 32));
}

static size_t section_begin(Writer *w, const char tag[4])
{
    w_bytes(w, tag, 4);
    const size_t at = w->n;
    w_u32(w, 0); /* taille, remplie à la fin */
    return at;
}

static void section_end(Writer *w, size_t at)
{
    if (!w->ok) {
        return;
    }
    const uint32_t size = (uint32_t)(w->n - at - 4U);
    w->p[at] = (uint8_t)size;
    w->p[at + 1U] = (uint8_t)(size >> 8);
    w->p[at + 2U] = (uint8_t)(size >> 16);
    w->p[at + 3U] = (uint8_t)(size >> 24);
}

static uint32_t fnv(const uint8_t *p, size_t n)
{
    uint32_t h = 2166136261U;
    for (size_t i = 0; i < n; i++) {
        h = (h ^ p[i]) * 16777619U;
    }
    return h;
}

static void write_world(Writer *w, const EngWorldState *world)
{
    const size_t at = section_begin(w, "WRLD");
    uint32_t count = 0;
    if (world != NULL) {
        for (uint32_t i = 0; i < ENG_WORLD_STATE_CAP; i++) {
            count += world->used[i] ? 1U : 0U;
        }
    }
    w_u32(w, count);
    if (world != NULL) {
        for (uint32_t i = 0; i < ENG_WORLD_STATE_CAP; i++) {
            if (!world->used[i]) {
                continue;
            }
            const EngCellState *c = &world->cells[i];
            w_u32(w, (uint32_t)c->coord.x);
            w_u32(w, (uint32_t)c->coord.z);
            w_u32(w, (uint32_t)c->removed);
            w_u32(w, (uint32_t)(c->removed >> 32));
            w_u32(w, c->camps_defeated);
            w_u32(w, c->revision);
        }
    }
    section_end(w, at);
}

static size_t finish(Writer *w, uint32_t version, uint32_t sections)
{
    if (!w->ok) {
        return 0;
    }
    w->p[0] = 'R';
    w->p[1] = 'S';
    w->p[2] = 'S';
    w->p[3] = 'V';
    Writer h = {w->p + 4, 12, 0, true};
    w_u32(&h, version);
    w_u32(&h, sections);
    w_u32(&h, fnv(w->p + HEADER, w->n - HEADER));
    return w->n;
}

size_t eng_save_encode(const EngSaveGame *s, const EngWorldState *world, void *out, size_t capacity)
{
    if (s == NULL || out == NULL || capacity < HEADER || s->enemy_count > ENG_SAVE_MAX_ENEMIES) {
        return 0;
    }
    Writer w = {out, capacity, HEADER, true};
    size_t at = section_begin(&w, "PLYR");
    for (int i = 0; i < 3; i++) {
        w_f64(&w, s->player_pos[i]);
    }
    w_f32(&w, s->player_yaw);
    w_f32(&w, s->hp);
    w_f32(&w, s->stamina);
    w_f32(&w, s->posture); /* v2 */
    w_f32(&w, s->hp_max);  /* v2 */
    w_f64(&w, s->play_time);
    section_end(&w, at);
    write_world(&w, world);
    at = section_begin(&w, "ENMY");
    w_u32(&w, s->enemy_count);
    for (uint32_t i = 0; i < s->enemy_count; i++) {
        const EngSaveEnemy *e = &s->enemies[i];
        w_u32(&w, e->alive ? 1U : 0U);
        w_f32(&w, e->hp);
        w_f32(&w, e->pos[0]);
        w_f32(&w, e->pos[1]);
        w_f32(&w, e->yaw);
    }
    section_end(&w, at);
    uint32_t sections = 3U;
    if (s->has_time) {
        at = section_begin(&w, "TIME");
        w_f64(&w, s->hour);
        section_end(&w, at);
        sections++;
    }
    if (s->has_progress) {
        at = section_begin(&w, "PROG");
        w_u32(&w, s->progress.level);
        w_u32(&w, s->progress.xp);
        w_u32(&w, s->progress.total_xp);
        section_end(&w, at);
        sections++;
    }
    return finish(&w, ENG_SAVE_VERSION, sections);
}

size_t eng_save_encode_v1(const EngSaveGame *s, const EngWorldState *world, void *out,
                          size_t capacity)
{
    if (s == NULL || out == NULL || capacity < HEADER) {
        return 0;
    }
    Writer w = {out, capacity, HEADER, true};
    const size_t at = section_begin(&w, "PLYR");
    for (int i = 0; i < 3; i++) {
        w_f64(&w, s->player_pos[i]);
    }
    w_f32(&w, s->player_yaw);
    w_f32(&w, s->hp);
    w_f32(&w, s->stamina);
    section_end(&w, at);
    write_world(&w, world);
    return finish(&w, 1U, 2U);
}

/* ── lecture ── */

typedef struct Reader {
    const uint8_t *p;
    size_t n;
    size_t at;
    bool ok;
} Reader;

static uint32_t r_u32(Reader *r)
{
    if (!r->ok || r->at + 4U > r->n) {
        r->ok = false;
        return 0;
    }
    const uint8_t *b = r->p + r->at;
    r->at += 4U;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static float r_f32(Reader *r)
{
    const uint32_t u = r_u32(r);
    float f = 0.0F;
    memcpy(&f, &u, 4);
    return f;
}

static double r_f64(Reader *r)
{
    const uint64_t lo = r_u32(r);
    const uint64_t hi = r_u32(r);
    const uint64_t u = lo | (hi << 32);
    double d = 0.0;
    memcpy(&d, &u, 8);
    return d;
}

static void fail(char *err, size_t cap, const char *msg)
{
    if (err != NULL && cap > 0U) {
        (void)snprintf(err, cap, "sauvegarde : %s", msg);
    }
}

static bool finite_player(const EngSaveGame *s)
{
    return isfinite(s->player_pos[0]) && isfinite(s->player_pos[1]) && isfinite(s->player_pos[2]) &&
           isfinite(s->player_yaw) && isfinite(s->hp) && isfinite(s->stamina) &&
           isfinite(s->posture) && isfinite(s->hp_max) && s->hp_max > 0.0F &&
           isfinite(s->play_time);
}

bool eng_save_decode(const void *data, size_t size, EngSaveGame *out, EngWorldState *world,
                     char *err, size_t err_capacity)
{
    const uint8_t *p = data;
    if (p == NULL || out == NULL || size < HEADER || memcmp(p, "RSSV", 4) != 0) {
        fail(err, err_capacity, "signature absente");
        return false;
    }
    Reader h = {p + 4, 12, 0, true};
    const uint32_t version = r_u32(&h);
    const uint32_t sections = r_u32(&h);
    const uint32_t sum = r_u32(&h);
    if (version == 0U || version > ENG_SAVE_VERSION) {
        fail(err, err_capacity,
             version == 0U ? "version 0 invalide" : "version plus récente que le jeu");
        return false;
    }
    if (fnv(p + HEADER, size - HEADER) != sum) {
        fail(err, err_capacity, "somme de contrôle fausse (fichier abîmé)");
        return false;
    }
    EngSaveGame s;
    eng_save_defaults(&s);
    s.version_read = version;
    static EngWorldState tmp_world; /* appliqué seulement si tout le fichier est valide */
    eng_wstate_init(&tmp_world);
    bool have_player = false;
    Reader r = {p, size, HEADER, true};
    for (uint32_t k = 0; k < sections && r.ok; k++) {
        if (r.at + 8U > size) {
            r.ok = false;
            break;
        }
        char tag[4];
        memcpy(tag, p + r.at, 4);
        r.at += 4U;
        const uint32_t len = r_u32(&r);
        if (!r.ok || r.at + len > size) {
            r.ok = false;
            break;
        }
        Reader sec = {p + r.at, len, 0, true};
        r.at += len;
        if (memcmp(tag, "PLYR", 4) == 0) {
            for (int i = 0; i < 3; i++) {
                s.player_pos[i] = r_f64(&sec);
            }
            s.player_yaw = r_f32(&sec);
            s.hp = r_f32(&sec);
            s.stamina = r_f32(&sec);
            if (version >= 2U) {
                s.posture = r_f32(&sec);
                s.hp_max = r_f32(&sec);
                s.play_time = r_f64(&sec);
            }
            have_player = sec.ok;
            r.ok = sec.ok;
        } else if (memcmp(tag, "WRLD", 4) == 0) {
            const uint32_t count = r_u32(&sec);
            if (count > MAX_WORLD_CELLS - 1U) {
                r.ok = false;
                break;
            }
            for (uint32_t i = 0; i < count && sec.ok; i++) {
                const EngCellCoord c = {(int32_t)r_u32(&sec), (int32_t)r_u32(&sec)};
                const uint64_t lo = r_u32(&sec);
                const uint64_t hi = r_u32(&sec);
                const uint32_t camps = r_u32(&sec);
                const uint32_t rev = r_u32(&sec);
                EngCellState *cs = sec.ok ? eng_wstate_edit(&tmp_world, c) : NULL;
                if (cs == NULL) {
                    sec.ok = false;
                    break;
                }
                cs->removed = lo | (hi << 32);
                cs->camps_defeated = camps;
                cs->revision = rev;
            }
            r.ok = sec.ok;
        } else if (memcmp(tag, "ENMY", 4) == 0) {
            const uint32_t count = r_u32(&sec);
            if (count > ENG_SAVE_MAX_ENEMIES) {
                r.ok = false;
                break;
            }
            for (uint32_t i = 0; i < count; i++) {
                EngSaveEnemy *e = &s.enemies[i];
                e->alive = r_u32(&sec) != 0U;
                e->hp = r_f32(&sec);
                e->pos[0] = r_f32(&sec);
                e->pos[1] = r_f32(&sec);
                e->yaw = r_f32(&sec);
                if (!isfinite(e->hp) || !isfinite(e->pos[0]) || !isfinite(e->pos[1]) ||
                    !isfinite(e->yaw)) {
                    sec.ok = false;
                }
            }
            s.enemy_count = count;
            s.has_enemies = sec.ok;
            r.ok = sec.ok;
        } else if (memcmp(tag, "TIME", 4) == 0) {
            const double h = r_f64(&sec);
            if (!sec.ok || !isfinite(h) || h < 0.0 || h >= 24.0) {
                r.ok = false;
                break;
            }
            s.hour = h;
            s.has_time = true;
        } else if (memcmp(tag, "PROG", 4) == 0) {
            EngProgress pg;
            pg.level = r_u32(&sec);
            pg.xp = r_u32(&sec);
            pg.total_xp = r_u32(&sec);
            if (!sec.ok || !eng_prog_valid(&pg)) {
                r.ok = false;
                break;
            }
            s.progress = pg;
            s.has_progress = true;
        }
        /* section inconnue : ignorée */
    }
    if (!r.ok || !have_player || !finite_player(&s)) {
        fail(err, err_capacity,
             !have_player && r.ok ? "section PLYR absente" : "section tronquée ou valeur invalide");
        return false;
    }
    *out = s;
    if (world != NULL) {
        *world = tmp_world;
    }
    return true;
}

bool eng_save_write_file(const char *path, const EngSaveGame *s, const EngWorldState *world)
{
    if (path == NULL || s == NULL) {
        return false;
    }
    static uint8_t buf[256U * 1024U];
    const size_t n = eng_save_encode(s, world, buf, sizeof buf);
    if (n == 0U) {
        return false;
    }
    char tmp[1100];
    (void)snprintf(tmp, sizeof tmp, "%s.tmp", path);
    if (!SDL_SaveFile(tmp, buf, n)) {
        return false;
    }
    if (!SDL_RenamePath(tmp, path)) {
        (void)SDL_RemovePath(tmp);
        return false;
    }
    return true;
}

bool eng_save_read_file(const char *path, EngSaveGame *out, EngWorldState *world, char *err,
                        size_t err_capacity)
{
    size_t size = 0;
    void *data = path != NULL ? SDL_LoadFile(path, &size) : NULL;
    if (data == NULL) {
        fail(err, err_capacity, "fichier introuvable");
        return false;
    }
    const bool ok = eng_save_decode(data, size, out, world, err, err_capacity);
    SDL_free(data);
    return ok;
}
