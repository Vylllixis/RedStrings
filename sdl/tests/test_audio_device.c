/* Sortie audio SDL (M5-19), pilote « dummy » : pas de haut-parleur, on vérifie le rappel. */
#include <SDL3/SDL.h>
#include <stdlib.h>

#include "engine/audio_device.h"
#include "engine/combat_audio.h"
#include "utest.h"

UTEST(audio_device, the_callback_pulls_frames_from_the_mixer_and_plays_queued_sounds)
{
    ASSERT_TRUE(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"));
    static EngMixer mixer;
    static EngCombatAudio ca;
    ASSERT_TRUE(eng_mixer_init(&mixer, 48000));
    ASSERT_TRUE(eng_combat_audio_init(&ca, &mixer));
    EngAudioDevice *dev = eng_audio_device_open(&mixer);
    ASSERT_TRUE(dev != NULL);
    eng_audio_device_lock(dev);
    ASSERT_TRUE(eng_combat_audio_play(&ca, ENG_SFX_HIT_LIGHT, 1.0F, 0.0F, 0));
    eng_audio_device_unlock(dev);
    /* le pilote « dummy » consomme en temps réel : on attend jusqu'à 3 s que la voix soit lue */
    bool done = false;
    for (int i = 0; i < 300 && !done; i++) {
        SDL_Delay(10);
        eng_audio_device_lock(dev);
        done = eng_mixer_active(&mixer) == 0U && mixer.frames_mixed > 0U;
        eng_audio_device_unlock(dev);
    }
    EXPECT_TRUE(done);
    EXPECT_GT(eng_audio_device_callbacks(dev), 0U);
    eng_audio_device_lock(dev);
    EXPECT_GE(mixer.frames_mixed, (uint64_t)ca.length[ENG_SFX_HIT_LIGHT]);
    eng_audio_device_unlock(dev);
    eng_audio_device_close(dev);
}

UTEST(audio_device, invalid_arguments_are_harmless)
{
    EXPECT_TRUE(eng_audio_device_open(NULL) == NULL);
    eng_audio_device_close(NULL);
    eng_audio_device_lock(NULL);
    eng_audio_device_unlock(NULL);
    EXPECT_EQ(eng_audio_device_callbacks(NULL), 0U);
}
