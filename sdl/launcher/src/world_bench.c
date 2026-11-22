#include "world_bench.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/ai_area.h"
#include "engine/jobs.h"
#include "engine/log.h"
#include "engine/upload_queue.h"
#include "engine/world_origin.h"
#include "engine/world_stream.h"

#define BUDGET_MS 25.0
#define MAX_FRAMES 400000U

static EngJobs g_jobs;
static EngWorldStream g_stream;
static EngAiArea g_area;
static EngUploadQueue g_uploads;
static float g_frame_ms[MAX_FRAMES];

static uint64_t cell_id(const EngCellData *c)
{
    return ((uint64_t)(uint32_t)c->coord.x << 32) | (uint32_t)c->coord.z;
}

static void on_ready(void *user, const EngCellData *c)
{
    (void)user;
    /* hauteurs (65² floats) + objets : ce qu'un renderer enverrait au GPU */
    (void)eng_upq_push(&g_uploads, cell_id(c),
                       (uint32_t)(sizeof c->heights + (size_t)c->object_count * 64U), 0.0F);
}

static void on_unload(void *user, const EngCellData *c)
{
    (void)user;
    (void)eng_upq_cancel(&g_uploads, cell_id(c));
}

static int cmp_float(const void *a, const void *b)
{
    const float x = *(const float *)a;
    const float y = *(const float *)b;
    return (x > y) - (x < y);
}

bool world_bench_run(double km, double speed, const char *report_path)
{
    if (!(km > 0.0) || !(speed > 0.0)) {
        return false;
    }
    if (!eng_jobs_init(&g_jobs, 0)) {
        ENG_LOG_ERROR("benchmark : pool de threads impossible");
        return false;
    }
    const EngWorldGen gen = eng_world_gen_default(1);
    if (!eng_stream_init(&g_stream, &gen, &g_jobs, 2, NULL)) {
        eng_jobs_shutdown(&g_jobs);
        return false;
    }
    g_stream.on_ready = on_ready;
    g_stream.on_unload = on_unload;
    eng_upq_init(&g_uploads, 64U * 1024U); /* 64 Ko par image */
    eng_ai_area_init(&g_area, 32.0F, 0.5F);
    EngWorldOrigin origin;
    eng_origin_init(&origin, 512.0);
    double p[3] = {128.0, 0.0, 128.0};
    const uint64_t t_start = SDL_GetPerformanceCounter();
    for (int i = 0; i < 5000 && !eng_stream_ready_around(&g_stream, p[0], p[2], 2); i++) {
        (void)eng_stream_update(&g_stream, p[0], p[2]);
        SDL_Delay(1);
    }
    const double warmup_ms = (double)(SDL_GetPerformanceCounter() - t_start) * 1000.0 /
                             (double)SDL_GetPerformanceFrequency();
    const double step = speed / 60.0;
    uint32_t frames = (uint32_t)(km * 1000.0 / step);
    frames = frames > MAX_FRAMES ? MAX_FRAMES : frames;
    uint32_t holes = 0;
    uint64_t ids[32];
    const double freq = (double)SDL_GetPerformanceFrequency();
    for (uint32_t f = 0; f < frames; f++) {
        const uint64_t t0 = SDL_GetPerformanceCounter();
        /* trajet en arc : change de direction, passe par des coins de cellules */
        const double heading = 0.6 + 0.4 * sin((double)f / 3000.0);
        p[0] += step * cos(heading);
        p[2] += step * sin(heading);
        (void)eng_stream_update(&g_stream, p[0], p[2]);
        float shift[3];
        if (eng_origin_update(&origin, p, shift)) {
            eng_ai_area_shift(&g_area, shift);
        }
        float local[3];
        eng_origin_to_local(&origin, p, local);
        const float focus[2] = {local[0], local[2]};
        (void)eng_ai_area_update(&g_area, &g_stream, &origin, focus);
        (void)eng_upq_take(&g_uploads, ids, 32);
        g_frame_ms[f] = (float)((double)(SDL_GetPerformanceCounter() - t0) * 1000.0 / freq);
        if (!eng_stream_ready_around(&g_stream, p[0], p[2], 1)) {
            holes++;
        }
        SDL_DelayNS(
            2000000ULL); /* le reste d'une image (rendu) : les threads chargent pendant ce temps */
    }
    double sum = 0.0;
    float worst = 0.0F;
    for (uint32_t f = 0; f < frames; f++) {
        sum += g_frame_ms[f];
        worst = g_frame_ms[f] > worst ? g_frame_ms[f] : worst;
    }
    qsort(g_frame_ms, frames, sizeof g_frame_ms[0], cmp_float);
    const float p99 = frames > 0U ? g_frame_ms[(size_t)((double)frames * 0.99)] : 0.0F;
    const bool ok = holes == 0U && worst <= BUDGET_MS;
    char report[2048];
    (void)snprintf(
        report, sizeof report,
        "benchmark monde ouvert (M7-14)\n"
        "trajet            : %.1f km a %.0f m/s, %u images a 60 Hz, %u threads de travail\n"
        "prechargement     : %.0f ms\n"
        "CPU thread princ. : moyenne %.3f ms, p99 %.3f ms, pire %.3f ms (budget %.0f ms)\n"
        "trous sous joueur : %u\n"
        "cellules          : %llu chargees, %llu dechargees, %u emplacements au plus (sur %u)\n"
        "memoire streaming : %zu Ko (fixe)\n"
        "origine flottante : %u recentrages, position finale (%.0f, %.0f) m\n"
        "navigation IA     : %u reconstructions de la fenetre de 64 m\n"
        "uploads GPU       : %llu envoyes, %u octets au plus par image (budget %u), attente max %u "
        "images\n"
        "resultat          : %s\n",
        km, speed, frames, g_jobs.worker_count, warmup_ms, sum / (frames > 0U ? frames : 1U),
        (double)p99, (double)worst, BUDGET_MS, holes, (unsigned long long)g_stream.loads_done,
        (unsigned long long)g_stream.unloads, g_stream.peak_slots, ENG_STREAM_SLOTS,
        eng_stream_memory() / 1024U, origin.rebases, p[0], p[2], g_area.rebuilds,
        (unsigned long long)g_uploads.sent, g_uploads.max_frame_bytes, g_uploads.budget_bytes,
        g_uploads.oldest_wait, ok ? "OK" : "ECHEC");
    ENG_LOG_INFO("%s", report);
    if (report_path != NULL) {
        FILE *fp = fopen(report_path, "w");
        if (fp != NULL) {
            (void)fputs(report, fp);
            (void)fclose(fp);
        }
    }
    ENG_LOG_INFO("BENCHMARK MONDE : %s", ok ? "OK" : "ÉCHEC");
    eng_stream_shutdown(&g_stream);
    eng_jobs_shutdown(&g_jobs);
    return ok;
}
