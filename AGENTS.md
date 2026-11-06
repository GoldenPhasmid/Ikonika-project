# Ikonika — agent guide

Desktop and mobile app for learning foreign vocabulary. Intensive Training + Periodic Review is a primary feature based on Leitner boxes.
Language everything app - discover, add and learn words without using third-party. It requires three things:
* learning scheme, based on daily goals and additional word discovery
* high-quality dictionary, rich in foreign words and native translations.
* document reader, pdf/epub/fb2 with context actions

## Build

Toolchain is **MSVC 2022 64-bit**, Qt 6.11.1 at `C:\Qt\6.11.1\msvc2022_64`

```[[AGENTS]]
.\scripts\tools\fetch-native.ps1             fetch speech libraries and models first (not in git)
.\scripts\tools\build-msvc.ps1               build Debug, configuring on first run
.\scripts\tools\build-msvc.ps1 -Release      build Release instead
.\scripts\tools\build-msvc.ps1 -Configure    force a reconfigure
.\scripts\tools\build-msvc.ps1 -Test         build then run the tests, speech and slow excluded
.\scripts\tools\build-msvc.ps1 -Slow         build then run only the slow tests
.\scripts\tools\build-msvc.ps1 -Speech       build then run only the speech tests
.\scripts\tools\build-msvc.ps1 -Run          build then launch the app
.\scripts\tools\build-msvc.ps1 -Release -Package   produce the installer, ~38 MB
.\scripts\tools\build-msvc.ps1 -Release -Msix      produce the Microsoft Store package
```

Build directories are `build\msvc-debug` and `build\msvc-release`. `-DIKONIKA_BUILD_APP=OFF`
builds `core` and the tests without the app.

Debug configuration uses APP_ROOT/local for any user data (logs, app database, etc.), while release uses AppData.

The package is an Inno Setup installer that goes into the per-user program directory, so it never
asks for administrator rights. It closes a running copy, replaces what it installed, and starts it
again, which is what lets an update apply itself. 

Each foreign language is build from separate independent components that user can choose to download. 
Those are packaged as separate releases in a github repository, and described in app manifest shipped with the Installer.
* dictionary - mandatory - 50-80MB
* TTS - Piper, one model per language, desktop only - 77MB. Android speaks through the platform's own TTS instead; nothing is downloaded there.
* Machine sentence translation - Helsinki Opus-MT int8 - 230MB each language pair, desktop only. Android translates through Google ML Kit instead, 43MB per language.
Initial package includes only core and app.

First time launch - user picks a foreign language they want to learn and download the dictionary.
User can download dictionaries for other foreign languages or additional components at any time via app.

Raise the version in the top-level `project()` call. On launch, app verifies current top-level version (project() call) 
and non-invasively proposes user to update the app.

## Testing

```
.\scripts\tools\build-msvc.ps1 -Test
```

Runs two binaries: `core` (`CoreTests.exe`) — scoring, answer comparison, selection, scheduling —
and `app` (`AppTests.exe`) — controllers, exercise flow, session checkpoints.

Excludes `[speech]` - requires audio synthesis, 20 second avg. Use `-Speech` ONLY if speech code has changed. 
Tests also non-deterministic (a VITS duration predictor is stochastic) — assert on margins or properties.

Excludes `[slow]` - tests over about 1.4 seconds each: sentence training on 2000 studied words, the built
sentence tree, OPUS-MT, long journal replays. The default run takes about 45 seconds, `-Slow` about
2.5 minutes. Use `-Slow` after touching sentence training, the sentence tree, translation, the journal,
or card promotion. Tag a new test `[slow]` when it takes over a second on its own.

Use `-Walk` after touching `Main.qml`, rail/tab QML, or a screen's routing signals — it opens every
shell screen and drives rail/tab navigation, training, exercises, drills, and repetition with real
posted clicks, desktop and mobile layouts both, then exits 1 if anything warned or routed wrong.
Not part of the default `-Test` run: each layout takes ~15-20s.
On Android, `python scripts\tools\walk-android.py` runs the same two walks on the installed Debug
build on the emulator. The emulator needs network, or the update check warns on the Today screen.

For a specific tag, run the executable directly. It needs Qt's `bin` on `PATH`:

```
$env:PATH = "C:\Qt\6.11.1\msvc2022_64\bin;$env:PATH"
build\msvc-debug\tests\Debug\CoreTests.exe "[scoring]"
```

Primary work environment is Mobile, your test bed is Emulator.
Within emulator, you have full freedom of actions - screenshot, replace db, check logs, launch with params, etc.

Any meaningful user-facing changes require additional manual confirmation on Emulator using mobile-mcp toolset.
See Mobile Workflow below.

