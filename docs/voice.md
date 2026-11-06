# Voice performance

Speech synthesis has one job in this app: play a word or sentence back fast enough that the delay after tapping speak does not register as a wait. This doc records what each candidate engine actually costs, on desktop and on Android, and which one plays audio on each platform today.

## At a glance

| Engine | Platform | word_short (4 ch) | word_medium (8 ch) | sentence_short (13 ch) | sentence_long (51 ch) |
|---|---|---|---|---|---|
| Piper | PC | 18 ms | 34 ms | 35 ms | 105 ms |
| Piper | Android | ~100–150 ms | ~150–200 ms | ~150–250 ms | ~250–350 ms |
| Supertonic | PC | 235 ms | 229 ms | 240 ms | 469 ms |
| Supertonic | Android | ~500 ms | ~500 ms | ~500 ms | ~500 ms |
| Android platform TTS, warm | Android | ~30–60 ms | ~30–60 ms | ~30–60 ms | ~30–60 ms |
| Android platform TTS, cold | Android | ~670 ms, first call only | — | — | — |

PC numbers come from a fresh run of the desktop scratch benchmark. Android numbers are ranges from the on-device investigation described below, not a per-length rerun.

Supertonic pays nearly the same cost per call regardless of length on both platforms — flow-matching runs a fixed number of denoising steps, so length only matters once the text needs an extra pass. Android's own engine beats bundled Supertonic by roughly 10x once warm. **Decision, desktop: Supertonic is the default voice and is downloaded automatically during first-launch setup.** It covers every language the app teaches in one 124 MB package, including Portuguese, where Piper has no voice in the catalog. The cost is about 230 ms per spoken line against 20 to 130 ms for Piper, which is the trade accepted for one voice that works everywhere. Piper stays in the catalog as a per-language alternative. **Decision, Android: unchanged.** The platform TTS is the only voice, with no bundled fallback. `FTSpeechService` picks the voice that covers the most languages when the user has not chosen one, which is Supertonic whenever it is installed.

## Engines investigated

| Engine | Type | Where it runs | Offered to users | Status |
|---|---|---|---|---|
| Piper (`vits-piper-fr_FR-siwis-medium`) | VITS, ONNX | Desktop, via sherpa-onnx | Yes, as a per-language alternative | Shipping, desktop alternative |
| Android platform TTS | `android.speech.tts.TextToSpeech`, `com.google.android.tts` engine | Android only | No — uses whatever the device has installed | Shipping, Android's only voice |
| Supertonic | Flow-matching, ~99M params, int8 ONNX | Desktop, via sherpa-onnx; measured on Android during this investigation | Desktop only, downloaded at setup | Desktop default; not offered on Android |

Windows ships SAPI5 (`System.Speech.Synthesis`) and WinRT (`Windows.Media.SpeechSynthesis`) system voices. Neither is wired into the app.

## Desktop synthesis time

Numbers above come from this machine, 2 synthesis threads, `Debug` build, warm (one throwaway call before timing).

Dropping `FTSpeechSynthesizer`'s thread count from 2 to 1 was tried as a way to cut sync overhead. It did not: a single word round-tripped at 877 ms and 943 ms on one thread against roughly 504 ms on two. `SynthesisThreads` stays at 2, unconditionally, on both platforms.

## Android synthesis and playback time

Numbers above come from an x86_64 emulator (`medium_phone`, `hw.cpu.ncore=4`), against `com.google.android.tts`. Android's own engine is the fastest of the three once warm, by roughly an order of magnitude over Supertonic. On Android, `AppPlatformSpeech` calls it directly through JNI, and the APK carries neither sherpa-onnx nor ONNX Runtime.

### Cold start

The first real call used to pay a ~670 ms tax that never showed up again after. Two bugs caused it, both in `PlatformSpeech.kt`:

1. Warmup exercised the default English voice — the French voice was never selected before the warmup call.
2. Warmup used `synthesizeToFile()`, which never touches the live `AudioTrack` pipeline a real `speak()` call needs, so the pipeline was still cold when the real call arrived. Worse, the real call's `QUEUE_FLUSH` could cancel an in-flight warmup utterance, forcing the real call to pay the warmup's own lazy-init cost.

