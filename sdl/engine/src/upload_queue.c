#include "engine/upload_queue.h"

#include <math.h>
#include <string.h>

void eng_upq_init(EngUploadQueue *q, uint32_t budget_bytes)
{
    if (q != NULL) {
        memset(q, 0, sizeof *q);
        q->budget_bytes = budget_bytes > 0U ? budget_bytes : 1U;
    }
}

bool eng_upq_push(EngUploadQueue *q, uint64_t id, uint32_t bytes, float distance)
{
    if (q == NULL || q->count >= ENG_UPLOAD_MAX || bytes == 0U || !isfinite(distance)) {
        return false;
    }
    for (uint32_t i = 0; i < q->count; i++) {
        if (q->req[i].id == id) {
            return false;
        }
    }
    const EngUploadReq r = {id, bytes, distance, 0};
    q->req[q->count++] = r;
    return true;
}

static int32_t index_of(const EngUploadQueue *q, uint64_t id)
{
    for (uint32_t i = 0; i < q->count; i++) {
        if (q->req[i].id == id) {
            return (int32_t)i;
        }
    }
    return -1;
}

static void remove_at(EngUploadQueue *q, uint32_t i)
{
    q->req[i] = q->req[q->count - 1U];
    q->count--;
}

bool eng_upq_cancel(EngUploadQueue *q, uint64_t id)
{
    const int32_t i = q != NULL ? index_of(q, id) : -1;
    if (i < 0) {
        return false;
    }
    remove_at(q, (uint32_t)i);
    return true;
}

bool eng_upq_set_distance(EngUploadQueue *q, uint64_t id, float distance)
{
    const int32_t i = q != NULL ? index_of(q, id) : -1;
    if (i < 0 || !isfinite(distance)) {
        return false;
    }
    q->req[i].distance = distance;
    return true;
}

uint32_t eng_upq_take(EngUploadQueue *q, uint64_t *out_ids, uint32_t max)
{
    if (q == NULL || out_ids == NULL || max == 0U) {
        return 0;
    }
    uint32_t n = 0;
    uint32_t bytes = 0;
    while (n < max && q->count > 0U) {
        uint32_t best = 0;
        for (uint32_t i = 1; i < q->count; i++) {
            const EngUploadReq *a = &q->req[i];
            const EngUploadReq *b = &q->req[best];
            if (a->distance < b->distance || (a->distance == b->distance && a->id < b->id)) {
                best = i;
            }
        }
        const EngUploadReq r = q->req[best];
        if (n > 0U && bytes + r.bytes > q->budget_bytes) {
            break; /* le reste attend l'image suivante */
        }
        out_ids[n++] = r.id;
        bytes += r.bytes;
        q->sent++;
        q->sent_bytes += r.bytes;
        remove_at(q, best);
        if (r.bytes > q->budget_bytes) {
            break; /* trop gros : passe seul */
        }
    }
    for (uint32_t i = 0; i < q->count; i++) {
        q->req[i].age++;
        if (q->req[i].age > q->oldest_wait) {
            q->oldest_wait = q->req[i].age;
        }
    }
    if (bytes > q->max_frame_bytes) {
        q->max_frame_bytes = bytes;
    }
    return n;
}
