# Scripts

Reference for scripts in  `scripts/` directory. All PowerShell scripts are invoked from the repository root, e.g. `.\scripts\tools\build-msvc.ps1`.
All Python scripts are invoked from `scripts/dictionary/`, `scripts/opus/`, or `scripts/tools/` directly, e.g.
`python build_fr_dictionary_v2.py` — see [dictionary-pipeline.md](dictionary-pipeline.md) for exact
argument shapes on the pipeline drivers.

## Build, fetch, launch (`scripts/tools/*.ps1`)

| Script | Purpose |
| --- | --- |
| `build-msvc.ps1` | Configure/build the MSVC 2022 desktop target. `-Release`, `-Configure`, `-Test`, `-Speech`, `-Walk`, `-Run`, `-Package` (installer, Release only), `-Msix` (Store package, Release only). |
| `build-android.ps1` | Configure/build the Android target. `-Abi` (default `x86_64`, the emulator kit; `arm64-v8a` for a real device), `-Release`, `-Configure`, `-Apk`, `-Aab` (Play Store bundle), `-Install` (installs the APK to the connected device/emulator). |
| `fetch-native.ps1` | Downloads binary files that are too large to be source-controlled, or external by nature: native libraries, speech models (sherpa-onnx, Piper) and the int8 OPUS-MT translation models. `-Force` to refetch. |
| `clear-local-state.ps1` | Wipes all per-user state: Roaming AppData, Local AppData, and the debug build's `local/` folder. Stops a running `IkonikaApp` first and retries a locked delete a few times. |
| `launch.ps1` | Launches the last-built Debug desktop app in desktop layout. `-Headless` starts it in the headless sandbox on an empty database, with the dictionaries from `data/built`. `-Database <file>` starts on a copy of an existing app database in the headless sandbox, such as one pulled from the emulator, and fills the sandbox with the dictionaries from `data/built`. `-Mobile` forces mobile layout. `-Onboarding` opens the setup screens in the headless sandbox, so the real data and the Windows startup entry stay as they are. |
| `launch-mobile.ps1` | Same as `launch.ps1 -Mobile`, always in the headless sandbox. Takes the same `-Database <file>` and `-Onboarding`. |
| `launch-mobile-preview.ps1` | Opens `design/mobile-preview/main.qml` (the phone-screen mockup viewer) via `qml.exe`. |
| `copy-directory-retry.ps1` | Recursive directory copy with retry/backoff. Used as a CMake `POST_BUILD` step (`app/CMakeLists.txt`) for the sherpa-onnx and OPUS-MT model directories, which occasionally fail to copy once from something briefly holding a freshly-written file open. |
| `copy-files-retry.ps1` | Same idea, one or more individual files. Used by `tests/CMakeLists.txt` for the sherpa-onnx DLLs beside `CoreTests.exe`. |
| `walk-android.py` | Runs the screen walk and the navigation walk on the installed Debug build on the emulator, then exits 1 if a walk step failed or a screen logged a warning. The walk switches reach the app as Qt's `extraenvvars` launch extra, which only a debuggable build reads. `--serial` picks another device. |
| `build-book-catalog.py` | Builds a language's book catalog, `books-<lang>.json`, and writes each book under its asset name with every Project Gutenberg mention stripped, from a TSV with `id`, `title`, `author`, `level` and `path` columns. `--target`, `--base-url` (where the files are published), `--out`. See [releases.md](releases.md#book-catalogs). |
| `publish-snapshot.py` | Replaces the working tree of a local `Ikonika-project` clone, the public source repository, with a commit of this private repository. It exports tracked files only with `git archive`, leaves out the dictionary pipeline, and scans the result with gitleaks. It refuses a clone that holds `catalog.json`, and never commits or pushes; it prints the commands to commit by hand. `--revision` (default `HEAD`, which must be clean), `--skip-scan`. See [ios.md](ios.md#repositories). |
| `make-sound-effects.py` | Synthesises the three sound effects (wrong answer, session complete, daily goal) into `app/android/res/raw` as 16-bit mono WAV. The files are committed; run it only to change a sound. |

## OPUS corpus alignment (`scripts/opus/*.py`)

| Script | Purpose |
| --- | --- |
| `preprocess_opus.py` | Filter OpenSubtitle parallel corpus and produce two separate aligned files, one for each language. See script header comment for more info. |
| `align_opus.py` | Word-aligns the cleaned corpus with `eflomal` (`third_party/eflomal/eflomal.exe`) and counts lexical translation pairs into `counts.txt`. |
| `normalize_counts.py` | Normalizes `align_opus.py`'s raw aligned-pair counts into a conditional probability per word. |
| `build_mwe_frequency.py` | Multi-word noun/phrase candidates from the native Wiktionary extract, ranked by literal phrase frequency against the cleaned corpus text (`counts.txt` is single-token pairs only and can't measure a phrase), with a per-million column. `--own-meaning-only` skips entries whose only senses point elsewhere; `--require-translation`/`--require-headwords` keep only phrases a translation source documents. Run through one wrapper per language, `build_<lang>_mwe_frequency.py` (fr, es, pt, en); only the English one skips pointer entries, since French Wiktionary files `il y a` as a form of `y avoir`. Output unions into the dictionary word set via `generate_word_set.py --extra-frequency-file` (fr/es) or `build_en_master_wordset.py --mwe-frequency-file` (en). |
| `run_opus.py` | Master driver that chains `preprocess_opus.py` -> `align_opus.py` -> `normalize_counts.py` -> `build_mwe_frequency.py`. Supports `--skip-preprocess` / `--skip-align` / `--skip-normalize` / `--skip-mwe-frequency` to resume partway through. |

## LLM tooling (`scripts/dictionary/*.py`)

| Script | Purpose |
| --- | --- |
| `ping_llm.py` | Checks whether the local OpenAI-compatible LLM server (e.g. LM Studio) is up and responsive. |
| `llm_generate_definitions.py` | Offline pass: groups a word's translations into meanings with the LLM, each with a gist, a target and a native definition, and an example sentence with its translation; translations that fit no meaning go into a dropped list. Produces the `*.llm.jsonl` artifact `merge_llm_results.py` later merges. A rejected reading is sent again on the next pass in smaller batches, until a pass of single readings resolves nothing. |
| `llm_deck_gists.py` (ikonika-llm only) | Offline pass: picks the one meaning each topic deck word with several meanings teaches, given the deck's scope. Produces the `*.deck-gists.jsonl` artifact `build_decks.py` reads. |

## Dictionary pipeline drivers (`scripts/dictionary/*.py`)

Each runs all six pipeline steps in order for one language pair. See
[dictionary-pipeline.md](dictionary-pipeline.md) for the fr/es steps walked through with real
input/output at every stage.

| Script | Produces |
| --- | --- |
| `build_fr_dictionary_v2.py` | French, from the English-Wiktionary extract. |
| `build_es_dictionary_v2.py` | Spanish, from the English-Wiktionary extract. |
| `build_it_dictionary_v2.py` | Italian, from the English-Wiktionary extract. No decks. |
| `build_en_uk_dictionary_v2.py` | English, translated for a Ukrainian reader — same headword and definition language, unlike the fr/es builds. `--word-set-only` imports the word set's headwords alone. It reads Lingea beside Balla and Busel, lets every source give all it has, and prunes after the corpus step; `--every-translation` turns that off. |
| `build_dictionary.py` | The lower-level driver the three scripts above call for steps 1–2; also directly runnable, see its own `--help`. |

## Dictionary pipeline steps (`scripts/dictionary/*.py`)

| Script | Step | Purpose |
| --- | --- | --- |
| `generate_word_set.py` | 1 | Merges a ranked lemma-frequency list with CEFR grades and writes one `words` row per (headword, part of speech) directly into the database (creates the schema too) — nothing later creates a row, only fills one in. `--extra-frequency-file` (repeatable) unions in additional frequency sources, e.g. `build_mwe_frequency.py`'s output. `--lemmatizer-file` skips a candidate that's itself an inflected form (pronoun/article only) of a different entry already in the set. |
| `build_lexique_frequency.py` | 1, fr | Lexique 3.82 → ranked lemma-frequency TSV. |
| `build_flelex_cefr.py` | 1, fr | FLELex → CEFR level per word, recomputed from its six per-band frequencies rather than FLELex's own `level` column. |
| `build_fr_master_wordset.py` | 1, fr | Wraps the two French adapters above plus `generate_word_set.py` into one step. |
| `build_espal_lemma_frequency.py` | offline, es | EsPal lemma and part-of-speech results → `es-frequency.tsv`, the same lemma-frequency TSV shape as `build_lexique_frequency.py`. Run once, not part of the rebuild. |
| `build_elelex_cefr.py` | 1, es | ELELex → the same CEFR TSV shape as `build_flelex_cefr.py`. |
| `build_es_master_wordset.py` | 1, es | Wraps the two Spanish adapters above plus `generate_word_set.py`. |
| `count_tagged_forms.py` | offline, pt | Counts every word form of a subtitle corpus with the part of speech a spaCy model tags it with. `--every N` reads one line in N. Needs the language's spaCy model, `pt_core_news_md`. |
| `build_tagged_frequency.py` | offline, pt | Folds those tagged form counts onto lemmas the English Wiktionary documents → `pt-frequency.tsv`, the same lemma-frequency TSV shape as `build_lexique_frequency.py`. Moves a pre-1990 spelling's count to the current spelling. Run once, not part of the rebuild. |
| `build_camoes_cefr.py` | offline, pt | The Referencial Camões PLE vocabulary inventory (saved page) → the same CEFR TSV shape as `build_flelex_cefr.py`, levels A1 to C2. |
| `prepare_european_portuguese.py` | offline, pt | `en-pt.jsonl` → `en-pt.european.jsonl`: removes meanings marked Brazil only, removes Brazilian spellings, gives a European spelling the meanings Wiktionary keeps under the Brazilian one, and puts the Portugal pronunciation first. Also writes `pt-brazilian-meanings.txt`, the headwords that lost a Brazilian meaning. |
| `build_pt_master_wordset.py` | 1, pt | Wraps `build_camoes_cefr.py` and `generate_word_set.py` over `pt-frequency.tsv`. |
| `build_kelly_cefr.py` | 1, it | The Italian Kelly list (`it_m3.xls`) → the same CEFR TSV shape as `build_flelex_cefr.py`, levels A1 to C2. Needs `xlrd`. |
| `build_it_master_wordset.py` | 1, it | Wraps `build_kelly_cefr.py` and `generate_word_set.py` over `it-frequency.tsv`. |
| `filter_tatoeba_portuguese.py` | offline, pt | Reads Tatoeba's export with authors (`por_sentences_detailed.tsv.bz2`) and decides each author's variant from the Brazilian and European marks in their sentences. Keeps every sentence of a European author (41 of 1,326 authors) and, of the other authors, the sentences with no Brazilian mark. Writes `por_sentences.european.tsv.bz2`, the file the Portuguese build reads. `--authors` writes the verdict per author; `--european-authors-only` keeps the first group alone. |
| `build_subtlex_frequency.py` | 1, en | SUBTLEX-US → the same lemma-frequency TSV shape, one row per lemma and part of speech like Lexique, from SUBTLEX's per-tag counts. `--wiktextract` moves an inflection Wiktionary documents ("going", "was") onto its lemma: always for a verb, for a noun only when its own page has no translation. |
| `build_en_master_wordset.py` | 1, en | SUBTLEX-US frequencies + CEFR-J grades, writing rows directly to the database like `generate_word_set.py` (own union logic, not a caller of it). Each reading carries its own rank; a CEFR-J reading SUBTLEX doesn't rank gets none. |
| `prepare_language_wiktextract.py` | pre-2 | Raw wiktextract extract → one language's `-only.jsonl` plus its lemmatizer TSV, so downstream steps read a per-language subset instead of the multi-language raw file. |
| `extract_language_lines.py` (`scripts/tools/`) | pre-2 | One-time pre-filter `prepare_language_wiktextract.py` calls to pull one language's lines out of a multi-language wiktextract extract. |
| `build_lemmatizer.py` | pre-2 | Surface-form → lemma TSV for one language, from its wiktextract extract. |
| `import_wiktextract.py` | 2 | wiktextract extract → fills senses, translations, examples, word_forms, categories onto the `words` rows step 1 already created. A pair wiktextract documents that the word set never scored on its own still gets a fresh row. |
| `build_gloss_translations.py` | 2 (inline) | Splits a gloss into candidate translations; called from `import_wiktextract.py`, not run standalone. |
| `build_reverse_gloss_translations.py` | 2, en-uk | Mines translations for a same-language-headword build out of the *other* Wiktionary edition's glosses. `--source-label` writes another `source` on the rows it inserts, so the prune step can tell them from Wiktionary's own tables. |
| `build_en_uk_dictionary_translations.py` | pre-2, en-uk | Pools the English-Ukrainian bilingual dictionaries under `data/raw/uk/dictionaries` (Balla 1996 and 2004, Busel 2009, Lingea 2023, Lew-Werbianyj 1947) into one TSV of headword, part of speech, translation, source dictionary, sense order and usage labels. |
| `dictionary_formats.py` | — | Lingvo DSL and StarDict readers used by the script above; not runnable. |
| `extend_uk_lemmatizer.py` | pre-4, en-uk | Wiktionary's Ukrainian lemmatizer plus the inflected forms Lingea's Ukrainian-English synonym index sends to its articles, about 240 thousand forms against 27 thousand; the English build's corpus step folds Ukrainian tokens with it. |
| `cross_reference_dictionaries.py` | 3 | Fills idioms, synonyms, gender, topic categories, and translations from the language's own native Wiktionary extract. Mandatory, not skippable. A word step 2 left senseless gets a real sense written from a matching native entry instead of just an unattributed translation, skipping "plural of X" pointer pages. |
| `build_everest_dictionary_translations.py` | offline | Decodes the 25 bilingual Everest volumes under `data/raw/everest_versiia_310/dictionaries` (Huffman-coded text, see [everest-extraction.md](everest-extraction.md)) and writes one pooled translations TSV per direction into `data/raw/everest_versiia_310/translations`, 26 files, plus `review.tsv` with the counts per volume and per file. Each volume fills its own direction and the inverted one. |
| `import_additional_translations.py` | 3, en-uk | Additional sources: adds translations from a pooled bilingual-dictionary TSV to existing words, keeping those two or more sources agree on and clean single-source ones, capped per word; flips `verified` on existing rows the sources confirm. Never reads a personal-name row. The English build reads Balla and Busel only. |
| `dictionary_metrics.py` | any time | Scores a built dictionary: coverage, translations per word, agreement, corpus measurement, definitions, examples, sentences. `--compare` lists what one build lost and gained against another. The metrics are described in [dictionary-pipeline.md](dictionary-pipeline.md#scoring-a-build). |
| `apply_word_tags.py` | 3b | Applies word-level tags from a TSV override (e.g. French's h-classification) that neither Wiktionary edition carries. |
| `drop_noisy_translations.py` | 3c, en-uk | Deletes translations that are not a word a learner would type: anything but Cyrillic letters, spaces, hyphens and apostrophes, an affix, a cut-off relative clause, or a grammar term on a non-noun ("службове дієслово" for "would"). |
| `collapse_translations_to_words.py` | 3d, en-uk | Moves every translation onto its headword, once per word and translation, without stress marks; none stays bound to a meaning. |
| `build_corpus_translations.py` | 4 | Turns the OPUS aligned-pair counts into `source = 'corpus'` translation rows. `--measure-only` (the en-uk build) adds none: it only writes count, probability and rank onto translations the word already has, and verifies those the corpus agrees with. `--exact-form-first` (the en-uk build) counts a subtitle word as written when the word already lists it ("люди", "мене"), and folds it only otherwise. |
| `prune_translations.py` | 4a, en-uk | Deletes the translations nothing but one copied dictionary stands behind. Keeps a translation a trusted dictionary (Lingea) or a Wiktionary table gives, one two independent sources agree on, and a one-word translation the corpus counted for 2% of the word's alignments. Balla and Busel count as one source. Takes out a translation the corpus never counted on a well attested word, a noun on a verb, and a corpus-only translation of the wrong part of speech, read from the Ukrainian extract and lemmatizer. A word left with nothing keeps its best three, so no word is lost. `--report` lists every deleted row. |
| `drop_untranslated_words.py` | 4b | Deletes a word that made it through steps 1-4 with no translation attached to any sense - the one step that removes a `words` row instead of filling one in. Run again after step 6. |
| `build_tatoeba_examples.py` | 5 | Matches Tatoeba sentence pairs against headwords and word forms, scores them, writes the top few per word. |
| `merge_llm_results.py` | 6 | Merges an `llm_generate_definitions.py` artifact into the database: replaces senses/translations, deletes the translations the LLM dropped, gives every new meaning the categories of the senses it replaced, sets `llm_complete`. |
| `build_sentence_tree.py` | 7, fr and es | Reduces every Tatoeba pair to the dictionary words needed to write it, picks the word the missing-word exercise blanks, and writes the `sentences` table and the `sentence_tree` blob. Needs spaCy and the language's model, `fr_core_news_md` or `es_core_news_md` (`pip install spacy`, then `python -m spacy download fr_core_news_md`). Flags: `--target-lang`, `--native-lang`, `--model`, `--min-words`, `--max-words`, `--processes`, `--limit`, `--stats`, `--dump`, `--dropped`. Both the French and the Spanish driver run it. |
| `form_sentences.py` | 7 (inline) | Picks two sentences per verb, noun and adjective form for the forms typing stage and writes `form_sentences`; called from `build_sentence_tree.py`, which passes it the word each token was read as. |
| `sentence_grammar.py` | 7 (inline) | Grades a sentence 1 to 4 by the hardest grammar it needs, reading verb tenses from `word_forms`; called from `build_sentence_tree.py`, not run standalone. |
| `vulgar_words.py` | 5 and 7 (inline) | Reads `overrides/<language>-vulgar-words.tsv` and decides whether a sentence pair is vulgar; called from `build_tatoeba_examples.py` and `build_sentence_tree.py`. |
| `test_build_sentence_tree.py`, `test_sentence_grammar.py`, `test_vulgar_words.py`, `test_form_sentences.py` | — | Unit tests for the scripts above, run with `python -m unittest`. None needs spaCy or a dictionary. |
| `build_learnable_readings.py` | 9 | Runs `learnable_readings.sql`: writes the `learnable_readings` table triage reads its words from, and the word range slider's `teaching_rank_ceiling_1000` and `level_teaching_ranks` metadata. |
| `compact_dictionary.py` | last | Rewrites the finished database without the free pages the drop and merge steps leave, which still hold deleted data. |
| `common.py` | — | Shared helpers (headword folding, etc.) used across the steps above; not a runnable script. |

## Present but not part of the pipeline

Per [dictionary-pipeline.md](dictionary-pipeline.md#not-part-of-this-pipeline) — kept in the tree, not called by any driver above:

| Script | Why it's here |
| --- | --- |
| `build_opus_frequency.py` | An alternative frequency source; not used by either master-wordset script. |
| `fill_translation_probabilities.py` | Only updates `probability` on existing rows; superseded by `build_corpus_translations.py`. |
| `fill_translations_opusmt.py` | A local neural-MT translation filler (ONNX OPUS-MT), unrelated to the OPUS-corpus alignment steps above despite the name. |
