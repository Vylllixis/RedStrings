#include "engine/clock.h"

#include <SDL3/SDL.h>

static double seconds_between(const EngClock *clock, uint64_t from, uint64_t to)
{
    /* Un compteur qui reculerait (ne devrait pas arriver) donne 0 plutôt qu'un dt négatif. */
    if (to <= from) {
        return 0.0;
    }
    uint64_t delta = to - from;
    /* Partie entière et reste séparés : la division directe en double perd de la précision
     * quand le compteur est grand. */
    uint64_t whole = delta / clock->frequency;
    uint64_t rest = delta % clock->frequency;
    return (double)whole + (double)rest / (double)clock->frequency;
}

void eng_clock_init_at(EngClock *clock, uint64_t frequency, uint64_t counter)
{
    *clock = (EngClock){0};
    clock->frequency = frequency != 0 ? frequency : 1;
    clock->start = counter;
    clock->last = counter;
    clock->window_start = counter;
}

void eng_clock_init(EngClock *clock)
{
    eng_clock_init_at(clock, SDL_GetPerformanceFrequency(), SDL_GetPerformanceCounter());
}

void eng_clock_tick_at(EngClock *clock, uint64_t counter)
{
    clock->dt_counter = counter > clock->last ? counter - clock->last : 0;
    clock->dt = seconds_between(clock, clock->last, counter);
    clock->time = seconds_between(clock, clock->start, counter);
    clock->last = counter;
    clock->frame++;
    clock->window_frames++;
    if (clock->dt > clock->window_max_dt) {
        clock->window_max_dt = clock->dt;
    }
}

void eng_clock_tick(EngClock *clock)
{
    eng_clock_tick_at(clock, SDL_GetPerformanceCounter());
}

bool eng_clock_take_stats(EngClock *clock, EngClockStats *out)
{
    double span = seconds_between(clock, clock->window_start, clock->last);
    if (span < 1.0 || clock->window_frames == 0) {
        return false;
    }
    out->frames = (uint32_t)clock->window_frames;
    out->fps = (double)clock->window_frames / span;
    out->avg_dt_ms = span * 1000.0 / (double)clock->window_frames;
    out->max_dt_ms = clock->window_max_dt * 1000.0;

    clock->window_start = clock->last;
    clock->window_frames = 0;
    clock->window_max_dt = 0.0;
    return true;
}

uint64_t eng_clock_counter(void)
{
    return SDL_GetPerformanceCounter();
}

double eng_clock_ms_between(const EngClock *clock, uint64_t from, uint64_t to)
{
    return seconds_between(clock, from, to) * 1000.0;
}
