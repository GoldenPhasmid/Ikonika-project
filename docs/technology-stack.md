# Technology stack

| Layer | Choice | Acquisition |
| --- | --- | --- |
| Language | C++20 | — |
| Build | CMake, Visual Studio generator (MSVC primary) | — |
| UI | Qt 6: Quick, Quick Controls 2 | Official Qt installer |
| Storage | SQLite via `QSqlDatabase`/`QSqlQuery` (`QSQLITE` driver) | Bundled with Qt |
| Audio playback | miniaudio | - |
| Speech | sherpa-onnx with Piper on desktop; the platform's own TTS on Android; Supertonic kept for dev | `tools\fetch-native.ps1` |
| Machine translation | OPUS-MT int8 through ONNX Runtime on desktop; Google ML Kit on Android | `tools\fetch-native.ps1` on desktop |
| Tests | Catch2 v3 | CMake FetchContent |
| Dictionary import | `scripts/dictionary/build_dictionary.py` | Build-time tool |

## Qt 6, Qt Quick

C++20, QML. Qt Quick Controls 2 supplies ready-made controls. 
Module breakdown:
* core - core layer
* app - app layer
* tests - test layer

## Storage

`FTDatabase`/`FTStatement` (in `core`) wrap `QSqlDatabase`/`QSqlQuery` rather than SQLite's raw C
API. Two things this isn't free: neither of SQLite's two hardening flags the project used to
compile in (`SQLITE_DEFAULT_FOREIGN_KEYS`, `SQLITE_DQS=0`) apply to Qt's bundled SQLite, so
`FTDatabase::Open` restores both per connection — foreign keys via `PRAGMA foreign_keys = ON`,
double-quoted-string rejection via `sqlite3_db_config(..., SQLITE_DBCONFIG_DQS_DDL/DML, 0, ...)`
reached through `QSqlDriver::handle()`. And `QSqlQuery::exec()` runs exactly one statement, unlike
`sqlite3_exec()`, so `Execute()` splits on `;` itself.

`core` links `Qt6::Sql`, so it is not Qt-free — it already linked `Qt6::Core` for `QString`,
`QSettings` and `QLoggingCategory`, so this doesn't cross a line that wasn't already crossed.
`CoreTests` needs an actual `QCoreApplication` instance (`tests/TestMain.cpp`), because
`QSqlDatabase` loads its driver as a Qt plugin and plugin loading needs one; the plugin itself
(`qsqlite(d).dll`) is copied next to `CoreTests.exe` by `tests/CMakeLists.txt`, since Qt only finds
plugins relative to the running executable.

FTS5 is not currently wired up. Revisit if free-text dictionary search becomes a real requirement.

## miniaudio

Single-header C library for sound playback, public domain. Chosen over Qt Multimedia: reliable
low-latency playback of many short clips, and no Qt dependency, so audio logic lives in `core`.

**The device opens at whatever rate the OS negotiates; a resampler matches each clip to it.** This
broke once, silently pitch-shifting everything, when a voice's own rate didn't match what a
particular audio device opened at — the Android emulator negotiates 48000 Hz against Piper's
22050. `Play()` still refuses a call whose sample rate argument doesn't match the voice it was
rendered at, since that is a caller-contract bug rather than a hardware mismatch.

Wiktionary recordings are `.ogg`/`.mp3`; miniaudio decodes mp3 natively but needs an extra file for
Ogg Vorbis, so the importer takes the mp3 set.

## Speech

One shipped engine per platform, decided by [docs/voice.md](docs/voice.md)'s measurements.

Desktop: neural synthesis, fully offline, via sherpa-onnx's **C API**, loaded at start-up.
**Piper `fr_FR-siwis-medium`** — VITS, eSpeak phonemes, 22050 Hz, one speaker, French-only,
Apache-2.0. Phonemes come from French rules rather than being inferred, so it can't mispronounce an
unfamiliar word. MinGW links the DLL directly, no import library needed. MSVC needs the `.lib`
import library the same official Python wheel also ships (`fetch-native.ps1` grabs both). Libraries
and the ~77 MB model are not in git; `fetch-native.ps1` fetches them.

