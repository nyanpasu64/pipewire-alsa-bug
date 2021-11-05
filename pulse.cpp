#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>

#include <pulse/simple.h>
#include <pulse/error.h>

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
static constexpr uint32_t RATE = 44100;

struct Shared {
    pa_simple *s;
};

static void thread_fn(Mutex<Shared> & mutex) {
    using namespace std::chrono_literals;

    int error;

    for (uint64_t t = 0;;) {
        // If you don't sleep, the main thread *never* gets to acquire the mutex.
        std::this_thread::sleep_for(5ms);

        auto guard = mutex.lock();
        auto s = guard->s;

        mix_sine(mixbuffer, BUFSIZE, RATE, NCHAN, t);
        t += BUFSIZE;

        for(size_t i = 0; i < BUFSIZE * NCHAN; i++)
            outbuffer[i] = round(mixbuffer[i]*0x7FFF);

        /* ... and play it */
        perr("writing...\n");
        if (pa_simple_write(s, outbuffer, BUFSIZE * NCHAN * sizeof(int16_t), &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
            exit(1);
        }
        perr("done.\n");
    }
}

int main(int argc, char*argv[]) {
    using namespace std::chrono_literals;

    /* The Sample format to use */
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16NE,
        .rate = RATE,
        .channels = 2
    };

    pa_simple *s = NULL;
    int ret = 1;
    int error;

    /* Create a new playback stream */
    if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        exit(1);
    }

    auto mutex = Mutex(Shared { .s = s });
    s = nullptr;

    auto thread = std::thread(thread_fn, std::ref(mutex));

    std::string cmd;
    while (true) {
        std::getline(std::cin, cmd);
        std::cerr << "Draining...\n";
        auto guard = mutex.lock();
        std::cerr << "Mutex locked.\n";

        auto s = guard->s;
        pa_simple_drain(s, &error);
        if (error < 0) {
            perr("uhh %s\n", pa_strerror(error));
        }
        if (cmd == "delay") {
            std::this_thread::sleep_for(1s);
        }
    }
}
