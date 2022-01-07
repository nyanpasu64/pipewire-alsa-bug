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
constexpr size_t CHANNELS = 2;

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
        perr("%s failed with error code %d\n", #__VA_ARGS__, _v); \
        exit(1); \
    } \
}

#define MUT
#define OUT

int main()
{
    using namespace std::chrono_literals;

    snd_pcm_t * device;
    TRY(snd_pcm_open(&device, "front:CARD=Generic,DEV=0", SND_PCM_STREAM_PLAYBACK, 0))
    // or "front:CARD=Audio,DEV=0"

    unsigned int MUT samplerate = 48000;
    unsigned int const channels = 2;
    snd_pcm_uframes_t MUT period;
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

        period = 144;
        int dir = 0;
        perr("period size: %lu", period);
        TRY(snd_pcm_hw_params_set_period_size_near(device, parameters, &period, &dir));
        perr(" -> %lu (moved %d)\n", period, dir);

        samples = 256;
        perr("buffer size: %lu", samples);
        TRY(snd_pcm_hw_params_set_buffer_size_near(device, parameters, &samples))
        perr(" -> %lu\n", samples);

        TRY(snd_pcm_hw_params(device, parameters))
        snd_pcm_hw_params_free(parameters);
    }

    snd_pcm_get_params(device, &samples, &period);
    perr("\nperiod size: %lu, sample count: %lu\n", period, samples);

    {
        snd_pcm_sw_params_t * param;
        TRY(snd_pcm_sw_params_malloc(&param));

        snd_pcm_sw_params_current(device, OUT param);

        snd_pcm_uframes_t boundary;
        snd_pcm_sw_params_get_boundary(param, &OUT boundary);
        perr("boundary: %lu\n", boundary);

        // Never stop playing even upon xrun.
        TRY(snd_pcm_sw_params_set_stop_threshold(device, param, boundary));

        TRY(snd_pcm_sw_params(device, param));
        snd_pcm_sw_params_free(param);
    }

    if (snd_pcm_prepare(device) < 0)
        return puts("Failed to prepare device"), 0;

    uint64_t t = 0;
    while(1)
    {
        std::this_thread::sleep_for(500ms);

        perr("\nsnd_pcm_avail=%ld\n", snd_pcm_avail(device));

        // Change this value to control how much gets written.
        uint64_t render = period;

        mix_sine(mixbuffer, render, samplerate, CHANNELS, t);
        t += render;

        for(uint64_t i = 0; i < render*CHANNELS; i++)
            outbuffer[i] = (int16_t) round(mixbuffer[i]*0x7FFF);

        perr("writing %lu frames...\n", render);
        auto written = snd_pcm_writei(device, outbuffer, render);
        perr("done.\n");

        while(written == -EPIPE or written == -ESTRPIPE)
        {
            perr("error %ld\n", written);
            snd_pcm_recover(device, (int) written, 0);
            written = snd_pcm_writei(device, outbuffer, render);
        }
    }

    snd_pcm_close(device);
    return 0;
}
