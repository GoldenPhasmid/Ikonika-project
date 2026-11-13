# Dictionary build pipeline

This document covers fr and es v2 dictionary pipeline. `build_pt_dictionary_v2.py` builds European Portuguese with the same steps; its word-set sources are described at the end of step 1, and it reads the Tatoeba sentences `filter_tatoeba_portuguese.py` kept. Entry point - `build_fr_dictionary_v2.py` and `build_es_dictionary_v2.py`, the two scripts that build the shipped French and Spanish dictionaries. Both run the same steps below, using both English and Native Wiktionary extract.  
Required components. Each of them is generated offline , not a part of build_dictionary script:  
* Lemmatizer - word-lemma pairs, built offline for each language in the pair from Native Wiktionary dataset.
*  OPUS aligned word pairs produced by OpenSubtitles dataset.  
* Native Wiktionary extract.  
* LLM pass - generated definitions fr headwords, matched to translations.
* Multi-word noun frequency - `build_mwe_frequency.py`, run via `run_opus.py` (not `build_dictionary.py`): phrase counts for compounds neither Lexique/EsPal nor FLELex/ELELex score, since both are single-token lexicons.

See [dictionary-schema.md](dictionary-schema.md) for database schema.

```
python build_fr_dictionary_v2.py [--native-jsonl PATH] [--llm-artifact PATH]
python build_es_dictionary_v2.py [--native-jsonl PATH] [--llm-artifact PATH]
python build_pt_dictionary_v2.py [--native-jsonl PATH] [--llm-artifact PATH]
```

The walkthrough below follows one word, `manger` ("to eat" / "a manger, a feeding trough"), through every step of the French build, with real inputs outputs of each script. `manger` is a useful example because it is two headword + POS pairs that share the spelling, but have very different usability in the actual language: a very common verb and a much rarer noun. Pipeline has to produce correct frequency, grade and match translations separate per pair and don't rely on the spelling.

## Adding a language

A new language needs data, code and release packages. This list assumes the English Wiktionary and the Everest volumes already cover it. `build_it_dictionary_v2.py` or `build_pt_dictionary_v2.py` is the closest copy for the driver.

Dictionary data, in `data/raw/<lang>/`:
* The English Wiktionary entries and the native Wiktionary extract, prepared by `prepare_language_wiktextract.py` into headwords and a lemmatizer.
* A frequency list counted from the language's side of OpenSubtitles: `count_tagged_forms.py`, then `build_tagged_frequency.py`, and phrase counts from `build_mwe_frequency.py`.
* A CEFR list and a reader for it. Every language so far has its own: `build_flelex_cefr.py`, `build_elelex_cefr.py`, `build_camoes_cefr.py`, `build_kelly_cefr.py`.
* The OpenSubtitles corpus aligned with English by `scripts/opus/run_opus.py`, for the translation counts of step 4.
* The Tatoeba sentence pairs for step 5, and a spaCy model for the sentence tree of step 7.
* `overrides/<lang>-vulgar-words.tsv` and `overrides/<lang>-tagged-readings.tsv`.
* The language's entries in the grammar tables of `sentence_grammar.py` and `form_sentences.py`.

LLM passes, in ikonika-llm:
* A worked example in `llm_generate_definitions.py`, then the definitions pass, merged by step 6.
* Topic decks: `overrides/<lang>-topic-decks.tsv`, `overrides/topic-names.<lang>.tsv`, the deck names in `deck-scopes.tsv`, then the deck gist pass. See step 8.

App code:
* The language's name in `AppLocalization.cpp`, `SettingsStore.cpp` and the language map in `DictionaryController.cpp`.
* Noun articles in `FTArticleRules.cpp`.
* The trained tenses and verb groups in `FTWordForms.cpp`. See [grammar-forms.md](grammar-forms.md).

Speech and translation, desktop only:
* Nothing for speech. Supertonic is one package for every language, and Android speaks through the phone's own voice.
* An OPUS-MT model pair, added to `scripts/tools/fetch-native.ps1`. Android translates through ML Kit.

Release, in ikonika-releases. See [releases.md](releases.md):
* The dictionary, voice and translation packages in `catalog.json`.
* A book catalog, `books-<lang>.json`, with beginner, intermediate and classic books. A classic another language already publishes is the easiest to add.

## 1. Word set

`build_fr_master_wordset.py` (Spanish: `build_es_master_wordset.py`) produces the target headword list. It has two jobs: rank headwords by real-world frequency, and grade them by CEFR level, then hand both to `generate_word_set.py`, which creates the database (schema included) and writes one `words` row per (headword, part of speech) directly into it — nothing later in the pipeline creates a `words` row, every subsequent step only fills content into one that already exists.

Frequency comes from `build_lexique_frequency.py` reading Lexique 3.82 (`Lexique382.tsv`). Lexique's own row for `manger` looks like this (trimmed to the useful columns):

```
ortho   lemme   cgram  freqlemfilms2  freqlemlivres
manger  manger  NOM    5.62           4.05
manger  manger  VER    467.82         280.61
```

`cgram` is Lexique's own part-of-speech code; `POS_CODE_BY_LEXIQUE_CGRAM` maps `NOM` → `noun` and `VER` → `verb`. The two frequency columns are corpus counts per million words, films and books. Script blends them (books weighted 20% by default) into one number per row: `0.8 × freqlemfilms2 + 0.2 × freqlemlivres`. For the verb that comes to roughly 430; for the noun, roughly 5.3. Resulting database has `manger`/verb at `frequency_rank = 242` and `manger`/noun at `frequency_rank = 6300` — the verb is two orders of magnitude more common, and the two parts of speech are ranked as if they were different words.

Spanish frequency comes from `data/raw/es/espal/es-frequency.tsv`, the same `folded spelling pos rank count` shape, with `count` in occurrences per million subtitle words. It is written once, offline, and is not rebuilt with the dictionary. EsPal's own database is form-level, so each lemma was uploaded to EsPal's "Words to Lemma and POS properties" tool (subtitle tokens) in chunks of 4,500, which returned `espal_lemma_pos_frequency.tsv`: per word, lemma and tag, the reading's frequency and its share of the lemma. `build_espal_lemma_frequency.py` turns that into one row per (lemma, part of speech):

- A lemma's frequency is the reading's frequency divided by its share of the lemma, and it is split over parts of speech by how the lemma's own spelling is tagged. The form `mira` therefore adds to the verb `mirar`, not to a noun `mira`.
- Proper-noun tags are left out of the split. A spelling that is mostly proper noun is dropped, unless it is an ordinary word that subtitles capitalise: it stays when most of its tokens are not forms of another lemma and the English Wiktionary documents it as a word. `gracias`, `dios` and `soler` stay; `david`, `srta` and `bob` go.
- EsPal's number category becomes `num`. Its date category covers months, weekdays and times, and counts as a noun when Wiktionary documents the lemma as one, so `junio`, `martes` and `medianoche` keep a noun row.
- A part of speech under 2% of the spelling's readings is dropped, and a single letter survives only as a preposition or conjunction.
- EsPal files some spellings under another lemma although the dictionary treats them as words of their own: `fruta` under `fruto`, `novia` under `novio`, `la` under `el`. A spelling that Wiktionary documents as a word of a part of speech, and that EsPal has tokens of that part of speech for under another lemma, gets its own row with EsPal's frequency for that spelling. Wiktionary's separate entry decides, so a plural with an entry of its own, such as `flores`, becomes a word too.

`build_es_master_wordset.py` reads this file directly and keeps rows at 0.01 per million and above (`--min-frq`). The 244,983-form `espal_subtitletokendb_05_20_2013.tsv` is no longer read by the build.

Portuguese has no published list that splits a spelling's count over its parts of speech in the current spelling. SUBTLEX-PT, the European Portuguese subtitle list, gives one count per form with every tag the form can take, and it is written in the spelling used before the 1990 agreement: it has `acção` and `óptimo` and no `ação` or `ótimo`. So the Portuguese list is counted from the corpus the build already aligns, the European Portuguese side of OpenSubtitles (`pt`, which OPUS keeps apart from `pt_BR`), in two offline steps:

The English Wiktionary documents both variants on the same pages and keeps the full entry under the Brazilian spelling where they differ, so the build does not read `en-pt.jsonl` directly. `prepare_european_portuguese.py` writes `en-pt.european.jsonl` from it, and every Portuguese step reads that file:

- A meaning marked Brazil and not Portugal is removed: `rapariga` keeps "girl" and loses the Brazilian insult. 6,503 meanings and 3,964 entries go.
- A Brazilian spelling that only points at the European one is removed, so `fato` keeps "suit" and loses "fact".
- A European spelling that only points at the Brazilian entry (`económico`, `registo`, `connosco`) takes that entry's meanings: 335 entries.
- The Portugal pronunciation is put first. Wiktionary lists the Brazilian one first, and the import takes the first.
- A verb keeps the forms of its first conjugation table. Wiktionary adds a second table for colloquial short forms (`tou`, `tás` under `estar`) or for the verb with its pronoun attached (`sinto-me` under `sentir`), and the import would read both as second spellings of the same person. A form marked Brazil is removed, so the past of `falar` is `falámos` only.

It also writes `pt-brazilian-meanings.txt`, the 4,895 headwords that lost a Brazilian meaning. Everest and the Portuguese Wiktionary are mostly Brazilian and carry no region marks, so for those headwords steps 3 and 3c only confirm a translation the word already has (`--confirm-only-headwords`). Everest writes a verb bare (`do`) where the glosses write `to make`, so steps 3 and 3c put `to` before a bare verb, modals aside. Step 3c gives an English article to an article, determiner, numeral or contraction only: `the` reached the preposition `a` through the spelling it shares with the article. Step 3c treats every phrase the same way (`--confirm-only-phrases`): Everest lists a phrase with the sentences it appears in, which gave `por favor` "another cup of coffee".

- `count_tagged_forms.py` tags one line in five with spaCy's `pt_core_news_md` and counts each form with its tag, into `data/raw/pt/opus/pt-form-pos-counts.tsv`. Names and numbers are not counted.
- `build_tagged_frequency.py` folds the forms onto lemmas into `data/raw/pt/opus/pt-frequency.tsv`. Every row is a headword and part of speech the English Wiktionary documents, so a rank cannot land on a reading the import has no entry for. A form Wiktionary lists only as an inflection gives its count to the lemma; a form of two lemmas (`pode` of `poder` and of `podar`) is split in proportion to what each lemma's own forms were counted. A past participle that is also a word (`obrigado`) counts as that word. A verb entry that lists a conjugated form of another verb counts as that form even when it adds a use of its own, so `vamos` goes to `ir` and is not a verb headword; a form Wiktionary only describes in words (`foram`, "third-person plural preterite/pluperfect indicative of ir") is read from the description. A verb with an attached pronoun (`disse-me`, `fazê-lo`) counts as the verb. A contraction (`do`, `numa`) is a word of its own. The tagger files `sim` under the adverb, where Wiktionary has only "indeed", and "yes" is under the interjection; `overrides/pt-tagged-readings.tsv` lists such forms (`sim`, `seu` and its forms, `menos`, `baixo`) with the part of speech to count them under (`--readings`). A lemma Wiktionary marks as replaced by the 1990 agreement gives its count to the current spelling.

The tagger was trained mostly on Brazilian news and is weak on second-person forms: it calls `tu` an interjection far more often than a pronoun. The fold corrects the cases it can: a form Wiktionary has under one part of speech takes that one, and an interjection tag is tried as a function word first.

`generate_word_set.py` ranks a phrase from the multi-word file on the single-word scale, as for French and Spanish. Before that fix the phrase list's own ranks put `não é` and `por favor` among the first words taught. In `build_pt_master_wordset.py` a phrase also takes the parts of speech the English Wiktionary documents it under, since the multi-word file carries the Portuguese Wiktionary's (`por isso` is a phrase there and a conjunction in the English edition), and the ranked row would otherwise be a different row from the one holding the meanings.

