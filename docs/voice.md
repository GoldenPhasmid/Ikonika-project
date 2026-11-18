# Voice performance

Speech synthesis has one job in this app: play a word or sentence back fast enough that the delay after tapping speak does not register as a wait. This doc records what each candidate engine actually costs, on desktop and on Android, and which one plays audio on each platform today.

## Historical INT8 measurements

| Engine | Platform | word_short (4 ch) | word_medium (8 ch) | sentence_short (13 ch) | sentence_long (51 ch) |
|---|---|---|---|---|---|
| Piper | PC | 18 ms | 34 ms | 35 ms | 105 ms |
| Piper | Android | ~100–150 ms | ~150–200 ms | ~150–250 ms | ~250–350 ms |
| Supertonic | PC | 235 ms | 229 ms | 240 ms | 469 ms |
| Supertonic | Android | ~500 ms | ~500 ms | ~500 ms | ~500 ms |
| Android platform TTS, warm | Android | ~30–60 ms | ~30–60 ms | ~30–60 ms | ~30–60 ms |
| Android platform TTS, cold | Android | ~670 ms, first call only | — | — | — |

PC numbers come from a fresh run of the desktop scratch benchmark. Android numbers are ranges from the on-device investigation described below, not a per-length rerun.

Desktop uses Supertonic 3 with the original ONNX weights. Listening comparisons found background noise in the INT8 output that disappeared with the original weights. The larger model is accepted for desktop speech quality. Supertonic is the default voice for every language. A Piper voice published for one language is an optional download, offered in Settings. Android uses platform TTS and downloads no voice model.

Portuguese has one language tag in Supertonic 3. The model has no switch between Brazilian and European Portuguese, and its voice presets carry no accent. Supertone said in its issue tracker that the Portuguese training data may lean European, while Brazilian users hear a mix of both. The repository was archived in September 2026, so a regional control will not come. Android and iOS ask the platform engine for a European Portuguese voice explicitly, because a bare Portuguese request resolves to Brazilian. Android falls back to any offline Portuguese voice when no European one is installed.

Memory cost of the original weights against INT8, from standalone CPU processes generating the same French sentence with Robert and Sarah at speed 1.0, 8 steps and seed 42. The figures include Python and ONNX Runtime and depend on text length.

| | INT8 | Original |
|---|---|---|
| Four model files | 137.8 MiB | 379.6 MiB |
| Peak resident RAM | 339.1 MiB | 542.1 MiB |
| Resident RAM after synthesis | 323.2 MiB | 526.2 MiB |
| Private committed memory | 396.0 MiB | 552.4 MiB |

The historical INT8 table records the earlier investigation. A later Debug run with original weights and two synthesis threads measured approximately 321, 323, 333 and 662 ms for the four desktop text lengths.

## Engines investigated

| Engine | Type | Where it runs | Offered to users | Status |
|---|---|---|---|---|
| Piper | VITS, ONNX | Desktop, via sherpa-onnx | Optional, chosen as the text to speech model for one language | European Portuguese `voice3` published |
| Android platform TTS | `android.speech.tts.TextToSpeech`, `com.google.android.tts` engine | Android only | No — uses whatever the device has installed | Shipping, Android's only voice |
| Supertonic | Flow-matching, ~99M params, original ONNX | Desktop, via sherpa-onnx; measured on Android during this investigation | Desktop only, downloaded at setup | Desktop default; not offered on Android |

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

### Android engine recovery

On 2026-11-11 a phone stayed silent across several app restarts. A speech request waits for the warmup to finish, and the warmup finished only when the engine called back, so an engine that never answered held every request forever. A request the engine rejected outright was dropped without a trace, and the engine's own errors went to logcat only.

`PlatformSpeech.kt` now recovers from three cases: the engine never reports init (10 s), a warmup that produces no result (10 s, held speech is released), and a `speak()` the engine rejects (the request is held and the engine is shut down and bound again). It gives up after 3 rebinds in a row, and any utterance that actually starts resets the count. Every engine event is written to `Ikonika.log` under `ikonika.speech` with the prefix `engine:`, so a silent session now leaves a trail.

Reproduced on the emulator by force-stopping `com.google.android.tts` and then opening a Discover quiz: the first word was rejected with result -1, the engine rebound, and the word played 2.5 s later.

## Synthesis and playback

Supertonic renders with 8 denoising steps and seed 42. The requested speed is clamped to 0.5–1.5, and Supertonic receives it directly only inside 0.8–1.3. Outside that range the model repeats words or produces gibberish, so it renders at the nearest edge of the range and the trimmed audio is time-stretched with the Signalsmith phase vocoder for the rest. A single word at the turtle speed of 0.6 therefore renders at 0.8 and is stretched by another 0.75. The stretched audio can sound robotic, which is accepted. Its generated waveform starts with 230 to 590 ms of silence, 430 ms on average for Sarah and 290 ms for Robert, measured over 16 words. That silence was audible as a pause after tapping speak, so the loader cuts it down to a 20 ms margin before the first sound, the same trim Piper uses. Trailing silence stays. The app applies no volume normalization to Supertonic. It converts samples to the playback PCM format, with range clamping for conversion safety. Device-rate conversion remains part of playback when required by the audio device.

