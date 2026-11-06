# Dictionary schema

Reference for the dictionary database: one file per foreign language, built from wiktextract. The DDL lives in [`scripts/dictionary/schema.sql`](../scripts/dictionary/schema.sql); this document restates it with the semantics that aren't visible in the column types alone. See [dictionary-research.md](dictionary-research.md) for the build pipeline.

`metadata.schema_version` (currently 11, `DictionarySchemaVersion` in `FTDictionaryPackage.h`) gates compatibility: the app reads versions from `OldestDictionarySchemaVersion` (10) to the current one and refuses anything else. Changing the DDL, indices included, means bumping the constant in both places. A change that only adds columns keeps the oldest version, so dictionaries not yet rebuilt keep working, and code reading a new column checks the file's version first. A change that breaks an existing query raises the oldest version, and every dictionary has to be rebuilt. Version 10 added the three search indices: the translation lookup and the accent-folded headword and form lookups. Version 11 added the LLM pass's native definition and example to `senses`.

## languages, parts_of_speech

Lookup tables, not fixed enums, because wiktextract's own value sets are long and open-ended (parts of speech include "intj", "art", "num"; languages cover regional and constructed varieties such as "normand" and "volapük classique"). The importer inserts an unfamiliar value as a new row instead of dropping it.

## categories

`code` is the source's own category name, kept verbatim. `word_count` is a cache — the number of learnable words tagged with the category, restricted to taught parts of speech — refreshed by the build whenever translations change, not computed on read.

## words

One row per (`headword`, `part_of_speech_id`); the same headword can repeat only across different parts of speech.

- `frequency_rank`: rank across all words at that part of speech, from the language's frequency source. NULL where the source has no count for that reading.
- `cefr_level`: 0–5 for A1–C2. NULL where the source does not grade that reading.
- `teaching_rank` / `teaching_score`: the order to introduce words in, independent of `frequency_rank`. `teaching_score` is the raw score `teaching_rank` is sorted by. The score is the base-10 logarithm of `frequency_rank`, less a fixed amount by part of speech: noun 1.2, pronoun 1.0, numeral 1.0, adjective 0.8, interjection 0.8, verb 0.3, adverb 0.2. The interjection amount is for greetings: `olá` and `obrigado` are interjections. A second reading of a spelling adds 0.5: `novo` the noun is mostly the same card as `novo` the adjective, so it waits behind it. The pronoun amount is what puts `eu`, `ele` and `tu` in the first sessions; without it a pronoun waits behind every common noun.
- `conjugation_group`: French verb group (1, 2 regular; 3 irregular). NULL for non-verbs and for verbs the source does not group.
- `llm_complete`: whether an LLM pass has written this word's sense definitions. Set only on `words`; the pass does not write examples.
- `synonyms`, `idioms`: comma-separated text, not their own tables.
- `tags`: comma-separated, open-ended word-level markers, the same convention `senses.tags` uses. Currently only ever holds `"pronominal"` (whether the verb is reflexive/pronominal), but new markers go here instead of a new column.

## word_forms

Inflected surface forms mapped back to their headword's row, for lookup and search. `form` is case-insensitive.

`mood`, `tense`, `person`, `gender`, `number` are integer codes (see the column comments in `schema.sql` for the full mapping); 0 means "not applicable" in each. Unique per (`word_id`, `form`, `mood`, `tense`, `person`, `gender`, `number`).

## senses

A word's meanings; a word has one or more. `gender` lives here, not on `words`, because gender is a property of a meaning, not of the spelling — one `words` row can have senses with different genders.

- `definition`: target-language text shown to the learner.
- `gloss`: a foreign-Wiktionary gloss (e.g. English glossing a Spanish word), not a definition. A build populates one or the other, never both.
- `gist`: a short, usually one-word summary of the definition. Within a word and gender the LLM merge keeps it unique, numbering a repeat (`saisir`, `saisir 2`), because a card finds its meaning by headword, part of speech, gender and gist.
- `native_definition`: the definition in the native language. Written by the LLM merge only.
- `example`, `example_translation`: a target-language sentence that can only be read in this meaning, and its native translation. Written by the LLM merge only; `examples` holds the Tatoeba sentences, which belong to the word.
- `tags`: comma-separated usage markers (e.g. "familiar,rare"). Senses marked obsolete, archaic, or a variant spelling are dropped at import instead of tagged.
- `is_derived_sense`: set when the source marks this as a figurative or widened reading of another sense rather than a distinct meaning to learn.

