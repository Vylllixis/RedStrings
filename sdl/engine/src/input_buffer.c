#include "engine/input_buffer.h"

#include <stddef.h>
#include <string.h>

void eng_ibuf_init(EngInputBuffer *b, uint32_t window)
{
    if (b == NULL) {
        return;
    }
    memset(b, 0, sizeof *b);
    b->window = window == 0U ? ENG_IBUF_DEFAULT_WINDOW : window;
}

uint32_t eng_ibuf_priority(EngCombatAction a)
{
    switch (a) {
    case ENG_ACT_DODGE:
        return 4U;
    case ENG_ACT_BLOCK:
        return 3U;
    case ENG_ACT_HEAVY:
        return 2U;
    case ENG_ACT_LIGHT:
        return 1U;
    default:
        return 0U;
    }
}

bool eng_ibuf_push(EngInputBuffer *b, EngCombatAction action, uint32_t tick)
{
    if (b == NULL || eng_ibuf_priority(action) == 0U) {
        return false;
    }
    if (b->count == ENG_IBUF_SIZE) { /* plein : on écrase la plus ancienne (indice 0) */
        memmove(&b->entries[0], &b->entries[1], (ENG_IBUF_SIZE - 1U) * sizeof b->entries[0]);
        b->count--;
    }
    b->entries[b->count].action = action;
    b->entries[b->count].tick = tick;
    b->count++;
    return true;
}

static void drop_expired(EngInputBuffer *b, uint32_t now)
{
    uint32_t w = 0;
    for (uint32_t i = 0; i < b->count; i++) {
        if (now < b->entries[i].tick || now - b->entries[i].tick <= b->window) {
            b->entries[w++] = b->entries[i];
        }
    }
    b->count = w;
}

static int find_best(const EngInputBuffer *b, uint32_t now, uint32_t allowed)
{
    int best = -1;
    for (uint32_t i = 0; i < b->count; i++) {
        const EngIbufEntry *e = &b->entries[i];
        if ((allowed & ENG_ACT_MASK(e->action)) == 0U || e->tick > now ||
            now - e->tick > b->window) {
            continue;
        }
        /* entrées rangées de la plus ancienne à la plus récente : `>` garde la plus ancienne */
        if (best < 0 || eng_ibuf_priority(e->action) > eng_ibuf_priority(b->entries[best].action)) {
            best = (int)i;
        }
    }
    return best;
}

EngCombatAction eng_ibuf_peek(const EngInputBuffer *b, uint32_t now, uint32_t allowed)
{
    if (b == NULL) {
        return ENG_ACT_NONE;
    }
    const int i = find_best(b, now, allowed);
    return i < 0 ? ENG_ACT_NONE : b->entries[i].action;
}

EngCombatAction eng_ibuf_take(EngInputBuffer *b, uint32_t now, uint32_t allowed)
{
    if (b == NULL) {
        return ENG_ACT_NONE;
    }
    drop_expired(b, now);
    const int i = find_best(b, now, allowed);
    if (i < 0) {
        return ENG_ACT_NONE;
    }
    const EngCombatAction a = b->entries[i].action;
    memmove(&b->entries[i], &b->entries[i + 1],
            (b->count - (uint32_t)i - 1U) * sizeof b->entries[0]);
    b->count--;
    return a;
}

void eng_ibuf_clear(EngInputBuffer *b)
{
    if (b != NULL) {
        b->count = 0;
    }
}
