#include "engine/combat_replay.h"

#include <math.h>
#include <string.h>

#define TWO_PI 6.28318530717958647692F
#define PI_F 3.14159265358979323846F
#define HEADER_BYTES 24U /* magic, version, combattants, ticks, empreinte (8) */

EngPackedInput eng_input_pack(const EngFighterInput *in)
{
    EngPackedInput p = {0, 0};
    if (in == NULL) {
        return p;
    }
    p.flags =
        (uint8_t)((in->light ? ENG_PIN_LIGHT : 0U) | (in->heavy ? ENG_PIN_HEAVY : 0U) |
                  (in->dodge ? ENG_PIN_DODGE : 0U) |
                  (in->block_pressed ? ENG_PIN_BLOCK_PRESSED : 0U) |
                  (in->block_held ? ENG_PIN_BLOCK_HELD : 0U) | (in->moving ? ENG_PIN_MOVING : 0U));
    const float x = in->move_dir[0];
    const float z = in->move_dir[1];
    if (isfinite(x) && isfinite(z) && sqrtf(x * x + z * z) > 0.5F) {
        const float t = (atan2f(z, x) + PI_F) / TWO_PI; /* 0..1 */
        long q = 1L + lroundf(t * 254.0F);
        if (q > 255L) {
            q = 255L; /* atan2 = π : même direction que q = 1 */
        }
        p.dir = (uint8_t)q;
    }
    return p;
}

EngFighterInput eng_input_unpack(EngPackedInput p)
{
    EngFighterInput in;
    memset(&in, 0, sizeof in);
    in.light = (p.flags & ENG_PIN_LIGHT) != 0U;
    in.heavy = (p.flags & ENG_PIN_HEAVY) != 0U;
    in.dodge = (p.flags & ENG_PIN_DODGE) != 0U;
    in.block_pressed = (p.flags & ENG_PIN_BLOCK_PRESSED) != 0U;
    in.block_held = (p.flags & ENG_PIN_BLOCK_HELD) != 0U;
    in.moving = (p.flags & ENG_PIN_MOVING) != 0U;
    if (p.dir != 0U) {
        const float a = ((float)(p.dir - 1U) / 254.0F) * TWO_PI - PI_F;
        in.move_dir[0] = cosf(a);
        in.move_dir[1] = sinf(a);
    }
    return in;
}

bool eng_replay_init(EngReplay *r, EngPackedInput *storage, uint32_t capacity, uint32_t fighters)
{
    if (r == NULL || storage == NULL || fighters == 0U || fighters > ENG_SIM_MAX ||
        capacity == 0U || capacity > ENG_REPLAY_MAX_TICKS) {
        return false;
    }
    r->fighters = fighters;
    r->capacity = capacity;
    r->ticks = 0;
    r->data = storage;
    return true;
}

bool eng_replay_record(EngReplay *r, const EngPackedInput *tick_inputs)
{
    if (r == NULL || r->data == NULL || tick_inputs == NULL || r->ticks >= r->capacity) {
        return false;
    }
    memcpy(&r->data[(size_t)r->ticks * r->fighters], tick_inputs,
           sizeof(EngPackedInput) * r->fighters);
    r->ticks++;
    return true;
}

const EngPackedInput *eng_replay_tick(const EngReplay *r, uint32_t t)
{
    if (r == NULL || r->data == NULL || t >= r->ticks) {
        return NULL;
    }
    return &r->data[(size_t)t * r->fighters];
}

bool eng_replay_setup_sim(EngCombatSim *s, const EngAttackSet *attacks, const EngReplaySetup *setup)
{
    if (setup == NULL || !eng_sim_init(s, attacks, setup->count)) {
        return false;
    }
    for (uint32_t i = 0; i < setup->count; i++) {
        if (!eng_sim_place(s, i, setup->x[i], setup->z[i], setup->yaw[i])) {
            return false;
        }
    }
    return true;
}

bool eng_replay_run(const EngReplay *r, const EngAttackSet *attacks, const EngReplaySetup *setup,
                    uint64_t *final_hash, uint64_t *trace, uint32_t trace_cap)
{
    if (r == NULL || setup == NULL || final_hash == NULL || r->fighters != setup->count) {
        return false;
    }
    EngCombatSim s;
    if (!eng_replay_setup_sim(&s, attacks, setup)) {
        return false;
    }
    for (uint32_t t = 0; t < r->ticks; t++) {
        const EngPackedInput *p = eng_replay_tick(r, t);
        EngFighterInput in[ENG_SIM_MAX];
        for (uint32_t i = 0; i < r->fighters; i++) {
            in[i] = eng_input_unpack(p[i]);
        }
        eng_sim_step(&s, in);
        if (trace != NULL && t < trace_cap) {
            trace[t] = eng_sim_hash(&s);
        }
    }
    *final_hash = eng_sim_hash(&s);
    return true;
}

