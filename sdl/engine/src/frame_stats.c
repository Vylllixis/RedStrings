#include "engine/frame_stats.h"

#include <math.h>
#include <string.h>

void eng_frame_stats_init(EngFrameStats *stats)
{
    memset(stats, 0, sizeof *stats);
}

static float sanitize(float v, bool *ok)
{
    if (!isfinite(v) || v < 0.0F) {
        *ok = false;
        return 0.0F;
    }
    return v;
}

bool eng_frame_stats_push(EngFrameStats *stats, float frame_ms, float cpu_ms)
{
    bool ok = true;
    stats->frame_ms[stats->next] = sanitize(frame_ms, &ok);
    stats->cpu_ms[stats->next] = sanitize(cpu_ms, &ok);
    stats->next = (stats->next + 1U) % ENG_FRAME_STATS_HISTORY;
    if (stats->count < ENG_FRAME_STATS_HISTORY) {
        stats->count++;
    }
    stats->total++;
    return ok;
}

/* Indice dans le tampon de la i-ème valeur chronologique (0 : la plus ancienne). */
static uint32_t slot(const EngFrameStats *stats, uint32_t i)
{
    return (stats->next + ENG_FRAME_STATS_HISTORY - stats->count + i) % ENG_FRAME_STATS_HISTORY;
}

EngFrameSummary eng_frame_stats_summary(const EngFrameStats *stats)
{
    EngFrameSummary s;
    memset(&s, 0, sizeof s);
    s.count = stats->count;
    if (stats->count == 0) {
        return s;
    }
    double sum_frame = 0.0;
    double sum_cpu = 0.0;
    for (uint32_t i = 0; i < stats->count; i++) {
        const float f = stats->frame_ms[slot(stats, i)];
        const float c = stats->cpu_ms[slot(stats, i)];
        sum_frame += (double)f;
        sum_cpu += (double)c;
        if (f > s.max_frame_ms) {
            s.max_frame_ms = f;
        }
        if (c > s.max_cpu_ms) {
            s.max_cpu_ms = c;
        }
    }
    s.avg_frame_ms = (float)(sum_frame / (double)stats->count);
    s.avg_cpu_ms = (float)(sum_cpu / (double)stats->count);
    s.fps = s.avg_frame_ms > 0.0F ? 1000.0F / s.avg_frame_ms : 0.0F;
    return s;
}

float eng_frame_stats_frame_at(const EngFrameStats *stats, uint32_t i)
{
    return i < stats->count ? stats->frame_ms[slot(stats, i)] : 0.0F;
}

float eng_frame_stats_cpu_at(const EngFrameStats *stats, uint32_t i)
{
    return i < stats->count ? stats->cpu_ms[slot(stats, i)] : 0.0F;
}

static uint32_t copy_values(const EngFrameStats *stats, const float *src, float *out,
                            uint32_t capacity)
{
    const uint32_t n = stats->count < capacity ? stats->count : capacity;
    const uint32_t skip = stats->count - n; /* on garde les plus récentes */
    for (uint32_t i = 0; i < n; i++) {
        out[i] = src[slot(stats, skip + i)];
    }
    return n;
}

uint32_t eng_frame_stats_copy_frame(const EngFrameStats *stats, float *out, uint32_t capacity)
{
    return copy_values(stats, stats->frame_ms, out, capacity);
}

uint32_t eng_frame_stats_copy_cpu(const EngFrameStats *stats, float *out, uint32_t capacity)
{
    return copy_values(stats, stats->cpu_ms, out, capacity);
}
