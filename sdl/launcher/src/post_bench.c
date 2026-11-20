#include "post_bench.h"

#include <string.h>

#include "engine/log.h"

typedef struct BenchConfig {
    const char *name;
    bool fxaa;
    float bloom;
    float ssao;
    float scale;
} BenchConfig;

static const BenchConfig CONFIGS[POST_BENCH_CONFIGS] = {
    {"base (aucun effet)", false, 0.0F, 0.0F, 1.0F},
    {"FXAA", true, 0.0F, 0.0F, 1.0F},
    {"bloom", false, 0.5F, 0.0F, 1.0F},
    {"SSAO", false, 0.0F, 0.6F, 1.0F},
    {"tout (FXAA, bloom, SSAO)", true, 0.5F, 0.6F, 1.0F},
    {"tout, échelle 75 %", true, 0.5F, 0.6F, 0.75F},
    {"tout, échelle 50 %", true, 0.5F, 0.6F, 0.5F},
};

void post_bench_init(PostBench *b, uint32_t frames_per_config)
{
    memset(b, 0, sizeof *b);
    b->frames_per_config = frames_per_config;
    b->active = frames_per_config > 0U;
}

double post_bench_mean(const PostBench *b, uint32_t k)
{
    if (b == NULL || k >= POST_BENCH_CONFIGS || b->counted[k] == 0U) {
        return -1.0;
    }
    return b->sum_ms[k] / (double)b->counted[k];
}

static void log_results(const PostBench *b)
{
    const double base = post_bench_mean(b, 0);
    ENG_LOG_INFO("mesure du post-process : %u configurations, %u images chacune (sans vsync)",
                 POST_BENCH_CONFIGS, b->frames_per_config);
    for (uint32_t k = 0; k < POST_BENCH_CONFIGS; k++) {
        const double mean = post_bench_mean(b, k);
        ENG_LOG_INFO("  post-process %-28s %7.3f ms par image (%+.3f ms par rapport à la base)",
                     CONFIGS[k].name, mean, mean - base);
    }
}

bool post_bench_frame(PostBench *b, double previous_frame_ms, EngTonemap *tonemap, EngGpu *gpu)
{
    if (b == NULL || !b->active || tonemap == NULL || gpu == NULL) {
        return true;
    }
    if (b->frame > POST_BENCH_WARMUP && previous_frame_ms > 0.0) {
        b->sum_ms[b->config] += previous_frame_ms;
        b->counted[b->config]++;
    }
    if (b->frame >= POST_BENCH_WARMUP + b->frames_per_config) {
        b->config++;
        b->frame = 0;
        if (b->config >= POST_BENCH_CONFIGS) {
            log_results(b);
            b->active = false;
            eng_gpu_set_render_scale(gpu, 1.0F);
            return false;
        }
    }
    const BenchConfig *c = &CONFIGS[b->config];
    tonemap->fxaa = c->fxaa;
    tonemap->bloom = c->bloom;
    tonemap->ssao = c->ssao;
    eng_gpu_set_render_scale(gpu, c->scale);
    if (gpu->present_mode != ENG_PRESENT_IMMEDIATE) {
        (void)eng_gpu_set_present_mode(gpu, ENG_PRESENT_IMMEDIATE);
    }
    b->frame++;
    return true;
}