Desktop UI checks go through the Ghost MCP server. It launches the app on a hidden desktop, so a check
never takes the user's focus. Never use mcp-windows: it brings the app window to the front for every action.
See [docs/desktop-ui.md](docs/desktop-ui.md) for the review steps.

## Mobile Workflow

Applies to bug reports and user-facing features alike. Test bed is the `medium_phone` emulator, driven through mobile-mcp.

Setup:
* Launch the adb emulator.
* `.\scripts\tools\build-android.ps1 -Install` — builds the Debug APK for the emulator (x86_64) and installs it.
  Run it in the foreground and wait for it to exit; don't background it expecting a notification,
  and don't trust an empty output file as "still running" - a Gradle daemon left over from a
  prior build shows up in `tasklist` too and proves nothing.
  Verify the install actually landed before testing against it:
  `adb shell dumpsys package com.goldenphasmid.ikonika | grep lastUpdateTime`, compared against `adb shell date`.
* Launch the app headless (-headless) to skip the sign-in onboarding flow.
* Seed dictionaries manually by copying the target language dictionaries from local data (data/built/fr or es), skipping the download phase.

Steps:
1. Ask complementary questions if the request is unclear. Bug: report is unclear or hard to reproduce. Feature: implementation requires them.
2. Launch the emulator with a freshly installed debug build.
3. Bug: reproduce using the provided steps. Feature: reach a place where the feature is user facing. Confirm the current state with a screenshot.
4. Investigate the codebase. Bug: confirm the exact cause. Feature: ideate a solution.
5. Bug: apply the fix. Feature: implement the solution. Rebuild and re-install to the emulator.
6. Bug: confirm it is resolved. Feature: confirm the solution was applied correctly, didn't break layout and caused no side effects. Make a screenshot.
7. Commit your changes.
8. Report back to the user:
   * Bug: issue and fix, short summary. Feature: any outstanding details the user needs to know.
   * Initial and final screenshots.

## Architecture

| Directory | Contents |
| --- | --- |
| `core/` | Static library. Data model, exercise engine, scoring, scheduler, dictionary lookup, audio resolution, database access. |
| `app/` | Qt Quick application. QML plus the C++ classes exposing `core` to QML. |
| `third_party/` | Vendored: miniaudio, sherpa-onnx headers, eflomal aligner (DLLs/models fetched separately). |
| `scripts/tools/` | Build, fetch, dev automation. |
| `packaging/` | Installer script fragments CPack includes when building the package. |
| `tests/` | Catch2 tests against `core`. |
| `docs/` | Design documents, see below. See also [docs/scripts.md](docs/scripts.md) for a script reference. |
| `scripts/dictionary/` | Offline dictionary pipeline: importer, frequency/CEFR word-set builders, `schema.sql`. |
| `data/` | Dataset store, not source controlled. `data/raw/` per-language source dumps. `data/built/<lang>/` - up-to-date dictionaries, one directory per foreign language — this is the only directory scanned. |
| `design/` | UI mockups, and `design/mobile-preview/` — the phone-screen mockup viewer. |
| `test-assets/` | test assets |

`core` links `Qt6::Core` and `Qt6::Sql` (for `QSqlDatabase`). No UI module.
 `CoreTests` needs a `QCoreApplication` (see `tests/TestMain.cpp`) to load SQLite integration.

**Schema changes must migrate in place from v0.1 onward.**

## Design principles

### Three Core Pillars - All in One Ecosystem App
1. Built-in word database with ability to quickly and easily add new words for learning. 
References - reverso, google translate.
2. Intensive Training - 15-20 minute session to learn 15 words. 
References - Words app, Quizlet, Duolingo.
3. Schedule Reviews using Leitner system - up to 3 levels of repetition, enforcing secondary learning and long-term memory. 
References - Anki

**Intensive Training.** 
Learn daily set of words with a click of a button. Each word in the dictionary has a teaching rank, commonly used words, which are proposed first.
User doesn't have to THINK what they want to learn.
Comprehensive dataset for commonly used words - multiple meanings with definition, translation and synonyms.
Add article for French/Spanish to learn gender while learning the words.

**Encourage user to create a Daily Habit**
Design includes Review and Notification to enforce secondary learning. Missed deadline demotes down the review 'ladder', or even drops 'learning' status from the word and it goes back to the pool.

**Card is a primary unit.** 
Every card lives in My Vocabulary. A word can be in multiple cards due to multiple meanings.
Decks are read-only word lists in the dictionary, such as IELTS General and IELTS Academic for English.

**Language-agnostic.** 
Source/target language are data. Each pair gets its own dictionary database.

## Terminology

