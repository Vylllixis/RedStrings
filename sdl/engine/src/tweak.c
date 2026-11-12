#include "engine/tweak.h"

#include <math.h>
#include <string.h>

void eng_tweaks_init(EngTweaks *t)
{
    memset(t, 0, sizeof *t);
}

void eng_tweaks_clear(EngTweaks *t)
{
    eng_tweaks_init(t);
}

static EngTweak *find_mut(EngTweaks *t, const char *name)
{
    if (name == NULL) {
        return NULL;
    }
    for (uint32_t i = 0; i < t->count; i++) {
        if (strcmp(t->items[i].name, name) == 0) {
            return &t->items[i];
        }
    }
    return NULL;
}

const EngTweak *eng_tweaks_find(const EngTweaks *t, const char *name)
{
    return find_mut((EngTweaks *)t, name);
}

/* Place une nouvelle entrée ; les vérifications communes sont ici. */
static EngTweak *reserve(EngTweaks *t, const char *name, void *value)
{
    if (name == NULL || name[0] == '\0' || value == NULL || t->count >= ENG_TWEAK_MAX ||
        find_mut(t, name) != NULL) {
        return NULL;
    }
    return &t->items[t->count];
}

bool eng_tweaks_add_float(EngTweaks *t, const char *name, float *value, float min, float max)
{
    EngTweak *e = reserve(t, name, value);
    if (e == NULL || !isfinite(min) || !isfinite(max) || min > max || !isfinite(*value) ||
        *value < min || *value > max) {
        return false;
    }
    *e = (EngTweak){name, ENG_TWEAK_FLOAT, value, min, max, *value};
    t->count++;
    return true;
}

bool eng_tweaks_add_int(EngTweaks *t, const char *name, int32_t *value, int32_t min, int32_t max)
{
    EngTweak *e = reserve(t, name, value);
    if (e == NULL || min > max || *value < min || *value > max) {
        return false;
    }
    *e = (EngTweak){name, ENG_TWEAK_INT, value, (float)min, (float)max, (float)*value};
    t->count++;
    return true;
}

bool eng_tweaks_add_bool(EngTweaks *t, const char *name, bool *value)
{
    EngTweak *e = reserve(t, name, value);
    if (e == NULL) {
        return false;
    }
    *e = (EngTweak){name, ENG_TWEAK_BOOL, value, 0.0F, 1.0F, *value ? 1.0F : 0.0F};
    t->count++;
    return true;
}

static EngTweak *find_typed(EngTweaks *t, const char *name, EngTweakType type)
{
    EngTweak *e = find_mut(t, name);
    return e != NULL && e->type == type ? e : NULL;
}

bool eng_tweaks_set_float(EngTweaks *t, const char *name, float v)
{
    EngTweak *e = find_typed(t, name, ENG_TWEAK_FLOAT);
    if (e == NULL || isnan(v)) {
        return false;
    }
    *(float *)e->value = v < e->min ? e->min : (v > e->max ? e->max : v); /* ±infini : borné */
    return true;
}

bool eng_tweaks_set_int(EngTweaks *t, const char *name, int32_t v)
{
    EngTweak *e = find_typed(t, name, ENG_TWEAK_INT);
    if (e == NULL) {
        return false;
    }
    const int32_t lo = (int32_t)e->min;
    const int32_t hi = (int32_t)e->max;
    *(int32_t *)e->value = v < lo ? lo : (v > hi ? hi : v);
    return true;
}

bool eng_tweaks_set_bool(EngTweaks *t, const char *name, bool v)
{
    EngTweak *e = find_typed(t, name, ENG_TWEAK_BOOL);
    if (e == NULL) {
        return false;
    }
    *(bool *)e->value = v;
    return true;
}

bool eng_tweaks_get_float(const EngTweaks *t, const char *name, float *out)
{
    const EngTweak *e = find_typed((EngTweaks *)t, name, ENG_TWEAK_FLOAT);
    if (e == NULL) {
        return false;
    }
    *out = *(const float *)e->value;
    return true;
}

bool eng_tweaks_get_int(const EngTweaks *t, const char *name, int32_t *out)
{
    const EngTweak *e = find_typed((EngTweaks *)t, name, ENG_TWEAK_INT);
    if (e == NULL) {
        return false;
    }
    *out = *(const int32_t *)e->value;
    return true;
}

bool eng_tweaks_get_bool(const EngTweaks *t, const char *name, bool *out)
{
    const EngTweak *e = find_typed((EngTweaks *)t, name, ENG_TWEAK_BOOL);
    if (e == NULL) {
        return false;
    }
    *out = *(const bool *)e->value;
    return true;
}

bool eng_tweaks_reset(EngTweaks *t, uint32_t index)
{
    if (index >= t->count) {
        return false;
    }
    EngTweak *e = &t->items[index];
    switch (e->type) {
    case ENG_TWEAK_FLOAT:
        *(float *)e->value = e->initial;
        break;
    case ENG_TWEAK_INT:
        *(int32_t *)e->value = (int32_t)e->initial;
        break;
    case ENG_TWEAK_BOOL:
        *(bool *)e->value = e->initial != 0.0F;
        break;
    }
    return true;
}

void eng_tweaks_reset_all(EngTweaks *t)
{
    for (uint32_t i = 0; i < t->count; i++) {
        (void)eng_tweaks_reset(t, i);
    }
}
