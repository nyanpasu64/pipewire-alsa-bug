#include <alsa/asoundlib.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>

static char const *device = "default";            /* playback device */

constexpr size_t FRAMES = 16 * 1024;
constexpr size_t CHANNELS = 2;

int16_t buffer[FRAMES * CHANNELS];              /* some random data */

int main(void)
{
    using namespace std::chrono_literals;

    int err;
    unsigned int i;
    snd_pcm_t *handle;
    snd_pcm_sframes_t frames;

    for (auto & value : buffer)
        value = random();

    if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
    if ((err = snd_pcm_set_params(handle,
                      SND_PCM_FORMAT_S16,
                      SND_PCM_ACCESS_RW_INTERLEAVED,
                      CHANNELS,
                      48000,
                      1,
                      500000)) < 0) {   /* 0.5sec */
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    unsigned ctr = 0;

    while (1) {
        printf("before writei %d\n", snd_pcm_state(handle));
        frames = snd_pcm_writei(handle, buffer, FRAMES);
        if (frames < 0)
            frames = snd_pcm_recover(handle, frames, 0);
        if (frames < 0) {
            printf("snd_pcm_writei failed: %d %s\n", frames, snd_strerror(frames));
            break;
        }
        if (frames > 0 && frames < (long)FRAMES)
            printf("Short write (expected %li, wrote %li)\n", FRAMES, frames);

        if (ctr == 0) {
            ctr += 1;
            printf("before drain, state %d\n", snd_pcm_state(handle));
            snd_pcm_drain(handle);
            printf("before prepare, state %d\n", snd_pcm_state(handle));
            snd_pcm_prepare(handle);
        }
    }

    /* pass the remaining samples, otherwise they're dropped in close */
    err = snd_pcm_drain(handle);
    if (err < 0)
        printf("snd_pcm_drain failed: %s\n", snd_strerror(err));
    snd_pcm_close(handle);
    return 0;
}
