# Translation probabilities

Ranking a French word's translations in each native language by how likely each one is, mined
from a parallel corpus. Used to order the translation list in the UI instead of showing
dictionary entries in arbitrary order.

## Data source

OPUS OpenSubtitles v2024. One language pair per native language, all aligned against French.
The French-English slice is in use; French-Ukrainian and any later pair each need their own
run of this pipeline. Human subtitle translations are much cleaner than machine-translated
corpora. The en-fr raw archive holds 83.9M sentence pairs; the cleaning pass keeps 72.86M
pairs (both sides alphabetical, length matched, high-confidence filters).

## Pipeline

1. Clean raw pairs with the preprocess script. Output is two aligned files, one line per
   sentence pair.
2. Word-align with eflomal (MSVC port, built locally). Driven by the align script, which
   tokenizes sentences into integer ids, runs the aligner, then counts every aligned word pair.
3. Counts are raw: `<target>  <fr>  <count>`. One row per pair of aligned words.
4. Normalize each French word's counts to probabilities. Output is
   `<fr>  <target>  <prob>  <count>`, sorted by French word then probability descending.
   Probabilities sum to 1.0 per French word.

### Duration per 1M sentence pairs

| Step | Time |
| --- | --- |
| Clean | tens of seconds |
| Tokenize | ~3.5s |
| Align (eflomal) | ~7s |
| Count aligned pairs | ~4s |
| Normalize | ~5s |

## Joining to the dictionary

`fill_translation_probabilities.py` runs after the LLM completion pass, which fills
translations the corpus cannot rank. The corpus holds inflected surface forms; the dictionary
stores lemmas, so the join goes through the word_forms lemma mapping on the French side. Counts
are summed per (headword, translation) and normalized. Probabilities attach to existing
translation rows, one per language. See docs/dictionary-pipeline.md for the whole build.

## Output quality

- Rare words collapse to a single pair, giving them probability 1.0. Keep the count column and
  apply a low-count filter when joining to the dictionary.
- Numeric and time tokens leak in (0, 10:00). Filter non-alphabetic French types out at join
  time.
- OpenSubtitles occasionally contains non-French characters. Mostly inconsequential at scale.
- Lower native-language volume means sparser probabilities and more unranked (NULL)
  translations, which sort last in the UI.
