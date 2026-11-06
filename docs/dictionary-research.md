# Dictionary sources

This document outlines dictionary requirements for Ikonika app.

see [dictionary-pipeline.md](dictionary-pipeline.md) for build steps

## Requirements

1. Word
2. Definition per sense (foreign language)
3. Translation per sense, sorted by use frequency
4. Part of speech
5. Gender - for article recognition/exercises
6. Use-frequency and CEFR grade - allows to present appropriate words based on learner level
7. Examples (foreign and native language)
8. Synonyms (grouped per sense)
9. Phonetic transcription (nice-to-have)

## Sources in use

| Requirement | French | Spanish | English |
| --- | --- | --- | --- |
| Words, gender, phonetics | wiktextract, English edition | wiktextract, English edition | --- |
| Translations | English glosses + OPUS corpus | English glosses + OPUS corpus | --- |
| Definitions | LLM pass | LLM pass | --- |
| Translation ranking | OPUS OpenSubtitles en-fr | OPUS OpenSubtitles en-es | --- |
| Frequency | Lexique 3.82 | EsPal | SUBTLEX-US |
| CEFR | FLELex (Beacco) | ELELex | CEFR-J 1.5 + Octanove |
| Examples | Tatoeba | Tatoeba | --- |
| Idioms, extra synonyms | fr-native cross-reference | es-native cross-reference | none |

**wiktextract / kaikki.org** — JSON Lines, one object per word. Free, bulk, no licensing blocker.
The English edition is a primary source for French and Spanish db. 
Reasons: 
- has much less definitions than fr-native and es-native, 
- allows to produce much higher translation coverage for top 10k,
- requires llm pass to invent senses and match translations to them

French and Spanish edition are used for their respective db to fill missing info: genders, forms, synonyms, more translations.
Forms coverage in wikiextract datasets allows us to build lemmatizer to map OPUS-MT corpus align results from forms to lemmas (e.g. running/ran -> run)

**OPUS OpenSubtitles v2024** — parallel subtitle corpus, word-aligned locally with eflomal. Supplies
additional translations, and P(translation | headword). One per language pair. Requires lemmatizer.
[translation-probabilities.md](translation-probabilities.md).

**Tatoeba** — human-written sentences with human translations. Three exports: target sentences,
English sentences, and link table. Quality uneven, developed an algorithm to select best sentences for a headword + POS.

**Lexique 3.82** — French frequency lists, word + POS frequency grading. 
Advanced algorithm converts "frequency rank" to "teaching rank" based on headword exposure in books and films

**EsPal** — Spanish subtitle frequency, form-level. Its lemma and part-of-speech tool turns it into one frequency per lemma and part of speech. 

**FLELex** — French CEFR, CC BY-NC-SA 4.0, word + POS cefr levels. 
Headword receives a combined cefr level, that is measured based on frequency in each A1-C2 level, provided by the dataset.

**ELELex** — Spanish CEFR, same cumulative-band rule as FLELex.

**SUBTLEX-US** — English subtitle frequency, form-level, Ghent University. 
**CEFR-J 1.5** — A1-B2 per reading, free with citation, Tono Laboratory TUFS; 
**Octanove** extends it to C1/C2, CC BY-SA 4.0.

**Local LLM** — uses set of translations to invent definitions and assigns each translation to a definition. Produces a JSONL artifact. 
I treat LLM as a "fill the gaps" stage rather than "invent from scratch". I hope to keep that moving forward.

## Rejected

**DBnary** — unpredictable coverage

**Kaggle dictionnaire-français** — 1.26M rows, CC BY-SA 4.0, from Wiktionnaire via WiktionaryX. No
translation column, and 67% of rows are inflected verb forms rather than headword senses.

**Open English WordNet** — no translation data.

**FreeDict** — too small.

**Apertium** — MT transfer rules, not learner-facing content. No EN-UK pair.

**NGSL** — 2809 public-domain English lemmas covering ~92% of general text. Superseded by SUBTLEX,
which measures per form and covers the long tail. Still on disk.

**Verbiste** — French conjugation templates. Forms already provided by wiktextract. 

**Oxford 3000/5000, COCA** — not freely usable at bulk scale.

**Commercial dictionary APIs** (Oxford, Collins, PONS, K Dictionaries, Merriam-Webster) — never verified, probably per API-request cost.

## Machine translation

**Opus-MT (Helsinki-NLP)** 300 MB per language pair, Apache 2.0.
