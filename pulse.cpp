#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>

#include <pulse/simple.h>
#include <pulse/error.h>

constexpr size_t BUFSIZE = 1024;
constexpr size_t NCHAN = 2;

double freq = 256;
double volume = 0.2;

float mixbuffer[BUFSIZE * NCHAN];
int16_t outbuffer[BUFSIZE * NCHAN];
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

int main(int argc, char*argv[]) {

    uint32_t rate = 44100;

    /* The Sample format to use */
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16NE,
        .rate = rate,
        .channels = 2
    };

    pa_simple *s = NULL;
    int ret = 1;
    int error;

    /* Create a new playback stream */
    if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        goto finish;
    }

    for (uint64_t t = 0;;) {
        mix_sine(mixbuffer, BUFSIZE, rate, NCHAN, t);
        t += BUFSIZE;

        for(size_t i = 0; i < BUFSIZE * NCHAN; i++)
            outbuffer[i] = round(mixbuffer[i]*0x7FFF);

        /* ... and play it */
        perr("writing...\n");
        if (pa_simple_write(s, outbuffer, BUFSIZE * NCHAN * sizeof(int16_t), &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
            goto finish;
        }
        perr("done.\n");
    }

    /* Make sure that every single sample was played */
    if (pa_simple_drain(s, &error) < 0) {
        fprintf(stderr, __FILE__": pa_simple_drain() failed: %s\n", pa_strerror(error));
        goto finish;
    }

    ret = 0;

finish:

    if (s)
        pa_simple_free(s);

    return ret;
}
