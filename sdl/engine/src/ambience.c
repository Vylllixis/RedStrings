#include "engine/ambience.h"

#include <math.h>
#include <string.h>

#define TAU 6.28318530718F

/* Générateur pseudo-aléatoire déterministe (même ambiance à chaque lancement). */
static uint32_t next_random(uint32_t *s)
{
    *s ^= *s << 13;
    *s ^= *s >> 17;
    *s ^= *s << 5;
    return *s;
}

static float random_unit(uint32_t *s) /* [-1 ; 1[ */
{
    return ((float)(next_random(s) & 0xFFFFFFU) / 8388608.0F) - 1.0F;
}

/* Rend une boucle sans couture : les `fade` dernières images sont fondues dans les premières (le
 * tampon a été rempli sur length + fade images). */
static void make_loopable(float *buf, uint32_t length, uint32_t fade)
{
    for (uint32_t i = 0; i < fade; i++) {
        const float t = (float)i / (float)fade;
        buf[i] = (buf[i] * t) + (buf[length + i] * (1.0F - t));
    }
}

/* Ramène le pic de la couche à `target` (niveaux prévisibles, jamais d'écrêtage seul). */
static void normalize(float *buf, uint32_t n, float target)
{
    float peak = 0.0F;
    for (uint32_t i = 0; i < n; i++) {
        peak = fabsf(buf[i]) > peak ? fabsf(buf[i]) : peak;
    }
    if (peak > 1e-6F) {
        const float k = target / peak;
        for (uint32_t i = 0; i < n; i++) {
            buf[i] *= k;
        }
    }
}

static void synth_wind(float *out, uint32_t n, uint32_t rate)
{
    uint32_t seed = 0x1234567U;
    float brown = 0.0F;
    float low = 0.0F;
    for (uint32_t i = 0; i < n; i++) {
        brown = (brown * 0.995F) + (random_unit(&seed) * 0.05F); /* bruit brun */
        low += (brown - low) * 0.02F;                            /* passe-bas */
        const float t = (float)i / (float)rate;
        const float gust =
            0.55F + (0.25F * sinf(TAU * 0.11F * t)) + (0.2F * sinf(TAU * 0.037F * t));
        out[i] = low * gust * 2.5F;
    }
}

static void add_chirp(float *out, uint32_t n, uint32_t at, uint32_t rate, float f0, float f1,
                      float dur, float amp)
{
    const uint32_t len = (uint32_t)(dur * (float)rate);
    float phase = 0.0F;
    for (uint32_t k = 0; k < len && at + k < n; k++) {
        const float u = (float)k / (float)len;
        const float f = f0 + ((f1 - f0) * u);
        phase += TAU * f / (float)rate;
        const float env = sinf(3.14159265F * u); /* attaque et chute douces */
        out[at + k] += amp * env * env * sinf(phase);
    }
}

static void synth_birds(float *out, uint32_t n, uint32_t rate)
{
    memset(out, 0, sizeof(float) * n);
    uint32_t seed = 0xBEEF123U;
    const uint32_t calls = (uint32_t)((float)n / (float)rate * 1.5F); /* ~1,5 appel par seconde */
    for (uint32_t c = 0; c < calls; c++) {
        uint32_t at = next_random(&seed) % n;
        const float base = 2600.0F + (1400.0F * (random_unit(&seed) + 1.0F) * 0.5F);
        const uint32_t notes = 2U + (next_random(&seed) % 4U);
        for (uint32_t k = 0; k < notes; k++) {
            add_chirp(out, n, at, rate, base, base * (1.25F + (0.2F * random_unit(&seed))), 0.07F,
                      0.12F);
            at += (uint32_t)(0.09F * (float)rate);
        }
    }
}

static void synth_crickets(float *out, uint32_t n, uint32_t rate)
{
    memset(out, 0, sizeof(float) * n);
    uint32_t seed = 0xC0FFEE1U;
    /* trilles : salves de 8 à 12 impulsions à 4,6 kHz, ~1,4 salve par seconde, deux insectes */
    for (int insect = 0; insect < 2; insect++) {
        const float freq = insect == 0 ? 4600.0F : 5100.0F;
        float t = 0.1F * (float)insect;
        while (t < (float)n / (float)rate) {
            const uint32_t pulses = 8U + (next_random(&seed) % 5U);
            for (uint32_t p = 0; p < pulses; p++) {
                const uint32_t at = (uint32_t)((t + ((float)p * 0.035F)) * (float)rate);
                const uint32_t len = (uint32_t)(0.018F * (float)rate);
                for (uint32_t k = 0; k < len && at + k < n; k++) {
                    const float u = (float)k / (float)len;
                    out[at + k] += 0.07F * sinf(3.14159265F * u) *
                                   sinf(TAU * freq * (float)(at + k) / (float)rate);
                }
            }
            t += 0.55F + (0.35F * (random_unit(&seed) + 1.0F));
        }
    }
}

