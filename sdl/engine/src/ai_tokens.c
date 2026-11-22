#include "engine/ai_tokens.h"

#include <math.h>
#include <string.h>

void eng_tokens_init(EngAiTokens *t, uint32_t max)
{
    if (t == NULL) {
        return;
    }
    t->max = max > ENG_AI_TOKEN_SLOTS ? ENG_AI_TOKEN_SLOTS : max;
    for (uint32_t i = 0; i < ENG_AI_TOKEN_SLOTS; i++) {
        t->holder[i] = ENG_AI_NO_HOLDER;
    }
}

bool eng_tokens_has(const EngAiTokens *t, uint32_t id)
{
    if (t == NULL) {
        return false;
    }
    for (uint32_t i = 0; i < t->max; i++) {
        if (t->holder[i] == id) {
            return true;
        }
    }
    return false;
}

uint32_t eng_tokens_in_use(const EngAiTokens *t)
{
    uint32_t n = 0;
    if (t == NULL) {
        return 0;
    }
    for (uint32_t i = 0; i < t->max; i++) {
        if (t->holder[i] != ENG_AI_NO_HOLDER) {
            n++;
        }
    }
    return n;
}

void eng_tokens_release(EngAiTokens *t, uint32_t id)
{
    if (t == NULL) {
        return;
    }
    for (uint32_t i = 0; i < t->max; i++) {
        if (t->holder[i] == id) {
            t->holder[i] = ENG_AI_NO_HOLDER;
        }
    }
}

void eng_tokens_arbitrate(EngAiTokens *t, const uint32_t *ids, const float *dist, uint32_t n)
{
    if (t == NULL || ids == NULL || dist == NULL) {
        return;
    }
    bool taken[ENG_AI_TOKEN_SLOTS * 4U] = {false};
    if (n > ENG_AI_TOKEN_SLOTS * 4U) {
        n = ENG_AI_TOKEN_SLOTS * 4U;
    }
    for (;;) {
        uint32_t slot = ENG_AI_TOKEN_SLOTS;
        for (uint32_t i = 0; i < t->max; i++) {
            if (t->holder[i] == ENG_AI_NO_HOLDER) {
                slot = i;
                break;
            }
        }
        if (slot == ENG_AI_TOKEN_SLOTS) {
            return;
        }
        int32_t best = -1;
        for (uint32_t k = 0; k < n; k++) {
            if (taken[k] || eng_tokens_has(t, ids[k])) {
                continue;
            }
            if (best < 0 || dist[k] < dist[best] || (dist[k] == dist[best] && ids[k] < ids[best])) {
                best = (int32_t)k;
            }
        }
        if (best < 0) {
            return;
        }
        taken[best] = true;
        t->holder[slot] = ids[best];
    }
}

void eng_ai_ring(const float center[2], const float (*pos)[2], uint32_t n, float radius,
                 float (*out)[2])
{
    if (center == NULL || pos == NULL || out == NULL || n == 0U) {
        return;
    }
    const float tau = 6.28318530718F;
    bool used[ENG_AI_TOKEN_SLOTS * 4U] = {false};
    if (n > ENG_AI_TOKEN_SLOTS * 4U) {
        n = ENG_AI_TOKEN_SLOTS * 4U;
    }
    /* le créneau 0 est tourné vers l'ennemi d'indice 0 : le placement ne dépend que des positions
     */
    const float base = atan2f(pos[0][1] - center[1], pos[0][0] - center[0]);
    for (uint32_t k = 0; k < n; k++) {
        const float ang = atan2f(pos[k][1] - center[1], pos[k][0] - center[0]);
        int32_t best = -1;
        float best_d = 1e9F;
        for (uint32_t s = 0; s < n; s++) {
            if (used[s]) {
                continue;
            }
            const float sa = base + tau * (float)s / (float)n;
            float d = fabsf(remainderf(ang - sa, tau));
            if (d < best_d) {
                best_d = d;
                best = (int32_t)s;
            }
        }
        used[best] = true;
        const float sa = base + tau * (float)best / (float)n;
        out[k][0] = center[0] + cosf(sa) * radius;
        out[k][1] = center[1] + sinf(sa) * radius;
    }
}