Piper keeps its leading-silence trimming and loudness adjustment in the loader, which only the tests exercise.

Two settings control speech speed. Speech rate applies to words. Sentence speech rate is a multiplier on the speech rate for whole sentences: sentence training, read aloud, and any other spoken text of four or more words, such as Quick Add, dictionary examples and Discover. Text of one to three words plays at the speech rate. Speech rate defaults to 100% and sentence speech rate to 85%. Both run from 50% to 150%, so a sentence plays at speech rate times sentence speech rate, clamped to the same range. At the defaults a sentence plays at 85% of the word speed. Both sliders are on the desktop and mobile sound settings, and the sentence slider plays a sample sentence at the sentence speed. The explicit slow-speech action multiplies its own factor on top of the sentence speed, so a sentence replayed slowly can reach the 0.5 floor. On Android the platform voice receives the same sentence multiplier for sentence training and for read aloud.

The official browser demo adds 0.05 to its displayed speed before synthesis. Its displayed 1.00 therefore corresponds to native speed 1.05. The app passes its selected speed without that offset.

## Voice picker

Desktop Settings → Sound offers all ten Supertonic voices. The speaker IDs match the packaged voice data.

| ID | Name |
| --- | --- |
| 0 | Sarah |
| 1 | Lily |
| 2 | Jessica |
| 3 | Olivia |
| 4 | Emily |
| 5 | Alex |
| 6 | James |
| 7 | Robert |
| 8 | Sam |
| 9 | Daniel |

Sarah is the default when no speaker selection has been saved. Existing saved selections are preserved. Selecting a voice plays a sample.

The voice picker lists Supertonic's speakers only, and shows only while Supertonic speaks the language being learned.

## Text to speech model

Settings → Sound has a Text to speech model row. It lists Supertonic first. After it come the catalog voice packages published for the language being learned, under their catalog `name`, and then any other installed voice for that language. The row appears only when the list has more than one model.

Choosing a model stores it for that language only, in `audio/preferredVoice/<language>`, as the folder the package unpacks into. Every other language keeps Supertonic. Choosing a model that is not installed starts its download, and the row shows the progress. The app loads the voice as soon as it lands and applies the stored choice then. Until it lands, Supertonic keeps speaking. Choosing Supertonic clears the stored choice.

A catalog voice package with a `target` language is optional. First-launch setup and the reader download only the voice package with no target. Supertonic stays the active voice, so a stored active voice name that is not Supertonic falls back to it at load.

## European Portuguese

Supertonic has one Portuguese tag, and its accent leans towards Brazilian. The app's Piper option for European Portuguese is `voice3` from `Phoonnx/pipertts_pt-PT_voice3`, trained on a synthetic dataset. Its repository states no licence. It is published as package `piper-pt-voice3` version 1, release `voice-piper-pt-voice3-1`, 64 MB zipped. Every other European Portuguese Piper voice found is CC BY-NC or CC BY-NC-ND, except the official `tugão`.

`scripts/tools/convert-piper-voice.py` turns a Piper voice into a directory sherpa-onnx loads. It writes the metadata, `tokens.txt` and `espeak-ng-data`, and renames the model to `<locale>-<name>-<quality>.onnx`. The app reads the middle part as the speaker name. To test the download locally, point `update/catalogUrl` at a catalog file whose entry has a `file:///` address to the zip.

## Model files and diagnostics

The fetch script downloads the four original ONNX models and tts.json from Supertone. It obtains the binary voice styles and character index from the Sherpa conversion package. Supertonic loading requires the original model filenames. A directory containing only INT8 weights is skipped so it cannot shadow a valid bundled model.

For a reproducible capture, set IKONIKA_SPEECH_CAPTURE to an existing output directory. Raw and final PCM WAV files are written per speed. IKONIKA_SPEECH_CAPTURE_SEED and IKONIKA_SPEECH_CAPTURE_STEPS override the seed and denoising count only while capture is enabled. Repeated captures at the same speed overwrite those diagnostic files.

The hidden test "Capture Supertonic sentence audio" generates a fixed French sentence at 1.0 and 0.75. The WSOLA stretcher stays only as the fallback when the Signalsmith stretch fails.

## Resampling

The Android emulator's audio device negotiates 48000 Hz. Supertonic renders at 44100 Hz. `FTAudioPlayer` used to force the device open at the voice's own rate; it now opens at whatever the device negotiates, reads the real rate back through miniaudio, and resamples each clip into that rate with `ma_resampler` before playback.

## Trade-off

A bundled model ships with the app and sounds the same on every device. Android's platform engine is roughly 10x faster warm, but depends on whatever TTS engine and voices are actually installed — verified against Google's engine on the emulator, not against arbitrary real devices. No fallback engine exists for a device with none; that is accepted for now rather than solved.

## Open items

- No device-side survey of what `com.google.android.tts` availability looks like on the oldest or cheapest hardware this app targets.
- No bundled fallback for a device whose platform TTS is missing or lacks a matching voice.
