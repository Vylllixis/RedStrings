#include "engine/loop.h"

#include "engine/log.h"

bool eng_loop_init(EngLoop *loop, uint64_t counter_frequency, uint32_t tick_hz,
                   double max_frame_seconds)
{
    *loop = (EngLoop){0};
    if (counter_frequency == 0 || tick_hz == 0 || !(max_frame_seconds > 0.0)) {
        ENG_LOG_ERROR("boucle : paramètres invalides (fréquence %llu, %u Hz, plafond %f s)",
                      (unsigned long long)counter_frequency, (unsigned)tick_hz, max_frame_seconds);
        return false;
    }
    double max_delta = max_frame_seconds * (double)counter_frequency;
    if (max_delta < 1.0 || max_delta > 1e15) { /* borne haute : évite tout débordement plus loin */
        ENG_LOG_ERROR("boucle : plafond de %f s hors limites", max_frame_seconds);
        return false;
    }
    loop->frequency = counter_frequency;
    loop->tick_hz = tick_hz;
    loop->max_delta = (uint64_t)max_delta;
    loop->tick_seconds = 1.0 / (double)tick_hz;
    return true;
}

uint32_t eng_loop_advance(EngLoop *loop, uint64_t frame_delta_counter)
{
    if (frame_delta_counter > loop->max_delta) {
        frame_delta_counter = loop->max_delta;
        loop->clamped_frames++;
    }

    loop->accumulator += frame_delta_counter * loop->tick_hz;
    uint64_t steps = loop->accumulator / loop->frequency;
    loop->accumulator %= loop->frequency;

    loop->tick_count += steps;
    loop->alpha = (double)loop->accumulator / (double)loop->frequency;
    return (uint32_t)steps;
}
