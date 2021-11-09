#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

#include <thread> // for sleep

#include <math.h>
#ifndef M_PI
#define M_PI 3.141592653589793
#endif

#define max(x,y) (((x)>(y))?(x):(y))
#define min(x,y) (((x)<(y))?(x):(y))

double freq = 256;
double volume = 0.2;

float mixbuffer[4096*256];
int16_t outbuffer[4096*256];
static void mix_sine(float * buffer, uint64_t count, uint64_t samplerate, uint64_t channels, uint64_t time)
{
    double nyquist = samplerate/2.0;
    for(uint64_t i = 0; i < count; i++)
    {
        for(uint64_t c = 0; c < channels; c++)
        {
            buffer[i*channels + c] = sin((i+time)*M_PI/nyquist*freq)*volume;
        }
    }
}

#define perr(...) fprintf(stderr, __VA_ARGS__)

#define TRY(...) { \
    auto _v = __VA_ARGS__; \
    if (_v < 0) { \
        printf("%s failed with error code %d", #__VA_ARGS__, _v); \
        exit(1); \
    } \
}

#define MUT
#define OUT

int main()
{
    using namespace std::chrono_literals;

    snd_pcm_t * device;
    TRY(snd_pcm_open(&device, "default", SND_PCM_STREAM_PLAYBACK, 0))

    unsigned int MUT samplerate = 48000;
    unsigned int const channels = 2;
    snd_pcm_uframes_t MUT samples;

    {
        snd_pcm_hw_params_t * parameters;
        TRY(snd_pcm_hw_params_malloc(&parameters))

        TRY(snd_pcm_hw_params_any(device, parameters))
        TRY(snd_pcm_hw_params_set_access(device, parameters, SND_PCM_ACCESS_RW_INTERLEAVED))
        TRY(snd_pcm_hw_params_set_format(device, parameters, SND_PCM_FORMAT_S16_LE))
        TRY(snd_pcm_hw_params_set_rate_near(device, parameters, &samplerate, 0))
        TRY(snd_pcm_hw_params_set_channels(device, parameters, channels))

        perr("actual sample rate %d\n", samplerate);

        snd_pcm_uframes_t period;
        int dir;

        // Sets period = 32.
        TRY(snd_pcm_hw_params_get_period_size_min(parameters, &OUT period, &OUT dir))

        // Setting this to ≤255, and running the app when no audio has played
        // in the last ~3 seconds, locks up pipewire until you close the app.
        period = 255;

        perr("min period size: %lu\n", period);
        TRY(snd_pcm_hw_params_set_period_size(device, parameters, period, dir))

        samples = max(period, 1024);
        perr("pretend buffer size is %lu\n", samples);

        // Only necessary on hw devices.
        TRY(snd_pcm_hw_params_set_buffer_size_near(device, parameters, &samples))

        TRY(snd_pcm_hw_params(device, parameters))
        snd_pcm_hw_params_free(parameters);
    }

    if (snd_pcm_prepare(device) < 0)
        return puts("Failed to prepare device"), 0;

    uint64_t t = 0;
    while(1)
    {
        std::this_thread::sleep_for(1ms);

        // Change this value to control how much gets written.
        int render = samples / 2;

        mix_sine(mixbuffer, render, samplerate, 2, t);
        t += render;

        for(int i = 0; i < render*2; i++)
            outbuffer[i] = round(mixbuffer[i]*0x7FFF);

        perr("writing %d frames...\n", render);
        auto written = snd_pcm_writei(device, outbuffer, render);
        perr("done.\n");

        while(written == -EPIPE or written == -ESTRPIPE)
        {
            snd_pcm_recover(device, written, 0);
            written = snd_pcm_writei(device, outbuffer, render);
        }
    }

    snd_pcm_close(device);
    return 0;
}