Portuguese CEFR levels come from `build_camoes_cefr.py`, reading the vocabulary inventory of the Referencial Camões PLE, the Camões institute's CEFR reference for European Portuguese as a foreign language. The saved page (`data/raw/pt/camoes/referencial-camoes-ple.html`) lists words and phrases for each of the six levels under 165 notions. A word takes the lowest level it is listed at and every part of speech Wiktionary documents for it. Greetings and numbers are not in the vocabulary inventory: they appear as example lines in the functions and grammar sections ("▪ Olá (Patrícia)!", "▪ dois, seis, dez"). Those lines are read for A1 and A2, and a line that is a word or phrase Wiktionary documents is graded. The inventory is a list of examples, not a graded corpus, so it grades about 4,200 words where ELELex grades 14,290.

`build_it_dictionary_v2.py` builds Italian with the Portuguese steps, without the Brazilian filtering. The frequency list is counted the Portuguese way, from the Italian side of OpenSubtitles (`data/raw/it/opus/it-frequency.tsv`). CEFR levels come from `build_kelly_cefr.py`, reading the Italian Kelly list (`data/raw/it/kelly/it_m3.xls`, CC BY-NC-SA 2.0), which grades about 5,200 lemmas of a web corpus A1 to C2. A word keeps Kelly's part of speech when the English Wiktionary documents it, and otherwise takes the ones Wiktionary documents. The Italian build has no decks. Its LLM pass runs on the Codex backend: `python run.py it --backend codex --model gpt-6-luna --reasoning high --parallel 3` in ikonika-llm. A verb keeps the forms of its first conjugation table, one spelling per cell, since the later tables hold archaic and regional forms.

CEFR grading comes from `build_flelex_cefr.py`, reading FLELex (`FleLex_TT_Beacco.tsv`), which measures how often a word turns up in French-as-a-foreign-language textbooks written for each level. FLELex ships its own opinion of the level in a `level` column, but we ignore that and recompute based on six per-band frequencies instead. Reason is partially 'this looks better' and sometimes 'why flelex says B2 when it has 50% usage in A section'. The rule - first band, in A1→C2 order, whose running share of the word's total frequency crosses 10%. FLELex's row for the verb:

```
word    tag  freq_A1    freq_A2   freq_B1   freq_B2  freq_C1  freq_C2  level
manger  VER  1064.7056  504.2399  309.6845  94.0751  92.388   12.833   A1
```

Over half of the verb's total frequency mass sits in the A1 band, so the running share crosses 10% immediately and the recomputed level agrees with FLELex's own tag: A1. The noun tells the opposite story:

```
word    tag  freq_A1  freq_A2  freq_B1  freq_C1  freq_C2  level
manger  NOM  0        6.6656   0.438    0        0        B1
```

FLELex's own column claims B1, but the noun is zeroin A1 and 94% in A2; the running share crosses 10% at A2, so the recomputed level is A2, not B1. This is exactly the case the recomputation step exists to correct.

`generate_word_set.py` unions the ranked frequency list with the graded CEFR list, one row per (headword, part of speech), and writes each straight into `words` (`headword`, `part_of_speech_id`, `frequency_rank`, `cefr_level`, `phonetic = NULL`, `llm_complete = 0`). For `manger`, that's two rows:

```
headword  pos    frequency_rank  cefr_level
manger    verb   242             0
manger    noun   6300            1
```

A word the graded list has no level for can take the level of another part of speech with the same spelling. It does so only when it is more common than that graded word, or the graded word has no frequency rank. The graded list tags `son` the possessive differently than Wiktionary does, so the possessive takes that A1. `grand` the noun is rarer than `grand` the adjective, so the noun stays without a level. English applies the same rule to CEFR-J.

Before writing, a candidate that the lemmatizer says is itself an inflected form of a different word already in this same set gets no row of its own — `--lemmatizer-file` (optional). Lexique/FLELex rank plenty of pronoun and article inflections as if they were their own lemma ("leurs" ranked separately from "leur"); their real lemma will pick up the spelling as a `word_forms` row once step 2 imports it, the same way a wiktextract "form of" page already works. This check is deliberately narrow — restricted to closed grammatical classes (`pron`, `article`), and never trusting a lemmatizer match whose lemma is itself a verb — because French has heavy noun/verb spelling overlap the lemmatizer doesn't distinguish from real inflection: "chose" (thing, a noun) also happens to spell a conjugated form of the rare verb "choser", "terre" (land) of "terrer". Widening the check to nouns or verbs measurably drops thousands of common words for an unrelated word's sake — checked directly against a real run before landing on this restriction.

Lexique and FLELex are both single-token lexicons: a noun-noun-de compound like `pomme de terre` never becomes a lemma in either, so it never reaches the word set on its own, even though wiktextract documents it. `build_mwe_frequency.py` reads the native Wiktionary extract for space-separated noun (French: `pos = noun`) or phrase (Spanish: `pos = phrase`) headwords, counts each candidate's literal occurrence in the OPUS OpenSubtitles corpus text (`clean.<lang>`, from `run_opus.py`), and writes a frequency TSV in the same shape as `build_lexique_frequency.py`'s. `generate_word_set.py --extra-frequency-file <path>` (repeatable) unions it in: a `(headword, part of speech)` the primary frequency file already ranked keeps its own rank, everything else is added fresh. A phrase is ranked on the single-word scale from its `per_million` column: it takes the rank of the first word counted less often per million. The primary file's `count` is per million for Lexique, EsPal and the Portuguese corpus alike. The multi-word file's own `rank` is the phrase's place among phrases, which put `à la` at rank 3 and `pomme de terre` at 2121, beside far more frequent single words.

The aligned-pair counts (`counts.txt`) `build_corpus_translations.py` reads are single-token pairs from a word aligner and can't measure a phrase — this is why `build_mwe_frequency.py` reads the raw sentence text directly instead, and why it runs as a `run_opus.py` step (a full corpus scan, minutes, not seconds) rather than inside the fast dictionary rebuild.

## 2. Import

`import_wiktextract.py` against the English-Wiktionary extract (`fr-only.jsonl`), with `--edition en --skip-examples`. Reads wiktextract's dictionary entries and, for a `headword + POS` pair step 1 already created a row for, fills in `senses`, `word_forms`, `categories`, and the translation table. A pair wiktextract documents that the word set itself never scored (a second reading of an already-qualifying headword) still gets a fresh `words` row here, same as before step 1 existed.

A topic category the English edition sets on the whole entry (`fr:Anatomy` on `tête`) goes on every sense that entry writes, the same as one set on a single sense. There is no cap on senses per word: the LLM merge replaces them anyway, and a word without an LLM reading ships every sense it has.

English-edition entry for the verb `manger` is one JSON object per headword; trimmed to useful info:

```json
{
  "word": "manger", "lang_code": "fr", "pos": "verb",
  "senses": [{"glosses": ["to eat"], "tags": ["transitive"]}, {"glosses": ["to eat"], "tags": ["intransitive"]}],
  "forms": [
    {"form": "mange", "tags": ["first-person", "indicative", "present", "singular"]},
    {"form": "mange", "tags": ["indicative", "present", "singular", "third-person"]},
    {"form": "mangé", "tags": ["participle", "past"]}
  ]
}
```

Primary goal of this step: better translation coverage than the native edition gives on its own. English Wiktionary documents French words for people learning French, so its glosses are already short target-word candidates; French Wiktionary documents them for French speakers, with longer native-language definitions that don't split into translations the same way. Measured on the shipped build: gloss-derived coverage alone reaches 93.4% of headwords, against 74.2% for native Wiktionary's own translation table used alone — see [Translation coverage by source](#translation-coverage-by-source) in step 3 for the full breakdown, native included.

Because this is the English edition, `--edition en`, importer uses 'glosses' instead of 'definitions': sense text goes into `senses.gloss` (here, `"to eat"`), not `senses.definition` - `definition` is produced way later using LLM artefact. In the same pass, each gloss is split into candidate translation rows (`source = 'gloss'`) by `build_gloss_translations.py`'s function, called inline rather than as its own process — so import already writes a `gloss` translation row for `"to eat"` before any corpus or LLM step has run.

Splitting a gloss into translations is a parsing algorithm in itself — a gloss is free-text English, and most of what separates its parts is explanation, not a list of synonyms. `gloss_translations()` runs four filters in ordert:

1. **Strip parentheticals first.** `chien`'s second sense glosses as `"cock, hammer (of a firearm)"` — without stripping `(of a firearm)`, the comma-split would still work, but a parenthetical elsewhere can itself contain commas and would wrongly produce extra candidates. Stripped, this gloss becomes `"cock, hammer"`.
2. **Split on semicolons and on commas that aren't inside a number.** `"cock, hammer"` splits into `["cock", "hammer"]`. The comma rule specifically skips a comma between digits so we don't numbers `"1,000"`
3. **Drop any part that opens with an explanatory phrase**, not a translation: `indicates`, `used`, `denotes`, `refers`, `a form of`, `alternative`, `synonym of`, `abbreviation`, `obsolete`, and a dozen more. The French tennis interjection `let` glosses as `"indicates a let on service"` — that's sentence explaining what saying "let" *does*, not translating the word. Opening match drops it.
4. **Drop any remaining part longer than four words.** The same `let` gloss would also fail this test on its own (five words) even without the `indicates` match — past a handful of words, a gloss part reads as a description of the word's use, not a term a learner would type as a translation.

More rules sit around those four. A part that describes a pronoun and then names it gives the name: `"third-person feminine singular nominative pronoun she"` gives `she`. A part that opens with "a" or "an" keeps the article as the gloss writes it (`"a medical test"`), except in the Portuguese build, which passes `--strip-gloss-articles` and gets `medical test`; a quantity keeps its article either way (`"a lot of"`). A gloss that says the word is a pre-reform spelling gives nothing. A gloss that only points at another entry gives nothing: `"See ter de, ter que."`, `"Same as entry above."`, `"Used other than figuratively or idiomatically: see uma, vez."`. A part that names a form or a part of speech is dropped: `"plural of pai"`, `"inflection of ir:"`, `"first-person singular reflexive pronoun"`. A contraction is glossed by what it is made of, so its translation is the reading given in quotes: `"contraction of a + os, literally “to the (masculine plural)”"` gives `to the`, and `"contraction of de (“from”) + aí (“there (near you)”)"` gives `from there`; `of/from the` is written out as `of the` and `from the`. Wiktionary files a contraction as an alternative form of the words it joins, and the import drops alternative forms, so a meaning marked as a contraction is the one exception to that.

`"to eat"` passes all four checks: no parenthetical, one part, no explanatory opening, two words. `"cock, hammer (of a firearm)"` passes them once the parenthetical is gone and the comma has split it. `"indicates a let on service"` passes none of them, and that sense ends up with zero gloss-derived translations — which is fine; not every sense is expected to produce one.

The common verb `accuser` is a cleaner demonstration of the length cutoff specifically, because none of its four senses match the explanatory-opening filter, so it's the word count alone deciding what passes:

```
gloss                                   words  passes?
to accuse                               2      yes
to find fault with                      4      yes  (exactly at the limit)
to show; to reveal                      2 + 2   yes, as two rows: "to show", "to reveal"
to acknowledge receipt of something     5      no   (one word past the limit)
```

`"to find fault with"` is a four-word English phrase describing what `accuser` means in one of its senses, not a grammar note, and it passes because the cutoff is "more than four," not "four or more." `"to acknowledge receipt of something"` reads the same way — it's a real, usable translation-shaped phrase, not an explanation like `indicates` — but at five words it still reads more like a definition than a term a learner would type, and it's dropped for length alone, with no explanatory-opening match involved. That sense (`accuser réception`, "to acknowledge receipt of something") ends up with no gloss-derived translation at all until a later step — the corpus or the LLM pass — supplies one.

The check runs again, independently, inside `build_corpus_translations.py` (step 4) when it decides which sense a corpus-measured pair belongs to: it re-splits the same gloss, tokenizes each candidate that passed into individual words, and checks whether the corpus's own translation word (`"eat"`, bare, not `"to eat"`) is among them — which is why a two-word gloss like `"to eat"` is still able to catch a one-word corpus translation like `"eat"`.

The `forms` array is where inflections come from. Wiktextract tags each form with mood/tense/person/number in prose ("first-person", "indicative", "present", "singular"); the importer maps that vocabulary onto the integer codes `word_forms` actually stores, so the form `mange` above becomes two rows differing only in `person`:

```
form    mood(1=ind)  tense(1=pres)  person  gender  number
mange   1            1              1       0       1
mange   1            1              3       0       1
```

Compound tenses never reach `word_forms`. The source does not spell them out: for `parler`, passé composé is the literal text `present indicative of avoir + past participle`, tagged `multiword-construction`, and every other compound tense has the same shape. The importer skips any row carrying that tag. The template text names the auxiliary (`avoir` for `parler`, `être` for `aller`), so a later import can compose the real forms from it, the auxiliary's own forms, and the past participle, which is already imported.

Only standard inflections reach `word_forms`, decided by three structural rules and no list of bad tags. First, a row qualifies only when every tag it carries is grammatical: mood, tense, person, gender, number and a few structural markers. `informal` is one of those markers: Spanish tags its ordinary second person singular with it ("hablas"), and tags the regional voseo form with `vos-form` on top ("hablás"), which is not a marker, so the first is kept and the second excluded. `diminutive` is not a marker either: a page such as "papelina", a feminine diminutive of "papel", would otherwise fill the feminine cell of "papel" as if it were its feminine form. A row tagged archaic, Quebec, nonstandard, slang or anything else is therefore excluded, including tags the source adds later. Second, a standalone "form of" page is classified one sense at a time, because a page such as `vas` carries a standard second-person sense and a Quebec first-person sense, and pooling their tags put the dialect reading in the standard cell. Third, a form page only fills a cell the lemma's own conjugation or declension table leaves empty, so the table's `vais` keeps the first-person cell and the page's `vas` cannot join it. A page spelling that appears as a word inside a table spelling is still kept (`emparait` beside the table's `s'emparait`), since it is the same form without its clitic.

