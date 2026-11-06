# Everest dictionaries

Everest Dictionary 3.10 beta is a freeware multilingual dictionary by Vladutu
Daniel, released in 2004 for Windows 98 through XP. It ships 37 word lists
covering English, French, German, Italian, Spanish, Portuguese, Hungarian,
Romanian and Bulgarian, plus monolingual English, German, French Larousse,
French synonyms and Spanish synonyms sets. File sizes run from 72 KB to
7.8 MB. Our copy lives in `data/raw/everest_versiia_310/`, with the installer
files under `Everest310/` and a full install reference at
`E:\Program Files (x86)\Everest`. The per-file inventory with the app's own
display names is in `everest_manifest.json`.

Each list is a `.ev` file: a text file compressed with a per-file Huffman code. A copy of all
37 sits in `data/raw/everest_versiia_310/dictionaries/`.

## Format

| Bytes | Contents |
| --- | --- |
| 0 | Bits left unused in the last byte. |
| 1-4 | Size of the code table, little-endian. |
| code table | One entry per character: the character, its code length in bits, then the code left-aligned in as many bytes as it needs. |
| body | The coded text, most significant bit first. |

The decoded text is a header line, then one `headword`translation` line per entry, with CRLF
line ends. The header reads `ENFR;English French Standard;ENFR1;0;409;409;English;French;`.
A semicolon separates alternatives on either side. French decodes as cp1252, Hungarian and
Romanian as cp1250, Bulgarian as cp1251.

All 37 files decode to the last byte with no bit left unmatched.

## Extraction

`scripts/dictionary/build_everest_dictionary_translations.py` decodes the 25 bilingual volumes
and writes 26 files into `data/raw/everest_versiia_310/translations/`, named
`<headword language>-<translation language>.tsv`, in the format
`import_additional_translations.py` reads. Each volume fills two files: its own direction as it
stands, and the opposite direction inverted, where each translation becomes a headword. Where
Everest has a volume for both directions, the file pools the direct volume and the inverted one,
and a pair both give is written once.

A semicolon or a comma separates alternatives on either side, and text in brackets is dropped.
Source is `everest-2004`. Part of speech and sense are empty. The labels column carries:

| Label | Meaning |
| --- | --- |
| `technical`, `medical`, `proverb`, `expression` | The volume the row came from. |
| `inverted` | The row was read from the volume of the opposite direction. |
| `truncated` | The volume stores the field cut at its fixed width. |
| `phrase` | The translation is longer than four words. |
| `mixed` | Both alphabets on one side of a Bulgarian pair. |
| `doubtful` | The headword is listed more than once in the Spanish technical volume. |
| `clause` | The English side starts with a subject pronoun (`I eat`, `he swims`): a generated verb phrase, not a translation of a headword. 137,444 of the 390,404 Portuguese rows. |
| `acronym` | The headword is written in capitals (`POR`), so its translation is the name the letters stand for and must not reach the lower-case word. |

The 12 monolingual volumes (synonyms, definitions) are not translations and are not written.

## Review

The build writes `review.tsv` beside the files: one row per volume and one per output file.

| Pair | Translations | Verdict |
| --- | --- | --- |
| en-fr | 257,780 | Good. 76% carry no label. A quarter of the standard volume is generated verb phrases such as `did not learn` for `n'apprenait`. |
| en-de | 499,759 | Good. German nouns carry their article. |
| en-pt | 390,403 | Good, with many generated phrases such as `these durabilities`. |
| en-it | 118,799 | Good. |
| de-fr | 125,615 | Good. |
| en-hu | 158,146 | Good. The Hungarian-English volume is the English-Hungarian one inverted, so it adds 120 pairs. |
| en-es | 459,788 | Poor. The standard volume is 27,463 clean pairs. The technical volume lists 101,839 of its 237,140 entries under a repeated headword, one line with the right translation and one with a neighbouring entry's. All of them are labelled `doubtful`. |
| en-bg | 392,002 | Poor. Entries are dictionary articles with grammar marks and example sentences, not word pairs. 151,287 rows are labelled `mixed`. |
| en-ro | 589,491 | Fair. Romanian is written without diacritics. One technical volume is cut at 35 characters. |
| fr-ro, de-ro | 189,125 and 212,911 | Fair. Cut at 38 and 30 characters, no diacritics. |
| ro-es, ro-it | about 18,000 each | Good, small. |

## Caveats

Four volumes store both fields at a fixed width, and the file itself holds the cut text:
`ENFR2` at 25 characters, `ENRO4` at 35, `FRRO1` at 38, `GERO1` at 30. Nothing can restore it.
The Hungarian and Spanish volumes cap a field at 30 or 50 characters. Every row from an entry
at the cap carries `truncated`.

The French technical volume is written in capitals with no accents. The build lower-cases it,
which also lower-cases its acronyms.

Generated verb phrases match no dictionary headword, and the import step ignores them.

## The earlier memory extraction

The first extraction read entries from the running program's memory, because the format was
not known. `fr_en.json`, `build_all_pairs.py` and `everest_memparse.py` are what is left of it.
Measured against the decoded files, it missed 12,928 of 255,909 pairs (5.1%), and 11,353 of its
254,334 pairs (4.5%) are not in the files. Of those, 3,668 have the two sides swapped and 7,561
pair a word with a neighbouring entry's translation. Do not use `fr_en.json`.
