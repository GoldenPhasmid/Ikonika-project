#!/usr/bin/env python3
"""
One-time pre-filter: pulls only the lines for one lang_code out of a wiktextract extract into
a smaller file, so downstream steps read only that language's subset instead of the full
multi-language extract every run.

fr-extract.jsonl documents every language Wiktionnaire happens to mention on a page with a
matching spelling, not only French, and raw-wiktextract-data.jsonl does the same for every
language English Wiktionary mentions, so the majority of either file is discarded on every
import run regardless of which words are targeted. Extracting the per-language subset once
removes that repeated cost.

A cheap substring check ('"lang_code": "<lang>"' in the raw line) runs before the real JSON
parse, skipping full parsing on other-language lines almost for free; only lines that pass both
the substring check and a real json.loads() with the matching lang_code are kept, since the
substring check alone can theoretically false-positive (a line about a different language could
still contain that exact substring somewhere in a translation entry, say).

Usage:
    python extract_language_lines.py <extract.jsonl[.gz]> <lang-code> <lang-only.jsonl>
"""

import argparse
import gzip
import json
import sys
import time


def open_maybe_gzip(path, mode="rt"):
    if path.endswith(".gz"):
        return gzip.open(path, mode, encoding="utf-8" if "t" in mode else None)
    return open(path, mode, encoding="utf-8" if "t" in mode else None)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", help="wiktextract extract (.jsonl or .jsonl.gz)")
    parser.add_argument("lang", help="lang_code to keep, e.g. fr or en")
    parser.add_argument("output", help="per-language output .jsonl")
    args = parser.parse_args()

    start_time = time.monotonic()
    progress_stride = 500_000

    needle = f'"lang_code": "{args.lang}"'

    lines_read = 0
    lines_written = 0

    with open_maybe_gzip(args.input, "rt") as input_handle, \
         open(args.output, "wt", encoding="utf-8") as output_handle:

        for line in input_handle:
            lines_read += 1

            if lines_read % progress_stride == 0:
                elapsed = time.monotonic() - start_time
                rate = lines_read / elapsed if elapsed > 0 else 0.0
                print(f"  ...{lines_read:,} lines read ({rate:,.0f}/s), "
                      f"{lines_written:,} written, {elapsed:,.1f}s elapsed", file=sys.stderr)

            if needle not in line:
                continue

            stripped = line.strip()
            if not stripped:
                continue

            try:
                record = json.loads(stripped)
            except json.JSONDecodeError:
                continue

            if record.get("lang_code") != args.lang:
                continue

            output_handle.write(line if line.endswith("\n") else line + "\n")
            lines_written += 1

    elapsed = time.monotonic() - start_time

    print(f"elapsed: {elapsed:,.1f}s")
    print(f"lines read: {lines_read:,}")
    print(f"lang_code {args.lang!r} lines written: {lines_written:,}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