Fix: select the French voice before warming, warm with a real silent `speak()` call (`KEY_PARAM_VOLUME = 0`) instead of `synthesizeToFile()`, and have `speak()` wait for warmup to finish before issuing its own call. That wait runs off the UI thread via `QThreadPool`, so it never stalls the interface. Measured result: 670 ms → 33 ms, matching the warm steady state.

## Leading silence

Supertonic pads every clip with 200 to 650 ms of silence before the voice starts, and the padding grows with the length of the text. Before the fix, the listener heard that padding on top of the 220 ms synthesis time. Piper pads less, from 0 to 180 ms.

Every clip now has its leading silence cut at synthesis, for both engines. Silence means samples quieter than 0.5% of the clip's peak, which is 46 dB down. The cut keeps 20 ms before the first louder sample, so a soft consonant keeps its onset. Trailing silence is left in place, because a new line stops the one still playing and the tail never delays anything. The loudness gain is measured after the cut, so the removed silence does not pull the clip's level down and push the gain up.

## Sentence rate

A sentence exercise speaks at three quarters of the user's speech speed. At the plain setting a whole
sentence comes out at conversational pace, which a learner hears as rushed, while a single word at
the same setting sounds fine.

The scale travels with the call. `ExerciseControllerBase::SetSpeechRateScale` stores it,
`ExerciseControllerBase::Speak` passes it to `AppSpeech::SpeakAtRate(Text, LanguageCode, RateScale)`,
and `AppSpeech::Speak` is that call with 1.0. `SentenceTrainingController` sets 0.75
(`SentenceSpeechRateScale`) on its three stage controllers; every other caller keeps 1.0.

| Platform | Where the scale is applied |
| --- | --- |
| Desktop | `FTSpeechService::Speak(Text, LanguageCode, RateScale)` synthesises at `clamp(Speed × RateScale, 0.5, 1.5)`. The render speed is part of the cache key, so the slowed line is a separate cache entry from the same text at full speed. |
| Android | `PlatformSpeech.speak(text, languageCode, rateScale)`, JNI signature `(Ljava/lang/String;Ljava/lang/String;F)V`, calls `setSpeechRate(rate × rateScale)` before `speak`, so the next plain call sets the rate back. |

The clamp means a user speed of 0.5 cannot be slowed further.

The slider's 100% is each voice's natural pace. `[speech]` covers the desktop cache behaviour and the
app tests cover the scale reaching `SpeakAtRate`.

## Supertonic pace

Supertonic predicts one length for the whole utterance, not a length per sound. Its speed setting
only stretches that total. Below its natural pace the model gets extra time, and it fills that time
with repeated words and long pauses. A long French sentence at speed 0.85 repeated two of its words.
At 0.68 it held a long pause after the first comma. Supertone confirms that repeats, skipped words and
long silences depend on the text, voice, seed and speed, and that more steps do not reliably fix them.

Supertonic therefore always renders at its own default pace of 1.05, with 8 steps, Supertone's own
default. The seed is fixed, so a line sounds the same on every render and a bad one can be
reproduced. `StretchSpeech` then changes the tempo of the render to `clamp(Speed × RateScale, 0.5, 1.5)`
without changing the pitch. It is WSOLA: 40 ms Hann frames overlapping by half, each taken from
within 12 ms of its nominal position, at the offset whose overlap best matches the previous frame's
continuation. At a tempo of 1.0 the render plays unchanged.

Piper predicts a length for each sound, so it slows cleanly and still takes the speed directly.

The gain is a voice that no longer repeats or stalls at slow speeds. The cost is that slow speech is
now signal processing on fast speech, so a heavy stretch sounds less natural than a slow render
from a model that handles it.

## Resampling

The Android emulator's audio device negotiates 48000 Hz. Supertonic renders at 44100 Hz. `FTAudioPlayer` used to force the device open at the voice's own rate; it now opens at whatever the device negotiates, reads the real rate back through miniaudio, and resamples each clip into that rate with `ma_resampler` before playback.

## Trade-off

A bundled model ships with the app and sounds the same on every device. Android's platform engine is roughly 10x faster warm, but depends on whatever TTS engine and voices are actually installed — verified against Google's engine on the emulator, not against arbitrary real devices. No fallback engine exists for a device with none; that is accepted for now rather than solved.

## Open items

- No device-side survey of what `com.google.android.tts` availability looks like on the oldest or cheapest hardware this app targets.
- No bundled fallback for a device whose platform TTS is missing or lacks a matching voice.