static void synth_music(float *out, uint32_t n, uint32_t rate)
{
    memset(out, 0, sizeof(float) * n);
    /* la mineur, fa majeur, do majeur, sol majeur : 8 s chacun, fondus de 2 s */
    static const float chords[4][3] = {{220.00F, 261.63F, 329.63F},
                                       {174.61F, 220.00F, 261.63F},
                                       {261.63F, 329.63F, 392.00F},
                                       {196.00F, 246.94F, 293.66F}};
    const float chord_s = 8.0F;
    for (uint32_t i = 0; i < n; i++) {
        const float t = (float)i / (float)rate;
        float v = 0.0F;
        for (int c = 0; c < 4; c++) {
            /* enveloppe de l'accord c : rampes de 2 s autour de sa fenêtre [8c ; 8c + 8], prise
             * modulo 32 s (l'accord 0 commence à monter à 30 s : raccord continu) */
            const float local = fmodf(t - (chord_s * (float)c) + 34.0F, 32.0F) - 2.0F;
            float env = 0.0F;
            if (local > -2.0F && local < chord_s + 2.0F) {
                env = local < 0.0F ? (local + 2.0F) / 2.0F
                                   : (local > chord_s ? (chord_s + 2.0F - local) / 2.0F : 1.0F);
            }
            for (int k = 0; k < 3; k++) {
                /* arrondie au 1/16 Hz : f, 2f et f/2 font un nombre entier de périodes sur
                 * 32 s, la boucle se raccorde sans saut (écart de hauteur inaudible) */
                const float f = roundf(chords[c][k] * 16.0F) / 16.0F;
                v += env * (sinf(TAU * f * t) + (0.3F * sinf(TAU * f * 2.0F * t)) +
                            (0.15F * sinf(TAU * f * 0.5F * t)));
            }
        }
        const float tremolo = 0.85F + (0.15F * sinf(TAU * 0.25F * t));
        out[i] = 0.035F * v * tremolo;
    }
}

bool eng_ambience_init(EngAmbience *a, EngArena *arena, uint32_t sample_rate)
{
    if (a == NULL || arena == NULL || sample_rate < 8000U || sample_rate > 192000U) {
        return false;
    }
    memset(a, 0, sizeof *a);
    a->rate = sample_rate;
    static const float seconds[ENG_AMB_LAYER_COUNT] = {8.0F, 6.0F, 6.0F, 32.0F};
    const uint32_t fade = sample_rate / 4U; /* 250 ms de raccord */
    for (uint32_t l = 0; l < ENG_AMB_LAYER_COUNT; l++) {
        const uint32_t len = (uint32_t)(seconds[l] * (float)sample_rate);
        float *buf = ENG_ARENA_NEW_ARRAY(arena, float, (size_t)len + fade);
        if (buf == NULL) {
            return false;
        }
        switch ((EngAmbienceLayer)l) {
        case ENG_AMB_WIND:
            synth_wind(buf, len + fade, sample_rate);
            make_loopable(buf, len, fade);
            break;
        case ENG_AMB_BIRDS:
            synth_birds(buf, len + fade, sample_rate);
            make_loopable(buf, len, fade);
            break;
        case ENG_AMB_CRICKETS:
            synth_crickets(buf, len + fade, sample_rate);
            make_loopable(buf, len, fade);
            break;
        default: /* musique : boucle exacte par construction (32 s, raccord des accords) */
            synth_music(buf, len, sample_rate);
            break;
        }
        normalize(buf, len, l == ENG_AMB_CRICKETS ? 0.4F : 0.5F);
        a->data[l] = buf;
        a->length[l] = len;
    }
    a->ready = true;
    return true;
}

float eng_ambience_gain(EngAmbienceLayer layer, float night)
{
    const float n = !isfinite(night) ? 0.0F : (night < 0.0F ? 0.0F : (night > 1.0F ? 1.0F : night));
    switch (layer) {
    case ENG_AMB_WIND:
        return 0.35F + (0.1F * n); /* un peu plus présent la nuit (plus de silence autour) */
    case ENG_AMB_BIRDS:
        return 0.8F * (1.0F - n);
    case ENG_AMB_CRICKETS:
        return 0.8F * n;
    case ENG_AMB_MUSIC:
        return 0.6F;
    default:
        return 0.0F;
    }
}

void eng_ambience_update(EngAmbience *a, EngMixer *m, float night, uint32_t lookahead)
{
    if (a == NULL || m == NULL || !a->ready) {
        return;
    }
    const uint64_t now = m->frames_mixed;
    if (!a->started) {
        for (uint32_t l = 0; l < ENG_AMB_LAYER_COUNT; l++) {
            a->next_start[l] = now; /* tout de suite */
        }
        a->started = true;
    }
    for (uint32_t l = 0; l < ENG_AMB_LAYER_COUNT; l++) {
        if (a->next_start[l] < now) {
            a->next_start[l] = now; /* retard trop grand (mixeur arrêté, pause) : on repart */
        }
        while (a->next_start[l] <= now + lookahead) {
            const float gain = eng_ambience_gain((EngAmbienceLayer)l, night);
            const EngAudioBus bus = l == ENG_AMB_MUSIC ? ENG_BUS_MUSIC : ENG_BUS_AMBIENCE;
            const uint32_t delay = (uint32_t)(a->next_start[l] - now);
            if (gain > 0.0F) {
                (void)eng_mixer_play_bus(m, bus, a->data[l], a->length[l], gain, 0.0F, delay);
            }
            a->last_gain[l] = gain;
            a->loops[l]++;
            a->next_start[l] += a->length[l];
        }
    }
}
