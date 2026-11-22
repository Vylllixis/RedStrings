#include "engine/mixer.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

bool eng_mixer_init(EngMixer *m, uint32_t sample_rate)
{
    if (m == NULL || sample_rate < 8000U || sample_rate > 192000U) {
        return false;
    }
    memset(m, 0, sizeof *m);
    m->sample_rate = sample_rate;
    m->master = 1.0F;
    for (uint32_t b = 0; b < ENG_BUS_COUNT; b++) {
        m->bus_volume[b] = 1.0F;
    }
    return true;
}

bool eng_mixer_set_bus(EngMixer *m, EngAudioBus bus, float volume)
{
    if (m == NULL || (uint32_t)bus >= (uint32_t)ENG_BUS_COUNT || !isfinite(volume) ||
        volume < 0.0F || volume > 1.0F) {
        return false;
    }
    m->bus_volume[bus] = volume;
    return true;
}

int32_t eng_mixer_play(EngMixer *m, const float *data, uint32_t length, float gain, float pan,
                       uint32_t delay_frames)
{
    return eng_mixer_play_bus(m, ENG_BUS_SFX, data, length, gain, pan, delay_frames);
}

int32_t eng_mixer_play_bus(EngMixer *m, EngAudioBus bus, const float *data, uint32_t length,
                           float gain, float pan, uint32_t delay_frames)
{
    if (m == NULL || (uint32_t)bus >= (uint32_t)ENG_BUS_COUNT || data == NULL || length == 0U ||
        !isfinite(gain) || gain < 0.0F || gain > 4.0F || !isfinite(pan) || pan < -1.0F ||
        pan > 1.0F) {
        return -1;
    }
    uint32_t slot = 0;
    bool found = false;
    for (uint32_t i = 0; i < ENG_MIXER_VOICES; i++) {
        if (!m->voices[i].active) {
            slot = i;
            found = true;
            break;
        }
    }
    if (!found) { /* vol de la voix la plus ancienne */
        for (uint32_t i = 1; i < ENG_MIXER_VOICES; i++) {
            if (m->voices[i].serial < m->voices[slot].serial) {
                slot = i;
            }
        }
    }
    const float angle = (pan + 1.0F) * 0.78539816F; /* π/4 : puissance constante */
    EngVoice *v = &m->voices[slot];
    v->data = data;
    v->length = length;
    v->pos = 0;
    v->delay = delay_frames;
    v->gain_l = gain * cosf(angle);
    v->gain_r = gain * sinf(angle);
    v->serial = ++m->serial;
    v->bus = bus;
    v->active = true;
    return (int32_t)slot;
}

void eng_mixer_mix(EngMixer *m, float *out, uint32_t frames)
{
    if (out == NULL || frames == 0U) {
        return;
    }
    memset(out, 0, sizeof(float) * 2U * frames);
    if (m == NULL) {
        return;
    }
    for (uint32_t vi = 0; vi < ENG_MIXER_VOICES; vi++) {
        EngVoice *v = &m->voices[vi];
        if (!v->active) {
            continue;
        }
        uint32_t f = 0;
        if (v->delay > 0U) {
            const uint32_t skip = v->delay < frames ? v->delay : frames;
            v->delay -= skip;
            f = skip;
        }
        const float bus = m->bus_volume[v->bus]; /* lu à chaque bloc : réglage en direct */
        const float gl = v->gain_l * bus;
        const float gr = v->gain_r * bus;
        for (; f < frames && v->pos < v->length; f++, v->pos++) {
            const float s = v->data[v->pos];
            const size_t o = (size_t)2U * f;
            out[o] += s * gl;
            out[o + 1U] += s * gr;
        }
        if (v->pos >= v->length) {
            v->active = false;
        }
    }
    for (uint32_t i = 0; i < 2U * frames; i++) {
        float x = out[i] * m->master;
        if (x > 1.0F) {
            x = 1.0F;
            m->clipped++;
        } else if (x < -1.0F) {
            x = -1.0F;
            m->clipped++;
        }
        out[i] = x;
    }
    m->frames_mixed += frames;
}

uint32_t eng_mixer_active(const EngMixer *m)
{
    uint32_t c = 0;
    if (m == NULL) {
        return 0;
    }
    for (uint32_t i = 0; i < ENG_MIXER_VOICES; i++) {
        c += m->voices[i].active ? 1U : 0U;
    }
    return c;
}

void eng_mixer_stop_all(EngMixer *m)
{
    if (m == NULL) {
        return;
    }
    for (uint32_t i = 0; i < ENG_MIXER_VOICES; i++) {
        m->voices[i].active = false;
    }
}

void eng_audio_spatialize(const float listener[2], float yaw, const float source[2], float ref_dist,
                          float max_dist, float *gain, float *pan)
{
    float g = 0.0F;
    float p = 0.0F;
    if (listener != NULL && source != NULL && isfinite(yaw) && isfinite(ref_dist) &&
        isfinite(max_dist) && ref_dist > 0.0F && max_dist > ref_dist) {
        const float dx = source[0] - listener[0];
        const float dz = source[1] - listener[1];
        const float d = sqrtf(dx * dx + dz * dz);
        if (isfinite(d) && d < max_dist) {
            g = d <= ref_dist ? 1.0F : ref_dist / d;
            const float fade_start = 0.9F * max_dist;
            if (d > fade_start) {
                g *= (max_dist - d) / (max_dist - fade_start);
            }
            if (d > 1e-4F) {
                /* repère de l'auditeur : devant = (-sin, -cos), droite = (cos, -sin) */
                const float fx = -sinf(yaw);
                const float fz = -cosf(yaw);
                const float rx = cosf(yaw);
                const float rz = -sinf(yaw);
                const float front = (dx * fx + dz * fz) / d;
                p = (dx * rx + dz * rz) / d;
                p = p > 1.0F ? 1.0F : (p < -1.0F ? -1.0F : p);
                if (front < 0.0F) {
                    g *= 1.0F + 0.25F * front; /* jusqu'à -25 % pile derrière */
                }
            }
        }
    }
    if (gain != NULL) {
        *gain = g;
    }
    if (pan != NULL) {
        *pan = p;
    }
}
