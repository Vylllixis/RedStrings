#include "engine/audio_device.h"

#include <SDL3/SDL.h>
#include <stdlib.h>

#include "engine/log.h"

#define CHUNK_FRAMES 1024U

struct EngAudioDevice {
    SDL_AudioStream *stream;
    EngMixer *mixer;
    uint32_t callbacks;
};

static void SDLCALL audio_callback(void *userdata, SDL_AudioStream *stream, int additional_amount,
                                   int total_amount)
{
    (void)total_amount;
    EngAudioDevice *d = (EngAudioDevice *)userdata;
    d->callbacks++;
    float chunk[2U * CHUNK_FRAMES];
    int remaining = additional_amount;
    while (remaining > 0) {
        uint32_t frames = (uint32_t)remaining / (2U * (uint32_t)sizeof(float));
        if (frames == 0U) {
            break;
        }
        if (frames > CHUNK_FRAMES) {
            frames = CHUNK_FRAMES;
        }
        eng_mixer_mix(d->mixer, chunk, frames);
        const int bytes = (int)((size_t)frames * 2U * sizeof(float));
        if (!SDL_PutAudioStreamData(stream, chunk, bytes)) {
            break;
        }
        remaining -= bytes;
    }
}

EngAudioDevice *eng_audio_device_open(EngMixer *mixer)
{
    if (mixer == NULL) {
        return NULL;
    }
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        ENG_LOG_WARN("audio : SDL_INIT_AUDIO a échoué : %s", SDL_GetError());
        return NULL;
    }
    EngAudioDevice *d = (EngAudioDevice *)SDL_calloc(1, sizeof *d);
    if (d == NULL) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return NULL;
    }
    d->mixer = mixer;
    const SDL_AudioSpec spec = {SDL_AUDIO_F32, 2, (int)mixer->sample_rate};
    d->stream =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_callback, d);
    if (d->stream == NULL) {
        ENG_LOG_WARN("audio : ouverture du périphérique impossible : %s", SDL_GetError());
        SDL_free(d);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return NULL;
    }
    if (!SDL_ResumeAudioStreamDevice(d->stream)) {
        ENG_LOG_WARN("audio : démarrage impossible : %s", SDL_GetError());
        SDL_DestroyAudioStream(d->stream);
        SDL_free(d);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return NULL;
    }
    return d;
}

void eng_audio_device_close(EngAudioDevice *d)
{
    if (d == NULL) {
        return;
    }
    SDL_DestroyAudioStream(d->stream); /* ferme aussi le périphérique */
    SDL_free(d);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void eng_audio_device_lock(EngAudioDevice *d)
{
    if (d != NULL) {
        (void)SDL_LockAudioStream(d->stream);
    }
}

void eng_audio_device_unlock(EngAudioDevice *d)
{
    if (d != NULL) {
        (void)SDL_UnlockAudioStream(d->stream);
    }
}

uint32_t eng_audio_device_callbacks(EngAudioDevice *d)
{
    if (d == NULL) {
        return 0;
    }
    eng_audio_device_lock(d);
    const uint32_t c = d->callbacks;
    eng_audio_device_unlock(d);
    return c;
}