When a cell still has several spellings and only some appear in Lexique (`--attested-forms`, French only), the unattested ones are dropped: this removes `bones` beside `bonnes`. Spellings that are all attested stay, so `paie` and `paye`, or `ciel` with `cieux`, remain as alternates.

`--skip-examples` is passed here specifically to stop the importer writing wiktextract's own literature-citation examples — step 5 replaces them with Tatoeba sentences instead, and keeping both would double up the `examples` table for no benefit.

## 3. Cross-reference

`cross_reference_dictionaries.py` fills gaps in the English-Wiktionary-sourced build from the language's own native Wiktionary extract, for the same headwords: idioms, synonyms, gender, and translations. It reads the native extract directly (`fr-native.jsonl` / `es-native.jsonl`, from `prepare_language_wiktextract.py`) rather than a prebuilt database — there is no `data/intermediate/<lang>-native/` build anymore, and this step is mandatory, not skippable.

Idioms, synonyms, and gender are word-level facts, so `(headword, part of speech)` alone is enough to place them, and nothing already present is overwritten — only filled in. Definitions are not touched: the two editions number their senses independently with no shared key, so there is no honest way to say which definition on one side corresponds to which on the other.

Topic categories from the native entry's senses (`Lexique en français de l’anatomie` becomes `anatomie`) go on every sense of the matching reading: the word's senses with the native entry's gender, or all of them when the native entry has no gender. A category describes the reading, not one meaning, so it needs no sense alignment. The native edition's entry-level categories are skipped: nearly all of them are grammar and spelling lists (adjective forms, letters, first names).

Translations sidestep that problem by not attempting sense alignment at all. Every translation the native extract has for a headword, in the target language, is pulled in unattributed to any sense — the same shape an unattributed corpus translation already has. A candidate is folded the same way `merge_llm_results.py` folds translation text (case-folded, a leading `"to "` stripped) and checked against every translation the word already has, across every sense:

- If the folded text matches an existing row, that row's `verified` flips to 1. Its `source`, `count`, `probability`, and `rank` are untouched — a match against an existing corpus row keeps that row's real measurements; a match against a gloss row leaves it with none, same as it always had.
- If nothing matches, a new row is inserted: `source = 'wiktionary'`, `sense_id = NULL`, no `count`/`probability`/`rank` — there's nothing here to compute one from, and that's fine, a gloss row has never had one either.

For `manger`/verb, native French Wiktionary's own translation table also lists `"eat up"`, a spelling the gloss (`"to eat"`) and the corpus alignment never produced. It doesn't fold to match the existing `eat` row, so it's inserted fresh:

```
sense_id  translation  source       count    probability  verified
60983     eat          corpus       361358   0.8777       1
NULL      eat up       wiktionary   —        —            0
```

This step runs before the corpus (step 4) specifically so that a corpus-measured pair colliding with an *identical* native translation string merges into that row rather than creating a duplicate — see step 4's note on why `corpus`-labeled row counts shifted down after this step was added.

### A word step 2 had nothing for

A word-set headword step 2 found no English-edition match for is not dropped: the `words` row already exists (step 1 created it), just with no `senses` yet. This step checks that directly — a `word_id` already in `senses` is enriched the normal way above (idioms/synonyms/gender, unattributed translations); a `word_id` with none yet gets a real sense written from scratch when a matching native record has one, via `import_wiktextract.py`'s own `write_word_content()` with `glosses_are_native = False`, since this edition *is* the target language's own, not a foreign gloss to translate. `chauffeur de taxi` is a real case: no entry in the English edition, so step 2 leaves it senseless; native French Wiktionary has both a definition (`"Homme qui conduit un taxi."`) and a translation table (`taxi driver`, `cabbie`, `cabdriver`, `cabman`), so step 3 writes the sense from that alone, translations attached to it directly rather than left unattributed.

A native record that is itself a "plural of X" / "feminine of X" pointer page is not a headword in its own right, and `is_form_of()` — the same check step 2 already applies — skips it here too, so a redirect page never becomes a fake, translation-less sense. A word this step also finds nothing for stays senseless (or sense-only, translation-less) until step 4 (corpus) has its own try; `drop_untranslated_words.py`, after step 4, is what finally decides a word never earned a real card. Any translation row counts, whether it is attached to a sense or sits on the word alone.

### Translation coverage by source

Numbers below are from the French build before multi-word support existed, denominator = 40,275 headwords throughout. "Native alone" is a counterfactual: what native Wiktionary's own translation table would cover on its own, independent of storage — it does not describe rows actually persisted by this step, which is the "native, persisted" column instead.

| stat | native alone (counterfactual) | gloss | corpus | native, persisted (this step) | combined (shipped) |
| --- | --- | --- | --- | --- | --- |
| coverage (≥1 translation) | 74.2% (29,885) | 93.4% (37,611) | 45.3% (18,226) | 34.4% (13,846) | 98.8% (39,794) |
| translations per headword+POS (over all 40,275) | 1.60 | 1.73 | 1.10 | 0.70 | 3.53 |
| translations per *covered* headword+POS | 2.16 | 1.85 | 2.43 | 2.03 | 3.57 |
| non-redundant contribution (vs. the other two persisted sources) | — | 16,028 | 1,285 | 209 | — |
| top 5,000 by frequency, coverage | — | 93.1% | 86.2% | 66.1% | 99.9% |
| `verified` rate | — | 25.5% | 22.6% (9,973 of 44,201) | 0% (never flips its own inserts) | — |

Two things explain why "native, persisted" (34.4%) looks much smaller than "native alone" (74.2%): most of what native would cover on its own, gloss or corpus already covers too, so this step flips `verified` on an existing row instead of adding a new one — 34,708 such flips happened on this build, none of which show up as new coverage in this table since they don't change which words have a translation, only whether one is corroborated. And corpus's own row count (44,201) is lower than it was before this step existed (47,816), because some translations that used to become new `corpus` rows now update a pre-existing `wiktionary` row in place instead — the coverage moved to a different `source` label, not away.

Corpus's `verified` rate is computed independently of this step and unaffected by it: it only checks a candidate against the same word's own gloss text, run at corpus's own step (5), after this one. This step's flips and corpus's own gloss-agreement check are two separate, non-overlapping ways a translation ends up corroborated.

## 3a. Native transcriptions

`fill_native_phonetics.py` runs right after step 3 in the French, Spanish, Italian, Portuguese and German drivers. It fills `words.phonetic` where the import left it empty, from the native extract step 3 already reads.

```
python fill_native_phonetics.py <dictionary.db> <native.jsonl>
```

**Problem.** The English edition carries an IPA string for far fewer entries than the native one. On the French build 13,087 of 54,055 words had no transcription, and the native extract has one for 11,935 of the 12,612 headwords concerned. Wiktionary was not the gap: the import never read the native `sounds`.

**Decision.** A native record fills the row with the same headword and part of speech, and an existing transcription is never replaced. A row still empty then takes the transcription of another reading of the same headword, from the database or from a native record of another part of speech, only when every transcription of that headword agrees. `douze` the adjective is said as `douze` the numeral, while `boxer` the noun and `boxer` the verb are not said alike and stay empty.

The French edition writes IPA between backslashes, which becomes the slashes every other source uses. A string with a missing or doubled slash is repaired. A string that is not delimited IPA is skipped: an ellipsis placeholder, or the ASCII notation some Portuguese entries use. The German and Spanish editions write narrow transcriptions between square brackets, and those stay as written.

| Dictionary | Words without a transcription, before | After |
| --- | --- | --- |
| French | 13,087 of 54,055 (24.2%) | 678 (1.3%) |
| Spanish | 8,547 of 44,739 (19.1%) | 3,823 (8.5%) |
| German | 14,320 of 51,407 (27.9%) | 3,019 (5.9%) |
| Italian | 10,468 of 35,914 (29.1%) | 6,650 (18.5%) |

**Trade-offs.** Most of what is left in Spanish and Italian has no record in the native extract at all, so no source here can fill it. A word without a transcription still gets a card, and the screens reserve the line so the layout does not move. The sharing rule leaves 6 French headwords with differing readings empty on purpose, which costs a few cards a transcription and avoids teaching a wrong one.

## 3c. Additional sources

`import_additional_translations.py` adds English translations from the Everest volumes
(`data/raw/everest_versiia_310/translations/fr-en.tsv` and `es-en.tsv`, see
[everest-extraction.md](everest-extraction.md)). A translation the word already has is marked
`verified`. A new one is inserted on the word with no meaning, `source = 'dictionary'`, at most 20
per word.

Everest gives no part of speech, so `--translation-pos` reads the English Wiktionary extract: a
translation reaches a reading only when its own part of speech there is compatible. This keeps
`eat` off `manger` the noun. Rows with these labels are not read:

| Build | Labels skipped | Why |
| --- | --- | --- |
| French | `truncated` | The technical volume is cut at 25 characters. Its other rows are read; the volume is written in capitals with no accents, so it is lower-cased and reaches only headwords spelled without an accent. |
| Spanish | `doubtful`, `truncated` | The technical volume repeats headwords with a neighbouring entry's translation. |

## Everest and the corpus

The question was whether Everest lets the corpus stop adding translations and only measure the
ones other sources give. It does, and the drivers build that way. Three fresh builds per language,
scored with `dictionary_metrics.py`, with Everest still skipping its `technical` and `phrase`
rows:

| French | Before Everest | Everest, corpus adds | Everest, corpus measures only |
| --- | --- | --- | --- |
| words | 53,616 | 55,829 | 54,845 |
| words dropped, no translation | 17,799 | 15,586 | 16,570 |
| words dropped from the top 5,000 | 1,742 | 1,497 | 1,580 |
| translations | 155,724 | 194,305 | 167,981 |
| translations agreed by two sources | 46,493 | 59,796 | 63,950 |
| translations with a corpus count | 60,600 | 62,337 | 47,633 |
| meanings with a definition | 54,547 | 54,567 | 52,863 |
| artifact translations the merge could not place | 5,786 | 5,786 | 10,438 |

