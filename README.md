# pipewire-alsa is bugged

This is a CMake project with two executables, alsa and pcm_min (corresponding to alsa.cpp and pcm_min.cpp).

## alsa.cpp

On the FiiO E10K, PipeWire hangs and plays no audio when the server is running at a quantum below 256 frames/samples. The pipewire server enters a loop of printing `spa.alsa     | [      alsa-pcm.c: 1245 get_status()] front:2: snd_pcm_avail after recover: Broken pipe`.

## pcm_min.cpp

This plays back looped noise from an audio thread. Pressing Enter in the console calls `snd_pcm_drain` and `snd_pcm_prepare` on the main thread. This works on pulseaudio-alsa, but causes the audio to stop playing on pipewire-alsa.

If you open a pipewire-alsa pcm device, then call `snd_pcm_drain` and `snd_pcm_prepare` on the GUI thread (synchronized by a mutex), then trying to `snd_pcm_writei` from the audio thread will block forever after a few calls (as soon as the buffer fills up), since the buffer is no longer being read from.

Does ALSA allow you to call `snd_pcm_prepare` on a different thread from `snd_pcm_writei`? If so, pipewire-alsa is at fault for not handling this case properly.

I based this code off BambooTracker. In that program, when you switch documents, it calls `RtAudio::stopStream()` and `RtAudio::startStream()` from the UI thread (and RtAudio in turn calls `snd_pcm_drain` and `snd_pcm_prepare`). When it's configured to use the "default" ALSA device and it points to pipewire-pulse, then switching documents causes the audio thread to hang (and closing the program or switching documents again causes the UI thread to hang as well).

## pcm_min.cpp license

pcm_min.cpp is based off alsa-lib's pcm_min.c, which is released under the LGPL.

## alsa.cpp license

You can use this code as though it were public domain. Released under the Creative Commons "Zero" license 1.0, a public domain attribution: https://creativecommons.org/publicdomain/zero/1.0/

The CC0 license is an accepted Free Software license: https://www.gnu.org/licenses/license-list.html#CC0
