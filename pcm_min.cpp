#include <alsa/asoundlib.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

static char const *device = "default";            /* playback device */
unsigned char buffer[16*1024];              /* some random data */

int drain = 0;

static void fn() {
    using namespace std::chrono_literals;

    int err;
    snd_pcm_t *handle;
    if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
    if ((err = snd_pcm_set_params(handle,
                      SND_PCM_FORMAT_U8,
                      SND_PCM_ACCESS_RW_INTERLEAVED,
                      1,
                      48000,
                      1,
                      500000)) < 0) {   /* 0.5sec */
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    snd_pcm_sframes_t frames;

    while (1) {
        // If you don't sleep, the main thread *never* gets to acquire the mutex.
        std::this_thread::sleep_for(5ms);

        snd_pcm_state_t state = snd_pcm_state(handle);
        std::cerr << "state " << state << "\n";
//        if (state != SND_PCM_STATE_PREPARED && state != SND_PCM_STATE_RUNNING) {
        if (drain % 4 == 2) {
            snd_pcm_drain(handle);
            std::cerr << "new state " << snd_pcm_state(handle) << "\n";
            snd_pcm_prepare(handle);
        }
        drain = (drain + 1) % 4;

        frames = snd_pcm_writei(handle, buffer, sizeof(buffer));
        if (frames < 0)
            frames = snd_pcm_recover(handle, frames, 0);
        if (frames < 0) {
            printf("snd_pcm_writei failed: %s\n", snd_strerror(frames));
            break;
        }
        if (frames > 0 && frames < (long)sizeof(buffer))
            printf("Short write (expected %li, wrote %li)\n", (long)sizeof(buffer), frames);
    }
}

int main(void)
{
    for (size_t i = 0; i < sizeof(buffer); i++)
        buffer[i] = random() & 0xff;

    fn();
}