uint32_t eng_replay_first_divergence(const uint64_t *a, const uint64_t *b, uint32_t n)
{
    if (a == NULL || b == NULL) {
        return n;
    }
    for (uint32_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return i;
        }
    }
    return n;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void putf(uint8_t *p, float f)
{
    uint32_t u = 0;
    memcpy(&u, &f, sizeof u);
    put32(p, u);
}

static float getf(const uint8_t *p)
{
    const uint32_t u = get32(p);
    float f = 0.0F;
    memcpy(&f, &u, sizeof f);
    return f;
}

size_t eng_replay_size(const EngReplay *r)
{
    if (r == NULL) {
        return 0;
    }
    return HEADER_BYTES + (size_t)r->fighters * 12U +
           (size_t)r->ticks * r->fighters * sizeof(EngPackedInput);
}

size_t eng_replay_write(const EngReplay *r, const EngReplaySetup *setup, uint64_t final_hash,
                        uint8_t *out, size_t cap)
{
    if (r == NULL || setup == NULL || out == NULL || r->data == NULL ||
        setup->count != r->fighters) {
        return 0;
    }
    const size_t need = eng_replay_size(r);
    if (cap < need) {
        return 0;
    }
    out[0] = (uint8_t)'R';
    out[1] = (uint8_t)'S';
    out[2] = (uint8_t)'R';
    out[3] = (uint8_t)'P';
    put32(out + 4, 1U);
    put32(out + 8, r->fighters);
    put32(out + 12, r->ticks);
    put32(out + 16, (uint32_t)(final_hash & 0xFFFFFFFFULL));
    put32(out + 20, (uint32_t)(final_hash >> 32));
    uint8_t *p = out + HEADER_BYTES;
    for (uint32_t i = 0; i < r->fighters; i++) {
        putf(p, setup->x[i]);
        putf(p + 4, setup->z[i]);
        putf(p + 8, setup->yaw[i]);
        p += 12;
    }
    for (size_t i = 0; i < (size_t)r->ticks * r->fighters; i++) {
        *p++ = r->data[i].flags;
        *p++ = r->data[i].dir;
    }
    return need;
}

bool eng_replay_read(const uint8_t *buf, size_t len, EngPackedInput *storage, uint32_t capacity,
                     EngReplay *r, EngReplaySetup *setup, uint64_t *final_hash)
{
    if (buf == NULL || storage == NULL || r == NULL || setup == NULL || final_hash == NULL ||
        len < HEADER_BYTES || memcmp(buf, "RSRP", 4) != 0 || get32(buf + 4) != 1U) {
        return false;
    }
    const uint32_t fighters = get32(buf + 8);
    const uint32_t ticks = get32(buf + 12);
    if (fighters == 0U || fighters > ENG_SIM_MAX || ticks > ENG_REPLAY_MAX_TICKS ||
        ticks > capacity) {
        return false;
    }
    const size_t need =
        HEADER_BYTES + (size_t)fighters * 12U + (size_t)ticks * fighters * sizeof(EngPackedInput);
    if (len != need) {
        return false;
    }
    EngReplaySetup su;
    memset(&su, 0, sizeof su);
    su.count = fighters;
    const uint8_t *p = buf + HEADER_BYTES;
    for (uint32_t i = 0; i < fighters; i++) {
        su.x[i] = getf(p);
        su.z[i] = getf(p + 4);
        su.yaw[i] = getf(p + 8);
        if (!isfinite(su.x[i]) || !isfinite(su.z[i]) || !isfinite(su.yaw[i])) {
            return false;
        }
        p += 12;
    }
    for (size_t i = 0; i < (size_t)ticks * fighters; i++) {
        if ((p[0] & ~ENG_PIN_ALL) != 0U) {
            return false;
        }
        p += 2;
    }
    p = buf + HEADER_BYTES + (size_t)fighters * 12U;
    for (size_t i = 0; i < (size_t)ticks * fighters; i++) {
        storage[i].flags = p[0];
        storage[i].dir = p[1];
        p += 2;
    }
    r->fighters = fighters;
    r->capacity = capacity;
    r->ticks = ticks;
    r->data = storage;
    *setup = su;
    *final_hash = (uint64_t)get32(buf + 16) | ((uint64_t)get32(buf + 20) << 32);
    return true;
}
