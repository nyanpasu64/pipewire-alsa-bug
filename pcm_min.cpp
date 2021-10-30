/*
 *  This extra small demo sends a random samples to your speakers.
 */

#include <alsa/asoundlib.h>

#include <memory>
#include <mutex>
#include <optional>

template<typename Ptr>
class [[nodiscard]] Guard {
    std::unique_lock<std::mutex> _guard;
    Ptr _value;

private:
    /// Don't lock std::unique_lock; the factory methods will lock it.
    Guard(std::mutex & mutex, Ptr value) :
        _guard{mutex, std::defer_lock_t{}}, _value{value}
    {}

public:
    static Guard make(std::mutex & mutex, Ptr value) {
        Guard guard{mutex, value};
        guard._guard.lock();
        return guard;
    }

    static std::optional<Guard> try_make(std::mutex & mutex, Ptr value) {
        Guard guard{mutex, value};
        if (guard._guard.try_lock()) {
            return guard;
        } else {
            return {};
        }
    }

    explicit operator bool() const noexcept {
        return _guard.operator bool();
    }

    typename std::pointer_traits<Ptr>::element_type & operator*() {
        return *_value;
    }

    Ptr operator->() {
        return _value;
    }
};

template<typename T>
class Mutex {
    mutable std::mutex _mutex;
    T _value;

public:
    explicit Mutex(T value) : _value(std::move(value)) {}

    using Ptr = T *;

    using Guard = Guard<Ptr>;

    /// Only call this in the GUI thread.
    Guard lock() {
        return Guard::make(_mutex, &_value);
    }
};

static char const *device = "default";            /* playback device */
unsigned char buffer[16*1024];              /* some random data */

int main(void)
{
    int err;
    unsigned int i;
    snd_pcm_t *handle;
    snd_pcm_sframes_t frames;

    for (i = 0; i < sizeof(buffer); i++)
        buffer[i] = random() & 0xff;

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

    while (1) {
        frames = snd_pcm_writei(handle, buffer, sizeof(buffer));
        if (frames < 0)
            frames = snd_pcm_recover(handle, frames, 0);
        if (frames < 0) {
            printf("snd_pcm_writei failed: %s\n", snd_strerror(frames));
            break;
        }
        if (frames > 0 && frames < (long)sizeof(buffer))
            printf("Short write (expected %li, wrote %li)\n", (long)sizeof(buffer), frames);

        snd_pcm_drain(handle);
        snd_pcm_state(handle);
        snd_pcm_prepare(handle);
    }

    /* pass the remaining samples, otherwise they're dropped in close */
    err = snd_pcm_drain(handle);
    if (err < 0)
        printf("snd_pcm_drain failed: %s\n", snd_strerror(err));
    snd_pcm_close(handle);
    return 0;
}
