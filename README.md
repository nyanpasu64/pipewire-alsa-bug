# pipewire-alsa is bugged

This is a CMake project with two executables, alsa and pcm_min (corresponding to alsa.cpp and pcm_min.cpp).

## pulse.cpp -> pulse-cpp

Press Enter to call `pa_simple_drain`. Type `delay` and press Enter to call `pa_simple_drain` and sleep for 1 second.

## alsa.cpp

"alsa" not only fails to play audio unless another app is already playing audio, but wedges pipewire in a status where Firefox can't start playing audio.

alsa.cpp is designed to only call `snd_pcm_writei` based on the available room as measured by `snd_pcm_avail_update`, to avoid blocking. I'm not sure it's a good design. The issue is that on pipewire-alsa, with no other audio streams open, `snd_pcm_avail_update` never increases until you issue a blocking call to `snd_pcm_writei`. And since alsa.cpp is waiting for `snd_pcm_avail_update` to increase before calling `snd_pcm_writei`, it never makes progress. I'm not sure if this is a pipewire-alsa bug, or if alsa.cpp is making too many assumptions about `snd_pcm_avail_update`.

Additionally, launching "alsa" without a stream already playing puts pipewire into a state where all other apps are stalled. In the current state, the program loops on `snd_pcm_avail_update` and stalls pipewire. If you change the program to ignore `snd_pcm_avail_update` and set `render` to a nonzero quantity (regardless if it's 1024, less, or greater), it blocks on `snd_pcm_writei` and stalls pipewire. This is a nasty pipewire/pipewire-alsa bug, possibly caused by the buffer size/count configuration chosen by alsa.cpp.

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