## sense_categories

Many-to-many join between `senses` and `categories`.

## translations

One row per translation, from any pipeline step. `source` records which step wrote it: `wiktionary` (the source edition's own translation table), `gloss` (parsed from a native-language gloss), `corpus` (an aligned parallel corpus), `dictionary` (a bilingual dictionary, pooled by the additional-sources step), or `llm` (a completion pass).

`word_id` is always set; `sense_id` is set only when the step could attribute the translation to a specific meaning. An unattributed row (`sense_id IS NULL`) is a valid, usable translation — corpus alignment in particular is word-level, not sense-level, so most of what it produces is unattributed.

Two unique indexes enforce dedup separately for the two cases: `(sense_id, language_id, translation)` where attributed, `(word_id, language_id, translation)` where not. A re-run of the corpus step updates `count`/`probability`/`rank`/`verified` on an existing row rather than inserting a duplicate.

- `probability`: P(translation | headword) from the corpus. NULL where the corpus has no evidence, and only populated for the language the corpus was aligned for — other languages' rows are unranked.
- `rank`: 1-first within a (`word_id`, `language_id`) pair.
- Row order matters for a translation with no count. The app shows measured translations first, by probability, and the rest in `id` order. The build writes a word's gloss translations first, in the order of its meanings, then the native Wiktionary's, then Everest's.
- `verified`: set when the source's own text also contains this translation — two independent signals agreeing.

Every query that reads translations must filter by `language_id`; one that doesn't returns every language mixed together.

## examples

One row per example sentence, attached to a word, not a sense — the source (Tatoeba) carries no sense information. `example` is the foreign-language sentence; `translation` is the native-language one. Both `translation` and `language_id` are nullable, for a source sentence with no translation.

## decks, deck_words

Read-only word decks, written by `build_decks.py`. Both tables are optional: a dictionary without them has no decks, and the app hides the Decks screen. Adding them did not change `schema_version`.

- `decks`: `id` and a unique `name`. The id order is the order decks are listed in.
- `deck_words`: `deck_id`, `headword`, `part_of_speech` (the `parts_of_speech.code`), an optional `gist` and an optional `context`. `id` is the order within the deck. A row names a reading by text, not by `words.id`, the same key a card uses, and `gist` names one meaning of that reading the way a card's `MeaningGist` does. The build leaves out a row whose reading or meaning the dictionary lacks.

## learnable_readings

The words a new words run offers, written by step 9 of the [pipeline](dictionary-pipeline.md) from `scripts/dictionary/learnable_readings.sql`. The table is optional: on a dictionary without it, the app works the same words out with its own queries, which read most of the word table each time a run starts. Adding it did not change `schema_version`.

One row per learnable reading and native language: `language_id`, `headword`, `part_of_speech` (the `parts_of_speech.code`), `gender`, `cefr_level`, `frequency_rank`, `teaching_rank` and `translation`, the reading's best translation into that language. A reading is learnable on the same terms the app's query uses: a ranked word of a learnable part of speech, and an interjection only when it is one word. A reading with no translation into a language has no row for it. The index on language, level, part of speech and teaching rank serves both the count per part of speech and the candidate list a run reads.

The same step writes two `metadata` rows for the word range slider: `teaching_rank_ceiling_1000`, the deepest teaching rank that still leaves 1,000 learnable words after it, and `level_teaching_ranks`, a JSON list of each CEFR level with the median teaching rank of its words. The app queries either value when its row is absent.

## sentences, sentence_tree

The sentence index the sentence exercises query. Both tables are optional: a dictionary built without step 7 of the [pipeline](dictionary-pipeline.md) has neither, `FTSentenceTree::Load` returns nothing, and the app offers no sentences. Adding them did not change `schema_version`.

### sentences

One sentence pair per row: a Tatoeba pair, or the example sentence of an LLM meaning with its translation.

| Column | Type | Meaning |
| --- | --- | --- |
| `id` | INTEGER PRIMARY KEY | Dense, from 1. The tree stores it as a `uint32`. |
| `source_id` | INTEGER | Tatoeba's id of the foreign-language sentence. NULL for the example sentence of an LLM meaning. |
| `language_id` | INTEGER, `languages(id)` | Language of `translation`, the same convention as `examples`. |
| `text` | TEXT NOT NULL | The foreign-language sentence, stripped. |
| `translation` | TEXT | The native-language sentence. |
| `gap_start`, `gap_length` | INTEGER, nullable | Position of the sentence's rarest dictionary word inside `text`, in UTF-16 code units. NULL when no token qualifies. Read by `SplitAtGap` in `FTSentenceCards.h`. |
| `level` | INTEGER NOT NULL DEFAULT 1 | Grammar level, 1 to 4. |
| `features` | INTEGER NOT NULL DEFAULT 0 | Bit mask of the grammar features found; the bits are listed in the pipeline doc. |
| `changed_count` | INTEGER NOT NULL DEFAULT 0 | How many dictionary words appear in a form other than their headword. |

The table has no index besides the primary key: rows are only ever read by `id`, in batches of 500, by `FTDictionaryRepository::LoadTreeSentences`.

### sentence_tree

One row, `id = 1` (enforced by a CHECK). `format` repeats the format number stored in the blob. `data` is the prefix tree, all values little-endian:

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | Magic, the bytes `IKST`. |
| 4 | 4 | Format, currently 3 (`FTSentenceTree::Format`). |
| 8 | 4 | Node count N. |
| 12 | 4 | Sentence reference count R. |
| 16 | 4 | Word span S: the highest `words.id` plus one. |
| 20 | 4 × N | `node_word`: the key on each node. A key below S is a `words.id`. A key of `tense × S + words.id` is a tense key: that verb trained in that tense, with the `word_forms.tense` code (1 present, 2 imperfect, 3 past simple, 4 future, 8 perfect). Node 0 is the root and holds 0. |
| … | 4 × (N + 1) | `first_child`: node i's children are nodes `first_child[i]` to `first_child[i + 1] - 1`. |
| … | 4 × (N + 1) | `first_sentence`: the sentences ending at node i are references `first_sentence[i]` to `first_sentence[i + 1] - 1`. |
| … | 4 × R | `sentence_id`: one `sentences.id` per reference. |
| … | R, padded to 4 | `level`: one byte per reference, the sentence's grammar level. |

Total size is `20 + 4 × (N + 2 × (N + 1) + R) + R rounded up to 4`.

Nodes are in breadth-first order with each node's children in ascending word id. A path from the root spells one sentence's words, rarest first by `teaching_rank`, each conjugated verb followed by its tense keys. A sentence is writable when every key on its path is known. Its length counts the word keys only. A sentence with an ambiguous word has one reference per reading, so R exceeds the row count of `sentences`.

`FTSentenceTree::Parse` rejects a blob unless: the magic and format match; N is at least 1; the size is exact; `first_child` starts at 1, ends at N and never decreases; `first_sentence` starts at 0, ends at R and never decreases; every node with children has `first_child[i] > i`. The last check is what guarantees the walk terminates.

Word ids in the blob are valid only for the dictionary file it is stored in. The app holds the parsed tree in memory (`AppDictionary::GetSentenceTree`) and drops it when the dictionary is closed or switched.

## form_sentences

The two sentences of every form the forms typing stage asks, a conjugated verb or a noun or adjective form, picked once by step 7 of the [pipeline](dictionary-pipeline.md). Optional: a dictionary without it makes the app pick sentences from the tree at session start. Adding it did not change `schema_version`.

| Column | Type | Meaning |
| --- | --- | --- |
| `headword` | TEXT NOT NULL | The word's headword. |
| `kind` | TEXT NOT NULL | The forms kind: `verb`, `adj` or `noun`. |
| `tense` | TEXT NOT NULL | The tense key of a verb, such as `present` or `perfect`. Empty for a noun or adjective. |
| `first`, `number` | INTEGER NOT NULL | The form's cell. `first` is the person 1 to 3 of a verb, or the gender of a noun or adjective, 1 masculine or 2 feminine. `number` is 1 singular or 2 plural. |
| `rank` | INTEGER NOT NULL | 0 or 1, the order of the two sentences. |
| `sentence_id` | INTEGER NOT NULL | `sentences.id`. |

The primary key is the six columns before `sentence_id`, and the table is `WITHOUT ROWID`, so a lookup by headword, kind and tense reads one range of the key. A form with fewer than two fitting sentences has fewer rows. A sentence id appears once in the whole table.