| Spanish | Before Everest | Everest, corpus adds | Everest, corpus measures only |
| --- | --- | --- | --- |
| words | 44,737 | 46,422 | 45,700 |
| words dropped, no translation | 6,875 | 5,190 | 5,912 |
| words dropped from the top 5,000 | 1,609 | 1,206 | 1,264 |
| translations | 127,679 | 182,971 | 151,583 |
| translations agreed by two sources | 31,627 | 46,990 | 52,027 |
| translations with a corpus count | 67,128 | 68,765 | 48,783 |
| meanings with a definition | 42,055 | 41,909 | 37,610 |
| artifact translations the merge could not place | 5,924 | 5,924 | 21,003 |

Adding Everest while the corpus keeps adding is a gain on every count: 2,213 more French words
and 1,685 more Spanish ones, with 508 and 1,685 translations lost on the words both builds have.

Taking the corpus's own translations away costs, against that build, 984 French words and 722
Spanish ones, and 26,324 French and 31,388 Spanish translations. It also costs definitions. The
LLM artifact was written against corpus translations, and the merge deletes a meaning when none
of its translations is in the build: 1,704 French meanings and 4,299 Spanish ones go.

What the measure-only build gains is agreement: more translations rest on two sources, because
none rests on the corpus alone.

The words and translations lost are mostly not worth keeping. Of the words only a corpus
translation keeps in the build, the ones in the top 5,000 are wrong readings and verb forms with a
co-occurring word for a translation: `mon` as an adjective with `buddy`, `a-t-il` with `wrong`,
`vas` as a noun with `going`. Past rank 5,000 about 70% are the same kind, such as `killer` with
`serial`. The rest are rare honest words, `rancard` with `date`, about 300 French and 200 Spanish.

The drivers therefore run the corpus measure-only, and `--corpus-inserts` builds the old variant.
The cost that remains is the definitions: the LLM artifact has to be regenerated against a
measure-only build, so its meanings stop depending on corpus translations.

The shipped builds also read Everest's `technical` and `phrase` rows:

| | French | Spanish |
| --- | --- | --- |
| words | 55,125 | 45,724 |
| words dropped, no translation | 16,290 | 5,888 |
| translations | 179,553 | 152,265 |
| translations agreed by two sources | 65,216 | 52,027 |
| median translations per word, top 1,000 | 5 | 4 |
| meanings with a definition | 52,893 | 37,610 |

## 4. Corpus translations

When the step only measures, it also counts pairs whose English side is a function word. That is how `ser` gets a count for "to be" and `ter` for "to have". When the step inserts, those pairs are still skipped. A function word is never counted for a phrase that contains it, or every `the` opposite the preposition `a` would count for "the subject". A translation the word already has is also counted under its own spelling when the step only measures: the English lemmatizer calls "yes" a plural of "ye", and folding it there left `sim` with no count for "yes".

The drivers pass `--measure-only --min-count 5`: the corpus writes a count, a probability and a
rank on the translations the earlier steps gave a word, sets `verified` on those it agrees with,
and inserts none. A translation the word already has is measured however small its share of the
word's alignments, so `burden` seen 20 times under `charger` ranks above `overdraw` seen never.
The 2% share only decides which measured translations get `verified`. The rest of this section describes the inserting mode, `--corpus-inserts`.

`build_corpus_translations.py` reads the OPUS aligned-pair counts file (`counts.txt`) — raw word-alignment counts between English and French subtitle sentences — and turns the strongest pairs into `source = 'corpus'` translation rows. Each line of `counts.txt` is `native_word  target_word  count`; every one of these is paired against the English word `eat`, and the aligner treats each French spelling as an unrelated word:

| `manger` | inflected forms of `manger` |
| --- | --- |
| 64,755 | mange 25,042 · mangez 7,042 · mangé 6,103 · manges 4,866 · mangent 3,560 · mangeons 3,167 |

The right-hand column is all the same French verb — `mange`, `mangez`, `mangé`, and the rest are conjugated forms of `manger`, not separate headwords. The aligner has no notion of lemmas, so it counts each of them as a distinct pair from `manger` itself. `eat` folds the same way on the English side: `counts.txt` also carries `eating` (11,762), `eaten` (1,120), `ate` (926), and `eats` (313), each paired against bare `manger`. The script folds both columns back onto their lemmas before aggregating — the French side via the French lemmatizer, the native side via whichever lemmatizer `--lemmatizer` points at (English, here) — so every line above, and every other inflected pairing of `manger` and `eat` in `counts.txt`, adds into one running total for the headword rather than being counted as unrelated pairs. It then normalizes that total into a probability P(translation | headword), and — because the gloss step already wrote `"to eat"` for this exact sense in step 3 — routes the aggregated count to that same sense rather than leaving it unattributed. The row this step inserts, exactly as shipped:

```
word_id  sense_id  translation  source   count    probability  rank  verified
2093     60983     eat          corpus   361358   0.8777       1     1
```

`count` here (361,358) is larger than the single raw line above because it is the sum over every inflected form of `manger`, not just the bare infinitive.

`verified` is this step's only correctness signal. It means something narrower than "correct." It is 1 when the corpus's own measured translation also turns up in wiktextract's gloss text for that word. It is 0 otherwise. The corpus has no idea what a word means. It only knows which English and French tokens keep lining up in translated subtitles. That can happen for reasons that have nothing to do with translation — two words can simply tend to appear in the same kind of sentence. Agreement with the gloss is a cheap way to tell "the aligner found a real translation" apart from "the aligner found a coincidence," without asking a human to check every row.

The check runs once per translation candidate. It compares the candidate with the `translation` column of the word's existing rows, across every sense, after the same folding the merge uses (case-folded, "to " stripped). The match is exact: the candidate has to equal a whole translation. A word that is only part of a multi-word translation never counts. At this point the existing rows are the gloss rows from step 2, one per gloss part as split by `gloss_translations()`, and the native-edition rows from step 3. `manger`'s only sense glosses `"to eat"`, stored as the row `to eat`. The corpus's own top candidate for `manger` is `"eat"`, which equals it after folding, so that candidate comes back `verified = 1`.

`salon` makes the distinction concrete. Its gloss `"living room"` is one translation, so step 2 writes one row for it. The aligner emits single words, so the corpus proposes `living` and `room` separately. Neither equals a whole existing translation, but both are whole words inside the row `living room`. A candidate like that gets no row of its own: its count is added to the row that contains it (split evenly when more than one row does), and the containing row keeps the `verified` flag it already had.

| translation | count | probability | verified | why |
| --- | --- | --- | --- | --- |
| living room | 7,648 | 0.404 | 1 | the gloss row, plus the counts of `living` (5,465) and `room` (2,183) |
| parlor | 1,256 | 0.066 | 1 | equals a native-edition row |
| lounge | 1,154 | 0.061 | 0 | no other source lists it |
| shop | 670 | 0.035 | 0 | no other source lists it |
| salon | 414 | 0.022 | 1 | equals the gloss row `salon` |

`lounge` is a correct translation of `salon`, and `verified = 0` says only that nothing else in the dictionary lists it. A fragment never sets `verified` on the row that absorbs it, because a word inside a phrase is not agreement with the phrase.

Not every corpus translation lands on a sense. The Spanish build's noun `comer` ("a meal") gets a corpus-derived translation `meal` that the gloss step never wrote and nothing else claims, so it is inserted with `sense_id = NULL` — a real, usable translation, just not filed under any particular meaning:

```
word_id  sense_id  translation  source   count  probability  rank
(comer)  NULL      meal         corpus   6889   0.0215        3
```

## 4b. Drop untranslated

`drop_untranslated_words.py` is the one step that deletes a `words` row rather than filling one in. A word-set headword that made it through steps 1–4 with no translation at all, attached to a sense or on the word alone, is not a usable card, and nothing left in the pipeline can still give it one — writes it to `<target>.no-translation-words.txt` (headword, part of speech, frequency rank), then deletes it and everything that points at it. Run a second time after step 6 (LLM merge), since that step can replace a word's only sense with one whose translations don't match anything measured before, deleting the meaning and leaving the word senseless again — rare, but real. The second run writes its own list, `<target>.no-translation-words-after-llm.txt`, so the first run's list survives. A build with no LLM artifact deletes that file, since no second run happened.

## 5. Examples