**sherpa-onnx is pinned at 1.13.8.** Releases before 1.13.5 leave out the pad token Piper expects
right after the start marker, and Piper then loses the first sound of every line: salé came out as
"alé", chasser as "tasser". A sentence hides one lost sound, a lone word does not. Upstream fixed it
in k2-fsa/sherpa-onnx#3770. The 2.x line removed the espeak-ng front end Piper reads text through,
so moving past 1.13.x means phonemising outside sherpa-onnx first. Its wheel ships onnxruntime
1.28.2, which sets the version of the onnxruntime C API headers.

Android: the device's own `android.speech.tts.TextToSpeech`, called through JNI. Roughly 10x faster
than any bundled ONNX model once warm, at the cost of depending on whatever engine and voices the
device has installed — no bundled fallback today. The APK carries neither sherpa-onnx nor ONNX
Runtime: core builds without its voice engine there, which keeps about 25 MB of arm64 libraries out
of every install.

**No longer offered to users:** Supertonic — paid a near-fixed ~500 ms per call regardless of length,
both platforms (see voice.md). `FTSpeechSynthesizer` still detects and loads a Supertonic directory;
`tools\fetch-native.ps1` still fetches it for local comparison. It just isn't fetched into a real
install, bundled, or served by the catalog any more.

**Rejected outright:** eSpeak (formant synthesis, unpleasant, nothing fixed it), pre-recorded audio
alone (covers single words only, not arbitrary sentences), cloud synthesis (better quality, but a
daily tool needing a connection to speak is worse than one that's slightly synthetic), Kokoro-82M
(its French is a single B− voice on under eleven hours of data).

eSpeak's remaining role: Piper phonemises through `espeak-ng-data` bundled in its own model
package, driven by sherpa-onnx — no direct dependency from this project. There is no local way to
generate IPA for a hand-typed card; it arrives from the dictionary import only.

## Machine translation

One engine per platform, decided by [docs/machine-translation.md](machine-translation.md)'s
benchmark. Desktop runs Helsinki-NLP OPUS-MT, the int8 ONNX export from onnx-community, through
the ONNX Runtime it already ships for speech. One model per language pair, 229 MB each. Android
runs Google ML Kit on-device translation, 43 MB per language, since the APK carries no ONNX Runtime.

## Dictionary screen

Search the web opens the chosen site, Reverso or Google Translate, via `QDesktopServices::openUrl`
— the system default browser, not a view inside the app. No WebEngine dependency. A plain link
keeps it inside Reverso's terms, which forbid embedding their service in another product. The sites
and their addresses come from the catalog, see [docs/releases.md](docs/releases.md).

## Catch2 v3

Closest UE equivalent: `IMPLEMENT_SIMPLE_AUTOMATION_TEST`. Chosen over GoogleTest — nothing here
needs GoogleMock, since scoring/scheduling are pure functions with no interfaces to fake, and
`REQUIRE(Score == 27)` prints both sides on failure without picking a macro. One FetchContent
block, no separate mocking library.

## Dependencies

**Vendoring** (SQLite amalgamation, miniaudio): source copied into the repo, compiled as project
source, like UE's `ThirdParty`. **CMake FetchContent** (Catch2): downloaded at configure time.
**vcpkg**: not used, specifically not for Qt — use the official installer instead.

## Dictionary

See [docs/dictionary-research.md](docs/dictionary-research.md) for the full source evaluation (wiktextract, DBnary,
WordNet, FreeDict, Tatoeba, and why each was picked or rejected) and
[docs/dictionary-schema.md](docs/dictionary-schema.md) for the schema. Current direction: live
lookup (Reverso, via the Dictionary screen) rather than a shipped local database — matches the
actual usage pattern of looking up one unknown word at a time while reading, and sidesteps most of
the quality problems a bulk-imported open dataset has for this purpose.

### Sentences

The sentence exercises use Tatoeba sentence pairs. Tatoeba text is CC BY 2.0 FR, with a part of it
CC0. The licence allows shipping the sentences inside the dictionary, commercial use included, and
asks for attribution, which the app gives under Settings, About. Tatoeba's audio is licensed per
contributor and is not used: the app speaks sentences with its own voice.

**spaCy** (`fr_core_news_md` and `es_core_news_md`, parser and NER disabled) reads each sentence at build time and gives
every token a lemma and a part of speech. The library is MIT licensed. It runs only in the
dictionary pipeline, so nothing of it ships in the app. Stanza was the alternative, more accurate
and slower; spaCy was enough once its lemmas were checked against the dictionary's own forms and the
translation, which overrides it on 110,208 of 2,137,474 French tokens.

spaCy is not used for verb tenses. Its tagger labels "avais", "serais" and "chanta" all as
`Tense=Pres`, so the grammar level of a sentence is read from the dictionary's `word_forms` table.
See [dictionary-pipeline.md](dictionary-pipeline.md), step 7.

**The index is a set-trie**, a prefix tree over sets. The question it answers is which stored word
sets are subsets of the learner's set. Each sentence is its set of word ids sorted rarest first, and
the walk follows a child only when the learner knows its word, so it never enters a branch holding
an unknown word. Putting the rarest word first makes most sentences fail at the first level, at the
cost of fewer shared prefixes: 1,793,704 nodes for 2,052,710 path words in the French tree.

The walk costs one bit test per child of every visited node, so the time grows with the learner's
vocabulary and not with the number of sentences. A test compares the walk with a brute-force subset
check over every path for three vocabulary sizes, and prints the timings.

Rejected alternatives:

- A table of sentence-to-word rows queried with `GROUP BY ... HAVING COUNT(*) = word_count`: about
  2 million rows in the French dictionary and one query touching every sentence of every known word.
- A per-learner "words still missing" counter per sentence, decremented as words are learned: exact
  and cheap to query, but a large writable set stored with user data and tied to one dictionary
  build.
- A Bloom-style bit signature per sentence: only a prefilter, every hit still needs an exact check.

## Architecture

`core` is a static library with no UI dependency (it links `Qt6::Core`/`Qt6::Sql`, not
Quick/Widgets), so it builds and tests without a window. It holds the database schema and access,
the exercise engine, scoring, the scheduler, dictionary lookup, and audio resolution. The Qt Quick
application is a shell around it.

## Card structure

A **Card** is one thing being learned: word, translation, definition, plus part of speech,
phonetic transcription, and an audio reference. One Leitner box per card regardless of how many
exercises or decks reference it — an earlier two-level Note/Card split (separate review state per
exercise) was dropped because it complicated every scheduling rule for a benefit nobody had felt.

## Settings

Including the Leitner intervals (`HKCU\Software\Ikonika` on Windows). Consequence: settings
don't sync — `QSettings` has no per-key timestamp, so a second machine would start from the
default ladder. Chosen deliberately over splitting settings across two stores; if sync needs
settings later, move those specific keys into a table rather than teaching `QSettings` to sync.
Anything `core` needs from settings is passed in, since `core` has no UI dependency to read them
with directly.

## Schema decisions

Kept cheap now, in the trainer database (see
[docs/trainer-schema.md](docs/trainer-schema.md)), so cloud sync stays possible later without a
breaking migration:

1. **Random UUIDs, not autoincrement.** Two devices issuing sequential IDs independently would
   collide on sync; random IDs don't.
2. **`LastUpdatedDate` on every row.** Sync reduces to "what changed since we last spoke" instead
   of comparing the whole database every time.
3. **Never hard-delete — `Deleted` flag instead (a tombstone).** A hard delete gives another
   device nothing to distinguish "deleted here" from "never existed there"; it would just copy the
   row back.

## Licence reference

| Licence | Practical meaning |
| --- | --- |
| MIT, Apache-2.0, public domain | Use freely, including closed commercial software. Keep the notice. |
| LGPL | Application source may stay closed if the user could substitute their own build of the library. Link dynamically. |
| GPL | Linking it requires the whole application to be open source under GPL. |
| CC BY-SA | Content licence, not code. Credit the source. |

Qt is LGPLv3, except Qt Network Authorization, which is GPLv3 or commercial. The desktop sign-in
links it, so the app is released under the GPLv3; see [ios.md](ios.md#licences). Sherpa-onnx is Apache-2.0, the Supertonic model is MIT, SQLite is public domain,
miniaudio is public domain, Catch2 is BSL-1.0, the Inter typeface is SIL OFL 1.1 and ships with its licence beside the font files. Piper relicensed to GPL-3.0 in late 2025, but its models are plain ONNX files
sherpa-onnx loads without Piper's code — the GPL doesn't reach them. The OPUS-MT models are
CC-BY 4.0. ML Kit translation is free but requires Google's attribution beside each result.
