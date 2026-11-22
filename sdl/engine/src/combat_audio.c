#include "engine/combat_audio.h"

#include <math.h>
#include <string.h>

bool eng_combat_audio_init(EngCombatAudio *a, EngMixer *mixer)
{
    if (a == NULL || mixer == NULL || mixer->sample_rate < 8000U ||
        mixer->sample_rate > ENG_SFX_MAX_RATE) {
        return false;
    }
    memset(a, 0, sizeof *a);
    a->mixer = mixer;
    for (uint32_t i = 0; i < (uint32_t)ENG_SFX_COUNT; i++) {
        a->length[i] =
            eng_sfx_synth((EngSfx)i, mixer->sample_rate, a->bank[i], ENG_SFX_MAX_SAMPLES);
        if (a->length[i] == 0U) {
            return false;
        }
    }
    return true;
}

uint64_t eng_audio_tick_frame(uint32_t tick, uint32_t sample_rate)
{
    return (uint64_t)tick * (uint64_t)sample_rate / 60U;
}

bool eng_combat_audio_play(EngCombatAudio *a, EngSfx s, float gain, float pan, uint32_t delay)
{
    if (a == NULL || a->mixer == NULL || (unsigned)s >= (unsigned)ENG_SFX_COUNT) {
        return false;
    }
    return eng_mixer_play(a->mixer, a->bank[s], a->length[s], gain, pan, delay) >= 0;
}

bool eng_combat_audio_on_event(EngCombatAudio *a, const EngSimEvent *e, const float listener[2],
                               float listener_yaw)
{
    if (a == NULL || a->mixer == NULL || e == NULL || listener == NULL || !isfinite(listener[0]) ||
        !isfinite(listener[1]) || !isfinite(listener_yaw) || e->kind == ENG_HITKIND_IGNORED) {
        return false;
    }
    EngSfx s = ENG_SFX_HIT_LIGHT;
    float gain = 0.6F + 0.4F * fminf(e->dealt / 25.0F, 1.0F);
    switch (e->kind) {
    case ENG_HITKIND_BLOCKED:
        s = ENG_SFX_BLOCK;
        gain = 0.7F;
        break;
    case ENG_HITKIND_PARRIED:
        s = ENG_SFX_PARRY;
        gain = 0.9F;
        break;
    default:
        s = e->hitstop >= 5U ? ENG_SFX_HIT_HEAVY : ENG_SFX_HIT_LIGHT;
        if (e->zone == ENG_ZONE_HEAD) {
            gain *= 1.1F;
        }
        break;
    }
    const float dx = e->point[0] - listener[0];
    const float dz = e->point[2] - listener[1];
    const float dist = sqrtf(dx * dx + dz * dz);
    const float right = dx * cosf(listener_yaw) + dz * -sinf(listener_yaw);
    float pan = right / 5.0F;
    pan = pan > 1.0F ? 1.0F : (pan < -1.0F ? -1.0F : pan);
    gain *= 1.0F / (1.0F + 0.1F * dist);
    const uint64_t start = eng_audio_tick_frame(e->tick, a->mixer->sample_rate);
    const uint32_t delay =
        start > a->mixer->frames_mixed ? (uint32_t)(start - a->mixer->frames_mixed) : 0U;
    return eng_combat_audio_play(a, s, gain, pan, delay);
}
