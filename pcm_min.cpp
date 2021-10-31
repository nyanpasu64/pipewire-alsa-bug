#include <alsa/asoundlib.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

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

    using GuardT = Guard<T *>;

    /// Only call this in the GUI thread.
    GuardT lock() {
        return GuardT::make(_mutex, &_value);
    }
};

static char const *device = "default";            /* playback device */
unsigned char buffer[16*1024];              /* some random data */

int drain = 0;

static void thread_fn(Mutex<snd_pcm_t *> & mutex) {
    using namespace std::chrono_literals;

    {
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

        auto guard = mutex.lock();
        *guard = handle;
    }

    snd_pcm_sframes_t frames;

    while (1) {
        // If you don't sleep, the main thread *never* gets to acquire the mutex.
        std::this_thread::sleep_for(5ms);

        auto guard = mutex.lock();
        auto handle = *guard;

        snd_pcm_state_t state = snd_pcm_state(handle);
        std::cerr << "state " << state << "\n";
//        if (state != SND_PCM_STATE_PREPARED && state != SND_PCM_STATE_RUNNING) {
        if (drain) {
            drain--;
//            drain = false;
            snd_pcm_drain(handle);
            std::cerr << "new state " << snd_pcm_state(handle) << "\n";
            snd_pcm_prepare(handle);
        }

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

static void main_fn(Mutex<snd_pcm_t *> & mutex) {
    while (true) {
        std::cin.get();
        std::cerr << "Draining...\n";
        auto guard = mutex.lock();
        std::cerr << "Mutex locked.\n";

        auto handle = *guard;
        drain = 1;
    }
}

int main(void)
{
    for (size_t i = 0; i < sizeof(buffer); i++)
        buffer[i] = random() & 0xff;

    auto mutex = Mutex((snd_pcm_t *) nullptr);
    auto thread = std::thread(thread_fn, std::ref(mutex));
    main_fn(mutex);
}
