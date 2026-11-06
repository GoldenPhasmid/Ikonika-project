# Machine translation

## Problem

The dictionary looks words up by headword. A phrase, an idiom or a whole sentence finds nothing:
"il pleut des cordes" returns an empty list. The reader has the same gap, since a word's sense
depends on the sentence around it.

Machine translation fills that gap. It translates the whole text the learner typed or selected.

## Decision

Each platform uses its own engine. Desktop runs OPUS-MT int8. Android runs Google ML Kit
on-device translation.

Both engines translated the same 50 French sentences, described in [Benchmark](#benchmark).

| | OPUS-MT int8 | ML Kit |
| --- | --- | --- |
| Platforms | Any, through ONNX Runtime | Android and iOS only |
| Wrong translations out of 50 | 6 | 12 |
| Download per language | 229 MB | 43 MB, both directions |
| Added to the app | ONNX Runtime, 16.6 MB | A 16 MB native library per Android ABI |
| Mean time per sentence | 62 ms on desktop | 28 ms on the x86_64 emulator |
| Licence | CC-BY 4.0 | Free; Google attribution beside every result |
| Language pairs | One model per pair | Every pair goes through English |

The two timings come from different machines, so they compare only roughly.

### OPUS-MT on desktop

Pros:

- It makes half the errors. ML Kit loses subjects and pronouns: "Bien qu'elle soit fatiguée"
  becomes "Although it is tired", and "je l'ai vue" (Marie) becomes "I saw it". It also drops
  words: "Je le lui ai donné" becomes "I gave it this morning".
- Desktop already ships ONNX Runtime for speech, so the engine adds no library.
- ML Kit has no desktop version.

Cons:

- 229 MB per language.
- Its register is casual, from the subtitles it was trained on: "I'm gonna call the doctor".
- It can drop a second sentence: "Tu as vu Marie ? Oui, je l'ai vue hier." becomes "Did you see
  Marie?".

### ML Kit on Android

Pros:

- The download is a fifth of OPUS-MT's.
- Android dropped ONNX Runtime on 2026-10-28, and ML Kit doesn't bring it back.
- ML Kit's text recognition can read PDF pages that have no text layer, for the reader.

Cons:

- It makes twice the errors.
- The model downloads from Google's servers.
- Every result needs Google's attribution on screen.
- A native language other than English goes through English, which loses meaning twice.

### int8 over fp32

The fp32 OPUS-MT export is 423 MB, against 229 MB for int8. On the benchmark, int8 gave the same
translation for 45 sentences. The other 5 were equally correct or better: "If you'd worked harder"
instead of "worked more". Int8 was also about 16% faster.

### Shared failures

Both engines translate idioms word for word. "Il pleut des cordes" became "raining ropes" and
"raining strings". "J'ai le cafard" became "I've been in the cockroach" and "I have been afoil".
Both misread "mémoire" (a thesis) as "memories" and "brief".

## Benchmark

The hidden test "French translation benchmark across grammar" in `tests/TranslationTests.cpp`
translates 50 sentences and prints each translation with its time. The sentences cover gender and
plural agreement, reflexive verbs, tenses from the passé simple to the futur antérieur, the
conditional, the subjunctive, the pronouns "y", "en" and "le lui", negation, the passive,
questions, relative clauses, idioms and three long sentences. English, Spanish and Portuguese have
their own 50 sentences covering the same ground in their grammar.

```
build\msvc-debug\tests\Debug\CoreTests.exe "[translation-benchmark][fr]"
```

The tags `[en]`, `[es]` and `[pt]` pick the other three. The `OPUS_MT_BENCHMARK_DIRECTORY`
environment variable points the test at another model directory.
ML Kit was measured on the same sentences by a throwaway Android app outside the repository.

### Punctuation

The OPUS-MT training text went through a punctuation cleanup first, so the models have barely seen
typographic marks. The translator does the same cleanup on each sentence before translating it.
Curly apostrophes and quotes become plain ones, guillemets become plain quotes, dashes become
hyphens, an ellipsis becomes three dots, and narrow or non-breaking spaces become plain spaces.
Without it, "ship’s time" in English to Ukrainian came out with a music note in place of the
apostrophe.

The text is split into sentences before the cleanup, so dialogue dashes and guillemets still mark
where a sentence starts.

### Per-model settings

A model directory may hold a `translation.json` with two optional fields:

- `excludedCharacters`: the translator never picks a word piece that contains one of these letters.
  English to Ukrainian sets `ыэъёЫЭЪЁ`, letters Ukrainian lacks, because the model was trained on
  data with Russian mixed in. Without it, "You should have told me the truth." comes out in Russian.
- `targetToken`: a language tag put in front of every sentence, for models that translate into
  several languages, such as `>>ukr<<`.

### English to Ukrainian: opus-mt-en-uk over opus-mt-tc-big-en-zle

The newer tc-big model translates English into Ukrainian, Russian and Belarusian, picked by the
`>>ukr<<` tag. In int8 it is 369 MB unpacked, against 113 MB for opus-mt-en-uk, and it was no
better on the benchmark. It handled the formal sentences better: "sit the final examination" and
"purchasing power" came out right. It did worse on everyday ones: "this afternoon" became
"this evening", "a house in the country" became a house in a nation, "I wish I had listened to my
parents" reversed who listened to whom, and "raining cats and dogs" was translated word for word.

## User flow

Machine translation is on by default. The CMake option `IKONIKA_ENABLE_MACHINE_TRANSLATION` turns it
off for a release that should not ship it. While it is off, the app never looks for a model, never
asks ML Kit about one, and shows none of the screens below. The published packages stay in the
catalog, and nothing offers them.

```
cmake -DIKONIKA_ENABLE_MACHINE_TRANSLATION=OFF build\msvc-debug
```

With it on, dictionary search shows machine translation when it finds nothing. A search of several
words is matched as a whole string only. It is never split into its parts, so a phrase the
dictionary does not hold goes to machine translation.

1. With the model installed, the search text is translated at once. A card labelled "Translation"
   replaces the empty list, with the language pair such as FR → EN at its top right, the searched
   text small above the translation, and a row of Listen, Copy and a save button under it. The
   button reads "Save phrase" for two or more words and "Add as word" for one. The result count
   and "Nothing found" are not shown while the card is up, since the search did not fail.
2. Without the model, the same place shows "Translate phrases offline" and an install button with
   the download size: 229 MB on desktop, 43 MB on Android. The row shows progress, then translates
   the current search when the download finishes.
3. An ML Kit result carries Google's attribution line under it.

In a book on desktop, selecting two or more words shows a translate button in the margin, beside the
last line of the selection. Clicking it opens a box under the selected text with its translation, a
button that reads the original aloud, and one that copies the translation. Without the model the box
offers the download instead. The box follows the text as the page scrolls and closes when the
selection goes away or on Escape. A selection of up to three words still opens the dictionary pane,
since the dictionary holds phrases that long.

Settings, Account opens a page for each language, which offers the same download under Sentence
translation for the language being learned. The model loads on the first translation, not at launch.

On desktop the model is a catalog package of kind `translation`, unpacked into the downloaded
models directory. Four pairs are published:

| Package | Source model | Download | Unpacked |
| --- | --- | --- | --- |
| `opus-mt-fr-en` | onnx-community/opus-mt-fr-en | 185 MB | 232 MB |
| `opus-mt-es-en` | onnx-community/opus-mt-es-en | 202 MB | 250 MB |
| `opus-mt-pt-en` | Xenova/opus-mt-ROMANCE-en | 71 MB | 116 MB |
| `opus-mt-en-uk` | Xenova/opus-mt-en-uk | 72 MB | 114 MB |

Portuguese has no model of its own, so it uses the one that reads any Romance language. The Xenova
exports quantize the decoder to about 60 MB, against about 190 MB for the onnx-community ones.

On Android ML Kit downloads its own model from Google, so the catalog plays no part. Every pair ML
Kit supports is offered, English to Ukrainian included. ML Kit reports no size or progress, so the
button shows no size and the download shows no percentage.

## Corpus notes

- OpenSubtitles remains the corpus of choice: clean human translations. CCMatrix, NLLB and
  ParaCrawl are machine-mined or machine-translated, and were rejected.
- OPUS monolingual files are the source side of the same bitext, not new content. Alignment needs
  bilingual pairs.