| Term                   | Meaning                                                        |
| ---------------------- | -------------------------------------------------------------- |
| **Card**               | One thing being learned: word, translation, definition.        |
| **My Vocabulary**      | Every card of the language pair. The Learning Pool's user-facing name. |
| **Pool**               | The cards of My Vocabulary not yet on the ladder. Where a session takes its words from. |
| **Review Interval**    | Leitner stage implementation, words scheduled for the future.  |
| **Session**            | A sequence of exercises over a fixed card set.                 |
| **Intensive Training** | Moves cards from the pool onto the ladder.                     |
| **Review**         	 | Works through cards already on the ladder.                     |
| **Exercise**           | One activity kind.                                             |

## Code style

C++20, UE-style. PascalCase types/functions, Allman braces, early-return, no abbreviations, no
inline-comment argument labels. Verify symbol names rather than guessing. No namespaces. 'FT' 
`FT` prefix for classes and structs, `EFT` for enums. 
Cover app with logs - important call sites - error, invalid state, or reaction to the user action.
Match surrounding style where it differs.
**Avoid adding unnecessary hint text**

## Version control

Linear history. Rebase, not merge.

**Commit to `main`. Never create a feature branch**

**Stage the files you changed, not the working tree.** 

**Commit after every meaningful change**
Before reporting any task done, check `git status` and commit your changes. Group
commits by related changes.

## Prerequisites

| Tool | Path |
| --- | --- |
| Qt 6.11.1, MSVC 2022 kit | `C:\Qt\6.11.1\msvc2022_64` |
| Visual Studio 2022 Community (MSVC v143, C++ workload) | `C:\Program Files\Microsoft Visual Studio\2022\Community` |
| CMake 3.30.5 | `C:\Qt\Tools\CMake_64\bin` |
| Qt Creator | `C:\Qt\Tools\QtCreator` |
| Inno Setup 7, for `-Package` only | `%LOCALAPPDATA%\Programs\Inno Setup 7` |
| Ghost 0.23.4, for desktop UI checks. Registered with `claude mcp add ghost --scope user -e GHOST_SHELL=off -- %USERPROFILE%\tools\ghost\ghost-mcp.exe` | `%USERPROFILE%\tools\ghost` |

## Deep dive documents

- [docs/architecture.md](docs/architecture.md) — feature-to-class map: which controller owns which
  feature, what core types it sits on, known oddities. Read before touching a feature you haven't
  worked in yet.
- [docs/learning-model.md](docs/learning-model.md) — the card, the box ladder, session types,
  exercises, scoring. Read before discussing behaviour.
- [docs/grammar-forms.md](docs/grammar-forms.md) — verb conjugation, noun gender, adjective forms:
  three exercises pulling from the learner's own pool rather than a hand-picked ladder.
- [docs/technology-stack.md](docs/technology-stack.md) — technology choices, rejected alternatives, licensing.
- [docs/mobile.md](docs/mobile.md) — what stands between the desktop app and a phone release, in order.
- [docs/ios.md](docs/ios.md) — the iOS port: licence blockers, platform services, the CI pipeline
  that builds and signs without a Mac, App Review risks.
- [docs/desktop-ui.md](docs/desktop-ui.md) — the rules every redesigned desktop screen follows: shell,
  screen patterns, where mobile sheets go. Read before touching desktop QML.
- [docs/releases.md](docs/releases.md) — what ships, how an install updates itself, where the downloadable data lives.
- [docs/trainer-schema.md](docs/trainer-schema.md) — the app's own database (cards and their progress).
- [docs/book-reader.md](docs/book-reader.md) — the reader: two pipelines (PDF rendered, other
  formats reflowed), one add-word popup.
- [docs/dictionary-schema.md](docs/dictionary-schema.md) — the dictionary database.
- [docs/machine-translation.md](docs/machine-translation.md) — OPUS-MT on desktop against ML Kit on
  Android: the decision, its benchmark, and the dictionary search flow.
- [docs/dictionary-research.md](docs/dictionary-research.md) — dictionary data source research and evaluation.
- [docs/dictionary-pipeline.md](docs/dictionary-pipeline.md) — the ordered steps that build the dictionary database.
- [docs/sentence-selection.md](docs/sentence-selection.md) — how a Tatoeba sentence is filtered, graded for difficulty, and picked for a sentence exercise.
- [docs/translation-probabilities.md](docs/translation-probabilities.md) — corpus mining of translation probabilities.
- [docs/google-oauth-setup.md](docs/google-oauth-setup.md) — where to register the OAuth client and where the secrets go.
- [docs/workspaces.md](docs/workspaces.md) — one closed workspace per account plus one signed-out, the account
  registry, and the sign-in, sign-out and switch flows. Read before touching sign-in or data paths.
- [docs/profiling.md](docs/profiling.md) — which profiler to use: simpleperf on the emulator, Visual Studio's VSDiagnostics on desktop. Read before any performance work.
