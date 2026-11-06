"""Synthesises the app's sound effects into app/android/res/raw.

    python scripts/tools/make-sound-effects.py

Each effect is a short run of sine notes with a soft attack and an exponential decay, written as
16-bit mono WAV. The files are committed, so this only needs running when a sound changes.
"""

import math
import struct
import wave
from pathlib import Path

SAMPLE_RATE = 44100
OUTPUT_DIRECTORY = Path(__file__).resolve().parents[2] / "app" / "android" / "res" / "raw"

EFFECTS = {
    "sfx_incorrect": {"peak": 0.30, "notes": [(165.0, 110.0, 0.14, 0.045)]},
    "sfx_complete": {"peak": 0.26, "notes": [(659.25, 659.25, 0.11, 0.07), (880.0, 880.0, 0.26, 0.09)]},
    "sfx_goal": {
        "peak": 0.26,
        "notes": [(523.25, 523.25, 0.10, 0.07), (659.25, 659.25, 0.10, 0.07), (783.99, 783.99, 0.34, 0.11)],
    },
}


def render_note(start_frequency, end_frequency, duration, decay):
    frame_count = int(duration * SAMPLE_RATE)
    attack_frames = int(0.004 * SAMPLE_RATE)
    samples = []
    phase = 0.0

    for frame in range(frame_count):
        progress = frame / frame_count
        frequency = start_frequency + (end_frequency - start_frequency) * progress
        phase += 2.0 * math.pi * frequency / SAMPLE_RATE

        attack = min(1.0, frame / attack_frames)
        envelope = attack * math.exp(-(frame / SAMPLE_RATE) / decay)
        tone = math.sin(phase) + 0.18 * math.sin(2.0 * phase)

        samples.append(tone * envelope)

    return samples


def render_effect(effect):
    samples = []

    for note in effect["notes"]:
        samples.extend(render_note(*note))

    fade_frames = int(0.01 * SAMPLE_RATE)

    for index in range(fade_frames):
        samples[-1 - index] *= index / fade_frames

    loudest = max(abs(sample) for sample in samples)

    return [sample / loudest * effect["peak"] for sample in samples]


def write_wave(path, samples):
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(b"".join(struct.pack("<h", int(sample * 32767)) for sample in samples))


def main():
    OUTPUT_DIRECTORY.mkdir(parents=True, exist_ok=True)

    for name, effect in EFFECTS.items():
        samples = render_effect(effect)
        path = OUTPUT_DIRECTORY / f"{name}.wav"

        write_wave(path, samples)
        print(f"{path.name}: {len(samples) / SAMPLE_RATE * 1000:.0f} ms")


if __name__ == "__main__":
    main()