`build_tatoeba_examples.py` fetches the target-language sentences, the English sentences, and the sentence-link table into `data/raw/<lang>/tatoeba/` first, skipping any file already on disk, then matches the sentence pairs against headwords and `word_forms`, scores every candidate sentence, and writes up to `--per-word` (default 3) per word into the `examples` table. Only pairs whose target-language sentence is between `--min-words` and `--max-words` tokens (3 and 14 by default) are even considered; everything else is scoring three things about a sentence that survives that filter. A pair the vulgar word lists flag is skipped before scoring, as [sentence-selection.md](sentence-selection.md#vulgar-words) describes.

**Length and balance.** `score_sentence()` starts every candidate at `-abs(length - 7)`, so a seven-word sentence scores 0 and anything longer or shorter loses one point per word of difference — seven is treated as the sweet spot between a fragment and a passage. It then subtracts `3 × balance`, where `balance` is how much the native-language sentence's word count differs from the target one's, as a fraction of the target length. A translation pair whose lengths diverge a lot is more likely a loose paraphrase than a literal translation, and the ×3 weight makes that penalty dominate the length term. The sentence `"J'ai une demande à te faire."` (its own headword worked through below) happens to be a perfect case: both the French and the English side tokenize to exactly seven words, so it scores `-abs(7-7) - 3×0 = 0` — the best a sentence can score on this axis.

**How rare the matched word-form is.** A sentence can match a headword through a common form ("a") or a rare one ("avoir"), and matching on the rare one is worth more: it's better evidence that the sentence is actually about that word rather than incidentally containing a common inflection shared with something else. `form_weight()` computes this as `log(sentences_scored / document_frequency)`, capped at 4.0 — `document_frequency` is how many candidate sentences contain that exact folded surface form at all, so a form that shows up in almost every sentence scores near zero bonus, and one that shows up in a handful scores close to the cap. When a sentence matches a word through more than one of its surface forms at once, only the rarest form's weight counts — the comment in the source gives the example directly: a sentence containing both `"avoir"` and `"a"` is evidence for `"avoir"`, and there's no reason to add the two.

**Which headword an ambiguous form belongs to.** A surface form can belong to more than one headword — `"demande"` is both the noun (`demande`, "a request," `teaching_rank` 391) and an inflected form of the verb `demander` ("to ask," `teaching_rank` 328). Left to the simple rule ("give it to whichever owner is best taught"), every sentence containing `"demande"` would go to `demander`, since 328 beats 391 — including a sentence that is actually using the noun. `reads_as_a_noun()` catches this from context alone: it looks at the token immediately before the match, and treats it as a noun use when that token is a genuine determiner (`un`, `une`, `mon`, `cette`, ...) and the token immediately after isn't itself the form of some other noun. For the real sentence `"J'ai une demande à te faire."`, the word before `demande` is `une` — a determiner, not one of the ambiguous ones — and the word after is `à`, not a noun form, so the sentence is routed to `demande` the noun rather than `demander` the verb, overriding what the teaching-rank fallback alone would have done. That's exactly the sentence the shipped database stores under `demande`, with `"I have a favour to ask you."` as its translation.

French and Spanish also carry a second-level check on top of that: `le`, `la`, `les` (French) and `el`, `la`, `los`, `las` (Spanish) double as object pronouns as well as determiners, so a match right after one of those is only trusted as a noun reading when it isn't also immediately preceded by a subject pronoun (`il le mange` — a pronoun before a verb) and isn't itself a spelling that's also a verb headword. A language with no determiner list configured skips this step entirely and always falls back to the best-taught owner, with a printed warning that says so.

**Headwords of several tokens.** A sentence is read as tokens of letters, so a headword that is several tokens (`à travers`, `tout de suite`, `peut-être`, `aujourd’hui`) can never equal one of them. These are matched as runs of consecutive tokens instead. A verb phrase also takes the spellings of its verb: `avoir chaud` is found as `j’ai chaud`, `nous avons chaud` and `tu avais chaud`, and a reflexive verb takes the pronoun that agrees with the form (`s’en aller` is found as `je m’en vais`, never as `je t’en vais`). A reflexive verb is only found in a simple tense or the infinitive, since the pronoun of a compound tense sits before the auxiliary. A run is scored like a form, by how rare it is. On the French build this took multi-word headwords with an example from 1 to 5,178 of 10,248.

A hyphenated headword is matched on its parts, so `peut-être` also finds `il peut être`.

Once every candidate sentence for a word has a final score (length/balance plus the best form-rarity bonus it earned), they're sorted highest first, and the top `--per-word` distinct sentence texts are kept — Tatoeba can assign two different sentence ids to the same text, so text, not id, is what gets deduplicated. For `manger`, the sentences that came out on top were:

```
example                                      translation
Si on ne mange pas, on meurt.                If you don't eat, you die.
J'ai mangé le quart du gâteau.                I ate a quarter of the cake.
Mangez-vous du riz dans votre pays ?          Do you eat rice in your country?
```

Each row is one word, one sentence, one translation — there is no sense column, because Tatoeba's sentence pairs carry no information about which meaning of a word they use, only that the word appears.

## 6. LLM merge (optional)

If an LLM artifact exists at `--llm-artifact` (default `<lang>/<lang>-dictionary.db.llm.jsonl` in the `ikonika-llm` checkout beside this repository), `merge_llm_results.py` replaces the matched word's senses and translations with the artifact's own, gives every new meaning all the categories of the senses it replaced, and sets `words.llm_complete = 1`. The artifact is generated separately and offline by `llm_generate_definitions.py`, through a local OpenAI-compatible server, the claude command or the codex command (`--backend server|claude|codex`); the driver only merges an existing file, it never generates one. Skipped with a warning if missing, in which case every sense keeps its gloss and no definition.

The German driver reads the two-pass layout instead. `generate_definitions_foreign.py` writes `<lang>/<lang>-dictionary.db.meanings.jsonl`, the gist and the target-language definition of each meaning, once per language. `map_translations_native.py` writes `<lang>/<lang>-dictionary.db.<native>.mapping.jsonl` once per language pair: it attaches the translations to the meanings and writes the native definition and the example with its translation. `--meanings` and `--mapping` hand them to `merge_llm_results.py`, which joins a meaning to its mapping by gist. A meaning the mapping gives no translation is not written, a reading the mapping lacks keeps its source senses, and a translation the mapping dropped or left unmatched is deleted. With `--meanings` alone the driver writes the meanings with no translation bound to them, builds the decks and stops, which is the database `llm_deck_gists.py` needs before the mapping exists. The flow is: build, meanings pass, meanings-only build, deck gists, mapping pass, full build. `split_artifact.py` in `ikonika-llm` turns a legacy artifact into the two files without a model call.

A reading the LLM found no meaning for, because none of its translations fit the headword, has those translations deleted, and so has every meaning of the reading left with none. Step 4b's second run then removes the reading when nothing else translates it. In the French build the LLM found no meaning for 1,027 readings, and 1,013 of them are dropped, mostly junk readings such as `tous` as a noun.

`llm_generate_definitions.py` doesn't send the LLM a word. It sends a reading: one headword, one part of speech, one gender. A word whose senses cover two genders — a noun that is masculine in one meaning and feminine in another — produces two separate readings, sent as two separate prompts, each carrying only the translations that belong to senses of that gender. A word with translations and no senses is sent as one reading without a gender. `words.llm_complete` already marks which words are done, so a reading whose word is flagged is skipped outright before anything else runs.

Which translations a reading carries is where `verified` does its second job. For a reading's gender, the script pools every translation attached to a sense of that gender with every translation still sitting unattributed on the word, and keeps a candidate only if it isn't a corpus translation at all (gloss, wiktionary and dictionary rows always pass), or it's a corpus translation with `verified = 1`. A build where the corpus only measures has no corpus rows, so every translation passes.

Every meaning the LLM returns carries a gist, a definition in the target language, the same definition in the native language, and an example sentence that can only be read in that meaning, with its native translation. The native language is the one most translations in the database are written in. The LLM puts a translation that fits no meaning of the headword into a dropped list. A response is rejected when it uses a translation that was not sent, compared after the same folding the merge uses. Every translation sent has to appear in a meaning or in the dropped list, except on a word with more than 10 translations, where fewer than 20% may go missing; the generator records those as missed. The merge deletes dropped and missed translations from the word. An example that contains no form of the headword is reported, not rejected, since the forms table misses pronominal verbs and many plurals. A rejected reading goes back into the queue: each next pass halves the batch size, down to one reading per request, and the run stops when a pass of single readings resolves nothing new. The readings left are printed with their last rejection reason. The script refuses to append to an artifact written before these fields existed.

The merge stores the native definition, the example and its translation on the sense. A gist that repeats within the reading gets a number (`saisir 2`). The record below predates the native definition and the example.

The real artifact record for `manger`/verb:

```json
{"headword": "manger", "part_of_speech": "verb", "gender": null,
 "meanings": [{"gist": "consommer",
               "definition": "Action d'ingérer des aliments pour se nourrir.",
               "translations": ["to eat"]}]}
```

Before deleting anything, the merge reads every existing translation the word has, under any sense, keyed by a folded, "to "-stripped form of its text — so both the gloss row `"to eat"` and the corpus row `"eat"` are captured under the same key `"eat"`; rows are read out in probability order, so the corpus row's measurement is the one kept for that key. Only then does it delete the word's old senses, and all of their translations, outright. For each meaning in the artifact, it writes a fresh sense with the LLM's `definition` and `gist` and `gloss` left empty — a build has one or the other, never both — and, for each translation text the LLM listed, folds it the same way and looks up whether that key was captured a moment ago. If it was, the *old* translation is reinserted under the *new* sense, carrying forward its original `source`/`count`/`probability`/`rank`/`verified` untouched. If it wasn't — the LLM proposed a translation nothing else ever measured — it's dropped outright; merge never invents a brand-new translation string. A meaning that loses every one of its translations this way is deleted whole, its definition included, since a definition with nothing to translate it against is not a card. For `manger`/verb, the artifact's `"to eat"` folds to the same key as the existing `eat`/`corpus` row, so that row — not a new `to eat` row — is what gets reattached to the new sense, measurements intact. The shipped database after this merge shows exactly that — one sense, one translation, carrying the corpus's own measured count and probability under the LLM's chosen definition:

```
sense.gist       sense.definition                                  translation  source  count   probability
consommer        Action d'ingérer des aliments pour se nourrir.    eat          corpus  361358  0.8777
```

## 7. Sentence tree

`build_sentence_tree.py` builds the index the sentence exercises query: for every Tatoeba pair, the set of dictionary word ids a learner must know to write the sentence, stored as a prefix tree. It runs after the second `drop_untranslated_words.py`, because it stores `words.id` values and nothing after it may delete a word. The French, Spanish and English drivers call it; English reads the Ukrainian side of Tatoeba and `en_core_web_md`. How the app filters the stored sentences at session time is in [sentence-selection.md](sentence-selection.md).

```
python build_sentence_tree.py <dictionary.db>
    --target-sentences fra_sentences.tsv.bz2 --native-sentences eng_sentences.tsv.bz2
    --links links.tar.bz2 --target-lang fr --native-lang en
    [--model fr_core_news_md] [--min-words 2] [--max-words 14]
    [--processes 4] [--limit N] [--stats out.json] [--dump out.tsv] [--dropped drops.tsv]
```

Spanish passes `--target-lang es --model es_core_news_md` with `spa_sentences.tsv.bz2`. The grammar rules and the per-language token rules live in tables keyed by language code, so a new language needs a table entry and a spaCy model and nothing else.

The walkthrough follows Tatoeba sentence 1848574, `Je veux savoir qui vous a dit cela.` / `I want to know who told you that.`

**Meaning examples.** `add_meaning_examples()` adds the example sentence of every LLM meaning, with its translation, as one more pair. A dictionary without the columns adds none. Their ids follow every Tatoeba id, so where both have the same text the Tatoeba sentence is kept. The row stores `source_id = NULL`.

**Pair selection.** `choose_pairs()` sorts the linked pairs, keeps the first translation of each target sentence, keeps one sentence per distinct text once case, punctuation, apostrophes, hyphens and spacing are ignored, and keeps only sentences of `--min-words` to `--max-words` letter tokens (2 to 14). French: 723,290 sentences, 436,862 translated pairs, 353,541 kept. Spanish: 441,233 sentences, 282,902 translated pairs, 248,295 kept. The build keeps 2-word sentences; the app asks for 3 or more.

**Tagging.** spaCy runs with the parser and NER disabled, `nlp.pipe(batch_size=1000, n_process=--processes)`. Each token contributes its text, lemma, UPOS and `PronType`. Nothing else from spaCy is used.

**Token resolution.** `resolve_token()` turns a token into a set of `words.id` values, or drops it:

1. Dropped when the UPOS is one of `PUNCT SYM SPACE PROPN X`, when the text holds a digit or a period, when it is a language's filler token (French `t` in "a-t-il"), or when the folded spelling is one of the language's free tokens: French `ne`, Spanish `no`, `al`, `del`, English `the`, `a`, `an`. French elisions (`l'`, `qu'`) are first expanded to the full word. A `PROPN` is kept as a word when its folded spelling is a headword, a word form, or a verb form with enclitic pronouns attached, and it is either lowercase or the first word of the sentence: "Demain" is kept, "Rends" in "Rends-moi mon sac" is kept as "rendre", a lowercase "cría" tagged as a name is kept as "criar". A capitalised `PROPN` inside the sentence is always a name, and so is a `PROPN` with no dictionary entry. English also treats a sentence-initial `PROPN` as a name (`initial_name_is_name`), since Tatoeba English opens a fifth of its sentences with "Tom" and the dictionary has "tom" as a noun. A name that is also a word form, such as a sentence-initial "Maria" (a form of "marier"), is kept as that word.
2. The suggestion is every word whose headword equals spaCy's lemma. For a function UPOS (`PRON DET ADP CCONJ SCONJ PART`) any part of speech counts; for the others the part of speech must match the UPOS. The candidates are the suggestion plus every word that owns the folded surface form in `words` or `word_forms`. When nothing owns the spelling, Spanish strips up to two attached pronouns (`me te se lo la le los las les nos os`) and looks for a verb.
3. No candidate at all: the token is unknown and drops the whole sentence, whatever its capitalisation. A token the tagger calls a name never gets here, it is dropped in rule 1. Candidates whose part of speech is `name character symbol suffix phrase proverb prep_phrase` are removed; none left drops the token.
4. The attested candidates are those with a translation whose stems meet the stems of the native sentence (`native_stems()`: lowercase tokens minus stopwords, each also with `s`, `ed`, `ing`, `ies` stripped).
5. Choice: suggestion ∩ attested; else the suggestion when its best teaching rank beats the best attested rank; else the attested, preferring those whose headword is the spelling itself. With no attested candidate: the words whose headword is the spelling when they are better taught than the suggestion; else the suggestion; else every candidate.
6. Readings sharing a headword collapse to the best taught, and at most three remain (`MAXIMUM_ALTERNATIVES`).
7. The token is dropped when one of the remaining readings is a free word.

A free word is decided per headword when the index loads. A headword is free when its best-taught reading is an article, determiner, contraction or particle. It is also free when it can be a pronoun, preposition or conjunction and none of its readings is ranked in the frequency list under a learnable part of speech. A ranked pronoun, preposition or conjunction is not free: the app teaches it as a card, so a sentence with it needs it known. French frees 36 headwords and Spanish 107.

| Token | UPOS | Outcome |
| --- | --- | --- |
| Je | PRON | `je`, teaching rank 4 |
| qui | PRON | `qui`, rank 300 |
| vous | PRON | `vous`, rank 48 |
| cela | PRON | `cela`, rank 1,170 |
| veux | VERB | `vouloir`, rank 246 |
| savoir | VERB | `savoir`, rank 288 |
| a | AUX | `avoir`, rank 10 |
| dit | VERB | `dire`, rank 195 |

Rule 5 is what corrects the tagger: in "Je suis content", spaCy lemmatises "content" as the verb "conter" (rank 48,702), the translation "happy" attests the adjective "content" (rank 510), and the adjective wins. 110,208 of 2,137,474 resolved French tokens were corrected this way; 1,214 stayed ambiguous.

**Paths.** `expand_paths()` takes one word per token in every combination, drops the sentence when that exceeds 16 combinations (`MAXIMUM_PATHS`), deduplicates each combination and sorts it by `(-teaching_rank, id)`, so the rarest word comes first. The example has one path: `cela, qui, savoir, vouloir, dire, vous, avoir, je`. A sentence with an ambiguous token gets several paths ending at the same sentence id, and a learner who knows any one of them can write it. French: 351,391 paths over 350,187 sentences, 5.84 words per path, 1,204 sentences with more than one path. Spanish: 247,476 paths over 244,724 sentences, 4.83 words per path, 2,641 with more than one.

**Drops.** A sentence leaves the build for one of four reasons: a token with no dictionary word (French 6,452, Spanish 4,826), a vulgar word in the sentence or its translation (1,441 and 891, a third of a percent), no required word left after the free words are removed (86 and 68), or more than 16 path combinations.

**Tense keys.** A conjugated verb needs more than its word: the learner must have trained that verb in that tense. `verb_tenses()` in `sentence_grammar.py` gives each verb token the tense codes it needs, the same codes `word_forms.tense` and the app's conjugation deck use:

| Verb form | Tense codes needed |
| --- | --- |
| infinitive | none |
| present, imperative | 1 |
| imperfect | 2 |
| past simple | 3 in Spanish, where the deck has it; every deck tense in French |
| future | 4 |
| past participle after a present auxiliary, or on its own | 8 |
| past participle after an auxiliary in another tense | every deck tense |
| conditional, subjunctive, present participle | every deck tense |

The deck tenses are `GRAMMAR[language]["deck_tenses"]`: 1, 2, 4, 8 for French and 1, 2, 3, 4, 8 for Spanish. They must match the tense table in `core/src/FTWordForms.cpp`.

**Regular presents.** A verb whose present is regular needs no present key, because the learner who knows the infinitive can derive the form. `is_regular_present()` in `sentence_grammar.py` builds the six present indicative forms from the infinitive, using the endings in `GRAMMAR[language]["regular_present"]`, and accepts the verb only when every form is in `word_forms` for that person and number. French has the `-er`, `-ir` (`finissons`) and `-re` (`vendons`) patterns, with `manger` and `commencer` keeping their sound in `nous`. Spanish has `-ar`, `-er` and `-ir`. A verb that breaks the pattern, such as `aller`, `prendre`, `acheter` (`achète`) or `pensar` (`pienso`), keeps its key, and so does any verb with a form missing. French has 4,510 of 7,031 verbs on the regular list and Spanish 3,297 of 5,924. Only a sentence whose verb needs the present alone is relaxed; a compound tense still asks for every deck tense.

`verb_tense_keys()` turns each code into a tense key, `tense × word span + word id`, where the word span is the highest `words.id` plus one and the word id is the verb's canonical id. `expand_paths()` adds the keys to every path that takes the verb reading of the token, right after the verb itself. A path that takes another reading of the same spelling, such as the noun "donne", gets none. The example's path is `cela, qui, savoir, vouloir, vouloir·1, dire, dire·8, vous, avoir, avoir·1, je`: "veux" and "a" need the present, "dit" needs the passé composé, "savoir" is an infinitive.

**Gap.** `pick_gap()` records where the first path's first word, the rarest, sits in the text: the first token resolved to it that is alphabetic and at least two letters long. Offsets are UTF-16 code units, because the app indexes the sentence as a `QString`. The example stores `gap_start = 30`, `gap_length = 4`, the word "cela". French has 6,749 sentences with no usable gap and Spanish 277; they store NULL.

**Changed count.** The number of resolved tokens whose folded spelling is not the headword of any word they resolved to. The example scores 3: "veux", "a", "dit".

**Grammar level.** `sentence_grammar.py` grades the sentence 1 to 4. `make_grammar_token()` gives each token three facts from the dictionary:

- `readings`: when the token resolved to a verb, the union of `reading_of(mood, tense)` over every verb row in `word_forms` that owns the spelling, plus `infinitive` when the spelling is a verb headword.
- `auxiliary`: "être" or "avoir" when the token resolved to one of them.
- `participle_of`: the verbs for which the spelling, or the spelling minus a final `e`, `s` or `es`, is a past participle. This is set even when the token resolved to an adjective, which is how "entendu" in "J'ai entendu" is still seen.

`reading_of()` maps the `word_forms` codes:

| mood, tense | Reading |
| --- | --- |
| any, 5 | infinitive |
| any, 8 | past_participle |
| any, 6 or 7 | present_participle |
| 3, any | conditional |
| 4, 0 or 1 | imperative |
| 2, 2 | subjunctive_imperfect |
| 2, other | subjunctive |
| 0 or 1, 3 | past_simple |
| 0 or 1, 4 | future |
| 0 or 1, 2 | imperfect |
| 0 or 1, 1 | present |

`classify_sentence()` then collects features in one pass over the tokens:

- A token is a compound participle when it follows an auxiliary within three tokens (`AUXILIARY_REACH`) with only `ADV` or `PRON` tokens between, and it can be a past participle. After "avoir" that is enough. After "être" the verb must be one of the motion verbs listed in `GRAMMAR["fr"]`, or a clitic pronoun must stand before the auxiliary. This keeps "Il est fatigué" out and "Il est venu", "Il s'est levé" in. It adds `past_participle`, and `compound` when the auxiliary's own reading is not `present`.
- Any other verb token takes one reading: `infinitive` when the spelling can be one, otherwise the easiest finite reading by level, then by the order of the feature table. "vis" is the present of "vivre" before the passé simple of "voir"; "parle" is present before subjunctive. The spelling is the same, so the learner writes the same letters.
- `object_pronoun`: `le la les l' lui leur me m' te t' se s'` tagged `PRON` and followed by a verb or another pronoun; `nous` and `vous` only when a pronoun precedes them.
- `adverbial_pronoun`: `y` or `en` tagged `PRON`, except in "il y a".
- `relative`: `dont`, `lequel` and its forms.
- `negation`: French, the negator `ne` without `pas`. Spanish, any of `nunca jamás nada nadie ninguno ninguna ningún tampoco ni`; a lone `no` is level 1.
- `question`: the sentence contains a `?`.
- `subordinate`: two or more finite verbs together with a clause marker, which is an `SCONJ` token, a pronoun with `PronType=Rel`, or `s'` before `il` or `ils`. Two clauses joined by `et` do not count.
- `inversion`: a `PRON` token that starts with a hyphen and follows a verb.
- A token resolved through Spanish enclitic stripping adds `object_pronoun`.

The level is the highest level among the features, and `features` is the sum of their bits. A language can override a feature's level in `GRAMMAR[language]["levels"]`; Spanish moves `past_simple` and `present_participle` to 2 and `subjunctive_imperfect` to 3, so it has no level 4.

| Feature | Bit | Level |
| --- | --- | --- |
| present | 1 | 1 |
| infinitive | 2 | 1 |
| imperative | 4 | 1 |
| past_participle | 8 | 2 |
| imperfect | 16 | 2 |
| future | 32 | 2 |
| object_pronoun | 64 | 2 |
| negation | 128 | 2 |
| inversion | 256 | 2 |
| question | 512 | 2 |
| conditional | 1024 | 3 |
| subjunctive | 2048 | 3 |
| compound | 4096 | 3 |
| present_participle | 8192 | 3 |
| adverbial_pronoun | 16384 | 3 |
| relative | 32768 | 3 |
| subordinate | 65536 | 3 |
| past_simple | 131072 | 4 |
| subjunctive_imperfect | 262144 | 4 |

Length and vocabulary play no part in the level.

The example: "veux" is present, "savoir" infinitive, "a dit" a participle after "avoir" in the present, "vous" an object pronoun after "qui", and "qui" marks a clause between two finite verbs. `features = 1 + 2 + 8 + 64 + 65536 = 65611`, level 3, although every verb in it is among the 300 best taught.

Verb tenses are read from `word_forms` and not from spaCy because `fr_core_news_md` labels "avais", "serais" and "chanta" all as `Tense=Pres`.

| Level | French | Share | Spanish | Share |
| --- | --- | --- | --- | --- |
| 1 | 121,966 | 35% | 76,996 | 31% |
| 2 | 142,514 | 41% | 117,841 | 48% |
| 3 | 71,134 | 20% | 49,887 | 20% |
| 4 | 14,573 | 4% | 0 | 0% |

**Tree.** `build_tree()` inserts every path into a trie keyed by word id, then renumbers the nodes breadth first with each node's children in ascending word id, so a node's children are contiguous. `serialize_tree()` writes the format 3 blob, which carries the word span, described in [dictionary-schema.md](dictionary-schema.md#sentences-sentence_tree). French: 1,793,704 nodes, 15,892 of them children of the root, 23,281,432 bytes. Spanish: 1,091,485 nodes, 14,019 children of the root, 14,335,228 bytes.

**Output.** The step drops and recreates `sentences`, replaces the single `sentence_tree` row, and writes its counters to `<target>.sentence-tree.json`. `--dropped` writes one row per dropped sentence to `<target>.sentence-drops.tsv` with the id and name of the filter that dropped it; the drivers pass it, and the filters are listed in [sentence-selection.md](sentence-selection.md#filters-in-the-build). `--dump` writes one line per sentence with its level, features and resolved words, which is how the grading was checked. With the default four processes a French run takes 208 seconds and a Spanish run 144.

**Form sentences.** After the tree is written, `form_sentences.py` picks two sentences for every form the forms training asks, a conjugated verb or a noun or adjective form, and writes them to `form_sentences`, described in [dictionary-schema.md](dictionary-schema.md#form_sentences). It takes one entry per headword and kind, the translated one first, and builds its forms table the way the app does. A noun or adjective whose forms all follow the regular rules gets no card in the app, so it gets no sentences either, and neither does a form spelled like its headword. It finds the word's sentences in the tree by every word id its headword has, because the tree keys a headword shared by a noun and an adjective, or a noun and a verb, under one id. It keeps the sentences the app would accept and files each under every form it fits. For a verb the fit is a copy of the app's rule: the form's spelling, its subject before or after it, and the third person for a French verb with no subject. A noun or adjective form fits wherever it stands. Four checks go past the app's rule. The word the app blanks must have been read as the word of the form earlier in this step, so "para" in "motivo para parar" is not taken for parar. Where that reading also holds another part of speech, as with "ciegas", spaCy must also have tagged the word with the form's part of speech; for a verb, so must a word right after an article, as in "o parto". spaCy's tag is not required everywhere, because the French model tags many verbs as adjectives or nouns. A simple tense right after an auxiliary is a participle, as in "Elle a éconduit", and is not taken for a present. The blanked text must not show again in the sentence, as in "Él lloró y lloró". A Spanish or Portuguese third person singular present that opens its clause with no subject is skipped when the translation opens with an English verb, because "Asume el mando." / "Take over." is a command to tú. spaCy marks such commands as indicative, so the translation is the only sign. A verb sentence that speaks the pronoun ranks first, then the lower level, then the shorter text. The forms with the fewest candidates pick first, and a sentence goes to one form across the whole dictionary. Spanish: 15,772 verb forms, 651 noun forms and 118 adjective forms, 25,784 rows. French: 15,192 verb forms, 737 noun forms and 2,045 adjective forms, 27,982 rows.

Tests: `python -m unittest test_build_sentence_tree test_sentence_grammar test_form_sentences` in `scripts/dictionary`. None needs spaCy or a dictionary. The core test tagged `[conjugation][gap][slow]` opens every dictionary in `data/built` and checks that the app finds the form of every `form_sentences` row in its sentence.

## 8. Decks

`build_decks.py` writes the read-only decks from hand-kept TSVs, after the sentence tree, when no later step deletes a word. Each driver passes the deck files of its language that exist: the topic decks `overrides/<lang>-topic-decks.tsv` and the exam decks below. Decks are listed in the order the files are given, so the exam decks come last.

French has no official DELF word list, but the DELF and DALF exams are pegged to the CEFR levels, and the dictionary grades readings by level from FLELex. So the French driver first runs `build_level_decks.py`, which writes one deck per level, DELF A1 to DALF C2, each holding every learnable reading of that level by teaching rank, into `<target>.exam-decks.tsv` under the group Examens. A learner preparing for B1 works through A1 and A2 as well. Spanish does the same for DELE under Exámenes, from ELELex levels; ELELex grades no reading C2, so Spanish has no DELE C2 deck. English does the same for IELTS A1 to IELTS C2 under Exams, from CEFR-J levels and the Octanove C1 and C2 lists. These replaced the IELTS General and IELTS Academic decks made from the examword lists.

The Expressions deck, the last deck of each topic deck file, holds 250 multi-word units a learner meets early and cannot work out from their words: phrasal verbs and verbs with a fixed preposition in English (give up, look forward to), pronominal verbs and verb phrases in French (se souvenir, avoir besoin de), verb phrases in Spanish (tener que, echar de menos), and prepositional and adverbial phrases in all three (à travers, a pesar de, by the way). The list was written by hand, not mined, and each entry was matched to the dictionary as a reading of its own: an entry the dictionary lacked was replaced by another until 250 matched, and an entry that only matched a plain verb, such as levantar for levantarse, was left out, since Spanish files most pronominal verbs under the bare verb. The 250 best taught are kept, by teaching rank. Rows name no gist, so a deck word is the whole reading. The deck is Expressions in the group Basics for English, Expressions in Bases for French, Expresiones in Básico for Spanish, and Expressões in Básico for Portuguese. The Portuguese dictionary lists almost no pronominal verbs and few phrases, so its deck leans on prepositional and adverbial phrases, and 256 of the 700 entries tried matched. No review makes it, so `assemble_topic_decks.py` keeps its rows as they are when it rewrites the file.

```
python build_level_decks.py <dictionary.db> <decks.tsv> --names "DELF A1,DELF A2,DELF B1,DELF B2,DALF C1,DALF C2" --category Examens
```

```
python build_decks.py <dictionary.db> <decks.tsv>... [--gists <deck-gists.jsonl>] [--report <report.tsv>]
```

The TSV columns are `row_id`, `deck`, `category`, `headword`, `part_of_speech`, `gist` and `context`. The category is the group the deck screen filters by, since the deck screen lists only decks with a group. Rows keep their `row_id` order within a deck, and decks keep the order they first appear in. A row whose headword and part of speech match no reading is left out and written to `<target>.deck-drops.tsv`, as is a repeated row.

A row's gist comes from the first of three sources that names a meaning the reading has: the deck gist artifact, the row's own `gist` column, and the reading's only meaning when it has one. A row none of them settles teaches the whole word. A gist the reading lacks is skipped for the next source and written to the report, so a rebuild that renames a meaning keeps the word in its deck.

The English driver no longer reads `data/raw/en/en-decks.tsv`. `convert_ielts_decks.py` made that file once from the examword IELTS lists (`data/raw/en/ielts-words.tsv`). The General list carries no part of speech, so a General row took the best-taught learnable reading of its headword, or its best-taught reading of any part of speech when none is learnable: the dictionary files country names such as `france` as names. An Academic row kept its part of speech, unless the dictionary had no such reading and only one reading of the headword. Headwords matched whatever their case, so `britain` found `Britain`. A headword the dictionary lacked was tried without hyphens, with hyphens as spaces, and as an inflected form. The 71 rows still unresolved, mostly joined compounds, misspellings and American spellings the dictionary lacks, kept their source spelling for a hand fix and are listed in `data/raw/en/en-decks.unresolved.tsv`. The file is edited by hand from here on.

### Topic decks

Every language pair shares one list of 32 topic decks in 7 groups, such as Body, Family and Emotions in People, or Food, House and Health in Daily Life. `overrides/topic-decks.tsv` lists each deck with its group, English name and a one-sentence scope, and `overrides/topic-groups.tsv` names the groups. A deck name avoids "and": Work and Education became two decks. A language shows its own names from `overrides/topic-names.<lang>.tsv`, so the French deck reads Santé in the group Vie quotidienne. The decks are made offline in three steps, against a built dictionary, and the result is the `<lang>-topic-decks.tsv` the driver reads.

**Problem.** Every source of topic words is noisy. A Wiktionary category or topic label usually comes from one meaning, often a rare one: `fenêtre` is labelled anatomy for a part of the ear. Topic lists hold every word that appeared in a topic text, so `acheter` and `poisson` sit in Health. An earlier attempt that took categories at face value put `optométriste` in a medicine deck. No rule over these signals separates a topic word from a word that merely occurs near the topic, so a reviewer decides.

1. `collect_topic_candidates.py` gathers candidates. A candidate is a learnable reading up to teaching rank 25,000 that a source puts in a deck: a Wiktionary category or topic label mapped onto the deck in `overrides/topic-categories.tsv`, from the English edition (language prefix removed), the native edition or either edition's sense labels; a hand-made topic list such as the French `data/raw/topics/topic-words.tsv`, whose codes `overrides/fr-topic-list.tsv` maps onto decks; or an extra list a model wrote for the words a deck misses, such as months, numbers and furniture, in `overrides/<lang>-topic-extra.tsv`. An extra word skips the rank cap, since a word like `après-midi` can rank low when its counts are split across spellings. It writes one numbered file per deck, each line with the reading's rank, CEFR level, evidence and every meaning as gist and definition.
2. A reviewer judges every candidate by the rules in `scripts/dictionary/topic-deck-review.md`: keep a word when one of its meanings belongs to the topic and a learner could need it, give the gist of that meaning when the word has several, and mark it core or useful. When the deck list moved from 20 decks to 32, the old review carried over: a word kept in an old deck stayed kept in the new deck that inherited its topic, a word an old split deck kept in two new decks was sorted by hand, and only the new topics (House, Time, Math, Geography, Science) were reviewed from scratch. English reuses the French review instead of making its own. A deck teaches how to say a set of things, so `translate_topic_decks.py` takes each kept French word, translates only the meaning the deck kept (fraise in Food gives strawberry, never the drill), and keeps the first translation the English dictionary has as a learnable reading. The result was read by hand, and the bad translations, such as chief for chef cuisinier, are listed in `overrides/en-topic-exclude.tsv`, so the next translation is tried in their place. The script's output goes to the collector as a translated list. A hand reading of the English candidates by teaching rank added the words French does not cover, and basic words no source tags, such as most months and numbers, came from a hand-written `overrides/en-topic-extra.tsv`. English ends with 6,243 words to the French 6,492; the gap is mostly French compounds English has no entry for, such as viande rouge. A bad translation is listed with the French word it came from, because son is wrong for enfant and right for fils.

Spanish reuses the French review too, through English, since the Spanish dictionary translates into English and the French one has no Spanish. `translate_topic_decks.py --via en` takes the English words of the kept French meaning and finds the Spanish meaning that has one of them among its first three translations, or as its gloss; among several, the lowest translation rank plus the tenth logarithm of the teaching rank wins, so husband gives marido and not hombre, and child gives niño and not retoño. Every other English word of the French meaning that the Spanish meaning also translates takes half a rank off, so tarte, pie and tart, gives tarta and not pizza. A Spanish translation past the third, or one the corpus never ranked and no gloss gave, is tried only when nothing else matches; without it pectoral, sida and resfriado found nothing. The Spanish meaning found becomes the row's gist. The English exclusions also apply to the pivot, and `overrides/es-topic-exclude.tsv` lists the Spanish pairs read and rejected by hand, with `*` dropping a French word none of whose translations fits. The French words the pivot still left without a Spanish word were translated by hand, read against the English of the kept meaning, and the 302 that the Spanish dictionary has as a learnable reading are listed in `overrides/es-topic-extra.tsv` with the gist of the meaning that fits, such as agujetas for courbature and tumbona for chaise longue. No Spanish row is reviewed again: `review_translated_decks.py` keeps every translated and extra candidate as core with the gist its list gives. Spanish ends with 5,396 words. About 380 French words have a Spanish word the dictionary lacks: compounds like hermano mayor and zumo de naranja, and single words like sobrina, sorprendido and dron. Spanish also loses words where several French words collapse into one common Spanish word, and French dishes like cassoulet have no Spanish name.
European Portuguese is carried over from the French review the same way, through English, with `overrides/topic-names.pt.tsv` for its names. Every one of the 4,940 rows the pivot first gave was read by hand: 422 wrong picks went into `overrides/pt-topic-exclude.tsv` as pairs, such as espetacular for méchant and foguete for roquette, and the 179 whose next pick was wrong too were dropped with `*`. The French words still without a row were translated by hand, and the 339 the Portuguese dictionary has are listed in `overrides/pt-topic-extra.tsv`. Portuguese ends with 5,068 topic words. It loses more than Spanish because its dictionary is smaller: about 670 hand translations, such as sobrinha, namorada, brócolos and sumo de laranja, have no entry. Portuguese has no exam decks.
Italian follows the same French-to-English-to-Italian route, with names in `overrides/topic-names.it.tsv`. The translation review excludes wrong English-pivot matches in `overrides/it-topic-exclude.tsv`; `*` drops the French source word rather than trying another unreviewed translation. Correct Italian replacements and additional everyday words are kept in `overrides/it-topic-extra.tsv`. The assembled file, `overrides/it-topic-decks.tsv`, has 4,957 entries across the 32 topics, followed by 250 hand-selected Espressioni under Basi. Expressions include common reflexive verbs, verb phrases, greetings and adverbial phrases, matched to exact dictionary readings and ordered by teaching rank. Italian currently has no definition or gist artifact, so its deck rows teach whole readings. The Italian driver imports these decks after the sentence tree and before learnable readings and compaction. It adds no exam decks.

To regenerate the Italian topic rows against the current dictionaries, run from the repository root. The assembler preserves the existing Espressioni rows in the output file:

```
python scripts/dictionary/translate_topic_decks.py data/built/fr/fr-dictionary.db data/built/it/it-dictionary.db local/it-topic-reviewed.tsv --names scripts/dictionary/overrides/topic-names.fr.tsv --via en --via-exclude scripts/dictionary/overrides/en-topic-exclude.tsv --exclude scripts/dictionary/overrides/it-topic-exclude.tsv --report local/it-topic-unmatched-reviewed.tsv
python scripts/dictionary/collect_topic_candidates.py data/built/it/it-dictionary.db local/it-topic-candidates --language it --translated local/it-topic-reviewed.tsv --extra scripts/dictionary/overrides/it-topic-extra.tsv --gloss-column gloss
python scripts/dictionary/review_translated_decks.py local/it-topic-candidates local/it-topic-review --translated local/it-topic-reviewed.tsv --extra scripts/dictionary/overrides/it-topic-extra.tsv
python scripts/dictionary/assemble_topic_decks.py data/built/it/it-dictionary.db local/it-topic-candidates local/it-topic-review scripts/dictionary/overrides/it-topic-decks.tsv --names scripts/dictionary/overrides/topic-names.it.tsv --report local/it-topic-assembly-report.tsv
```

A changed dictionary or French source deck can produce new Italian choices. Review those choices and the unmatched report before replacing the checked-in deck file; the translated-review tool accepts its inputs without judging them again.

German uses this same English-pivot workflow against the local German-to-English dictionary. Substitute `de` for `it` in the Italian commands and use `overrides/topic-names.de.tsv` for the assembly names. The reviewed inputs are `overrides/de-topic-exclude.tsv` and `overrides/de-topic-extra.tsv`; the former drops unsuitable source matches, and the latter supplies exact German readings for corrections and additional everyday vocabulary. The resulting `overrides/de-topic-decks.tsv` contains 5,034 entries across the 32 topics and 250 Redewendungen under Grundlagen. The Expressions deck includes fixed phrases, greetings, adverbial phrases and common separable verbs, ordered by teaching rank; the assembler preserves its existing rows. The German driver imports the decks before learnable readings and compaction, and accepts the optional `ikonika-llm/de/de-dictionary.db.deck-gists.jsonl` artifact. With no German definitions or gists yet, the initial decks teach whole readings. A replacement absent from the local dictionary is omitted rather than replaced by another unreviewed translation. No exam decks are generated.

3. `assemble_topic_decks.py` turns the kept numbers into the deck TSV, `overrides/<lang>-topic-decks.tsv`, which is in git because a review made it and no later step can remake it without one. Each deck lists its core words first, then its useful ones, each by teaching rank, so the deck view opens on the words a learner expects. A kept word whose headword does not match its candidate is dropped, and a gist the reading lacks is cleared; both are reported.

```
python collect_topic_candidates.py <dictionary.db> <candidates-dir> --language fr --wiktextract <en-fr.jsonl> --native-wiktextract <fr-native.jsonl> [--topic-list <topic-words.tsv> --topic-map <fr-topic-list.tsv>] [--extra <fr-topic-extra.tsv>]
python assemble_topic_decks.py <dictionary.db> <candidates-dir> <review-dir> overrides/<lang>-topic-decks.tsv --names <topic-names.fr.tsv> --report <report.tsv>
python translate_topic_decks.py <fr-dictionary.db> <en-dictionary.db> <translated.tsv> --names <topic-names.fr.tsv> --exclude <en-topic-exclude.tsv> --report <report.tsv>
python translate_topic_decks.py <fr-dictionary.db> <es-dictionary.db> <translated.tsv> --names <topic-names.fr.tsv> --exclude <es-topic-exclude.tsv> --via en --via-exclude <en-topic-exclude.tsv> --report <report.tsv>
python collect_topic_candidates.py <es-dictionary.db> <candidates-dir> --language es --translated <translated.tsv> --extra <es-topic-extra.tsv>
python review_translated_decks.py <candidates-dir> <review-dir> --translated <translated.tsv> --extra <es-topic-extra.tsv>
```

**Trade-offs.** The reviewer gives the decks their quality, which no source does alone. The cost is that the decks depend on one review pass: a rebuilt dictionary keeps them as long as their readings survive, and a row that loses its reading falls out at build time. A word no source tags never becomes a candidate, so recall is bounded by the sources. A word can sit in several decks, and decks overlap where topics do: Transport and Travel share the airport.

### Deck gists

**Problem.** A deck word with several meanings should teach the one its deck is about: fraise in Food is the strawberry, never the drill. The French review chose a gist for most such words, and Spanish and Portuguese took theirs from the French meaning through the pivot. English never got any, since its translation step keeps a word and not a meaning, and no review chose a meaning for the Expressions decks.

**Decision.** Gists live in an artifact beside the definitions artifact, `ikonika-llm/<lang>/<lang>-dictionary.db.deck-gists.jsonl`, one line per deck, headword and part of speech. The gists the reviews chose were moved there from the deck TSVs and are marked `review`. `llm_deck_gists.py` in ikonika-llm fills the rest. It needs nothing from this repository: `fetch_dictionary.py` downloads the latest released dictionary, which holds the decks and the gists, and `deck-scopes.tsv` there is a copy of every deck's scope under each language's deck name. It sends each topic deck row whose reading has several meanings and no gist the reading has, up to 40 words a request across decks, and the LLM names one gist or none. Its lines are marked `llm`. A later line for the same row replaces an earlier one. The drivers pass the artifact to `build_decks.py` when it exists. Exam decks have no scope and are not sent: a CEFR level is not a topic, so their rows get a gist only when the reading has one meaning. A renamed or added deck needs its line in `deck-scopes.tsv`.

```
python fetch_dictionary.py en
python llm_deck_gists.py en --backend codex --model gpt-6-luna --reasoning low --parallel 3
```

**Trade-offs.** A deck word teaches one meaning, even where two fit, as hôte does for host and guest in Relations; the deck screen matches a card to a deck word by headword and part of speech, so a second row would show the same card. The artifact names meanings by gist, so it stays right only while the definitions artifact does; a gist that a new definitions pass renames is reported by the build, and the next gist pass sends that row again.

## 9. Learnable readings

`build_learnable_readings.py` runs `learnable_readings.sql` after the decks and before compaction, in all four drivers. It writes the `learnable_readings` table and the word range slider's two metadata rows, described in [dictionary-schema.md](dictionary-schema.md).

```
python build_learnable_readings.py <dictionary.db>
```

**Problem.** A new words run starts by counting the learnable words of a CEFR level per part of speech, then loads the first candidates. On a dictionary without this step, both are queries over most of the word table: nothing indexes the level, and each word's best translation is chosen by sorting all of its translations. On French that is about 180 ms on the UI thread before the screen can open, plus about 50 ms to work out the slider's upper end and level marks on launch.

**Decision.** The step works the same answers out once at build time. The table holds one row per learnable reading with its best translation, so a run's count and candidates are indexed lookups, and the slider reads two stored values. The app keeps its queries for a dictionary built before this step.

**Trade-offs.** The table repeats data the dictionary already has: about 3 MB on French, 2% of the file. It is only right while it matches the app's own definition of a learnable reading and of the best translation. The core tests run `learnable_readings.sql` against a generated dictionary and compare the app's answers with and without the table, so a change on either side that breaks the match fails there. The slider's upper end is stored for 1,000 words, the count the app asks for; an app asking for another count queries it.

## 9b. Forms flags

`build_forms_flags.py` runs `FormsFlagsTool`, a console target of `core`, after step 9 and before compaction, in all six drivers. It writes `words.has_forms` and the `forms_flags` metadata row.

```
python build_forms_flags.py <dictionary.db> --target fr [--native en] [--tool PATH]
```

**Problem.** The Today screen shows how many studied words each forms training can use. The app answered that by looking up the full entry of every studied noun, adjective and verb, to learn whether a forms card can be made from it. On a phone that cost about 0.9 s on the UI thread for a learner with a few thousand studied words.

**Decision.** The answer depends only on dictionary data, so the build computes it once. The tool runs the app's own rules (`IsFormsEligible`, which is `MakeFormsCard` for the word's kind) against the entry the app would look up, so the rules live in one place. The app reads the flags of its studied headwords with one indexed query. A headword without a flag still goes through the full lookup.

**Trade-offs.** The column adds one integer per word. The step needs `FormsFlagsTool` built, so `build-msvc.ps1` has to run first, and a change to the forms rules means rebuilding the dictionary. The core tests compare the written flags with the lookup the app falls back to.

## Scoring a build

`dictionary_metrics.py <dictionary.db>` prints the numbers below, and `--compare <other.db>`
adds what one build lost and gained against another. A change to the pipeline is judged on these
numbers, taken before and after.

```
python dictionary_metrics.py data/built/fr/fr-dictionary.db
    [--compare <other.db>] [--json <out.json>]
```

A build is scored on four questions.

**Does the learner get the word at all?** A word with no translation is dropped, so the word
count is the coverage.

| Metric | Better | What it tells |
| --- | --- | --- |
| words | higher | Cards the dictionary can offer. |
| words dropped, no translation | lower | Word-set headwords no source could translate. Read from the two drop reports beside the database. |
| words dropped from the top 1,000 / 5,000 / 10,000 | lower | The same, for the words a learner meets first. One lost word here costs more than a hundred lost in the tail. |

**Does each word have enough translations?**

| Metric | Better | What it tells |
| --- | --- | --- |
| translations, translations per word | higher | Distinct translations, counted once per word. |
| median translations per word, also for the top 1,000 / 5,000 | higher | What a typical word has. The mean is pulled up by a few words with dozens of translations. |
| words with one translation | lower | Words where an answer is marked wrong unless the learner types that one spelling. |
| translation rows from each source | - | Where the translations come from. A source that falls to zero shows a step did not run. |

**Can the translations be trusted?**

| Metric | Better | What it tells |
| --- | --- | --- |
| translations agreed by two sources | higher | `verified = 1`. Two independent sources give the same translation. |
| words with no agreed translation | lower | Words where every translation rests on one source. |
| translations with a corpus count | higher | Translations the corpus measured, so they can be ordered by how often they are used. |
| words with no measured translation, also for the top 1,000 / 5,000 | lower | Words whose translations are shown in no meaningful order. |

**Is the rest of the card intact?** These catch damage a translation change causes further down.

| Metric | Better | What it tells |
| --- | --- | --- |
| meanings with a definition, words with definitions | higher | The LLM merge keeps a meaning only when one of its translations is still in the build. A translation source that disappears takes meanings with it. |
| translations bound to a meaning | higher | Translations shown under a definition, not loose on the word. |
| words with an example | higher | Words with a Tatoeba sentence. |
| sentences | higher | Sentences the writing exercise can use. A sentence is dropped when one of its words is not in the dictionary. |

**Is the text clean?** These catch text a source wrote for a machine and a screen cannot show as it is.

| Metric | Better | What it tells |
| --- | --- | --- |
| words with a transcription | higher | Words with an IPA string. The flip, flashcard and review screens show it under the word. |
| top 1,000 / 5,000 words with no transcription | lower | The same gap for the words a learner meets first, where a blank line is seen most. |
| translations with an underscore | zero | A source that joins the words of a translation with `_`, as `dead_man`. The import writes spaces, so a non-zero count means a source bypassed it. |
| meanings with an underscore in a native text | zero | The same in a native definition or an example translation of a meaning. |
| example translations with an underscore | zero | The same in a Tatoeba sentence translation. |

The comparison counts words lost and gained, and translations lost and gained on the words both
builds have. Translations are compared folded the way the merge folds them, so `to eat` and `eat`
are one translation.

## 10. Placement word lists

The placement test asks from a fixed list per language pair, `app/placement/<target>-<native>.tsv`, shipped inside the app. The test reports a teaching rank as the learner's cutoff, and the app applies it to the downloaded dictionary, so a rank in the list that differs from the dictionary's puts the cutoff at the wrong word. After any rebuild that changes teaching ranks, restamp every list:

```
python scripts/dictionary/build_placement_words.py data/built/<lang>/<lang>-dictionary.db <native code> app/placement/<target>-<native>.tsv --restamp
```

Restamp keeps the reviewed words and rewrites their ranks. A word is the main reading of its spelling when its part of speech has the lowest frequency rank. A listed word whose part of speech is not the main one moves to the main one if its best translation still qualifies, and is dropped otherwise.

## Flags

- `--native-jsonl PATH`: overrides step 3's input. Step 3 itself is not optional — there is no flag to skip it.
- `--llm-artifact PATH`: overrides step 6's input. The German driver takes `--meanings PATH` and `--mapping PATH` instead.
- `--mwe-frequency-file PATH`: fixed at `data/raw/<lang>/opus/<lang>-mwe-frequency.tsv` by both drivers; threaded through `build_dictionary.py` → `build_fr_master_wordset.py`/`build_es_master_wordset.py` → `generate_word_set.py --extra-frequency-file`. Optional at every layer; omitting it just means no multi-word candidates reach the word set.
- `--lemmatizer PATH` (`build_dictionary.py`): the native lemmatizer, fixed at `data/raw/<lang>/wiktextract/<lang>-lemmatizer.tsv` by both drivers; threaded to step 1's `--lemmatizer-file`. Optional; omitting it just means no inflected-form filtering happens, so a form like "leurs" gets its own row same as before this existed.

## Not part of this pipeline

- `build_dictionary.py --llm-results` and `--counts`: present in its argparse, called by neither driver. `--llm-results` expects a retired artifact shape; `--counts` would call `fill_translation_probabilities.py`, superseded by step 4.
- `fill_translation_probabilities.py`: only updates `probability` on rows that already exist, never inserts. Reachable only through the unused `--counts` flag above.
- `fill_translations_opusmt.py`: a local neural-MT filler (ONNX OPUS-MT model), unrelated to the OPUS-corpus steps above despite the name. Not called by either driver.
- `build_opus_frequency.py`: an alternative frequency source, not used by either master-wordset script.
