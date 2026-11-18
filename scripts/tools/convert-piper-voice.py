#!/usr/bin/env python3
"""Turns a Piper voice into a directory sherpa-onnx loads as a VITS model.

Piper publishes a voice as an .onnx file and an .onnx.json config. sherpa-onnx needs three more
things: metadata inside the .onnx that names the model family and its espeak voice, a tokens.txt
built from the config's phoneme map, and espeak-ng-data for phonemisation.

The model is renamed to Piper's <locale>-<name>-<quality>.onnx convention, which the app reads to
name the voice in Settings.

Usage:
  convert-piper-voice.py <model.onnx> <espeak-ng-data> <output directory> <locale> <name> <quality>

Example:
  convert-piper-voice.py tuga-tts-train-synthetic-voice3.onnx local/models/piper-es/espeak-ng-data
      local/models/piper-pt-voice3 pt_PT voice3 medium

Needs the onnx package: pip install onnx
"""

import json
import shutil
import sys
from pathlib import Path

import onnx

LANGUAGE_NAMES = {
    "de": "German",
    "en": "English",
    "es": "Spanish",
    "fr": "French",
    "it": "Italian",
    "pt": "Portuguese",
}


def main():
    if len(sys.argv) != 7:
        print(__doc__)
        sys.exit(1)

    source_model = Path(sys.argv[1])
    espeak_data = Path(sys.argv[2])
    output = Path(sys.argv[3])
    locale, name, quality = sys.argv[4], sys.argv[5], sys.argv[6]

    config = json.loads(Path(f"{source_model}.json").read_text(encoding="utf-8"))
    language = locale.split("_")[0]

    if language not in LANGUAGE_NAMES:
        sys.exit(f"No language name for {language}")

    if not (espeak_data / f"{language}_dict").exists():
        sys.exit(f"{espeak_data} has no {language}_dict")

    output.mkdir(parents=True, exist_ok=True)
    stem = f"{locale}-{name}-{quality}"

    model = onnx.load(str(source_model))
    existing = {entry.key for entry in model.metadata_props}
    metadata = {
        "model_type": "vits",
        "comment": "piper",
        "language": LANGUAGE_NAMES[language],
        "voice": config["espeak"]["voice"],
        "has_espeak": 1,
        "n_speakers": config["num_speakers"],
        "sample_rate": config["audio"]["sample_rate"],
    }

    for key, value in metadata.items():
        if key in existing:
            continue

        entry = model.metadata_props.add()
        entry.key = key
        entry.value = str(value)

    onnx.save(model, str(output / f"{stem}.onnx"))

    config["language"] = {"code": locale}
    (output / f"{stem}.onnx.json").write_text(json.dumps(config, ensure_ascii=False, indent=2), encoding="utf-8")

    with open(output / "tokens.txt", "w", encoding="utf-8", newline="\n") as tokens:
        for symbol, ids in config["phoneme_id_map"].items():
            tokens.write(f"{symbol} {ids[0]}\n")

    target_espeak = output / "espeak-ng-data"

    if not target_espeak.exists():
        shutil.copytree(espeak_data, target_espeak)

    print(f"Wrote {output / stem}.onnx, metadata {metadata}")


if __name__ == "__main__":
    main()
