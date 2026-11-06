# Sentence selection and difficulty

How a Tatoeba sentence becomes a sentence exercise, which sentences are thrown away on the way, and how difficulty is decided. Written for developers. The mechanism is the same for every language; only the grammar tables differ.

Two stages do the work. The dictionary build filters and grades every sentence once, offline. The app filters again each time a session starts, against the words the learner knows.

## Build stage

`scripts/dictionary/build_sentence_tree.py` reads Tatoeba and writes the `sentences` table and the `sentence_tree` blob into the dictionary. See [dictionary-pipeline.md](dictionary-pipeline.md#7-sentence-tree) for the step itself.

### Filters in the build

| Filter | Drops a sentence when |
| --- | --- |
| No translation | It has no English link. Only the first translation of each sentence is used. |
| Duplicate text | Another sentence with the same words was already kept. Case, punctuation, apostrophe style, hyphens and non-breaking spaces do not count, so "Au temps pour moi." and "Au temps pour moi !" are one sentence. The lowest sentence id stays. |
| Length | It has fewer than 2 or more than 14 letter tokens. |
| Unknown word | A token the tagger does not call a name matches no dictionary word and no word form. Capitalisation does not matter. One such token drops the whole sentence. |
| Empty | Every token was dropped, as punctuation, a name, a number or a free word, so the sentence needs no known word at all. |
| Too ambiguous | The readings of its tokens multiply to more than 16 combinations. |
| Repeated word | The same word appears three or more times in a row, with only spaces or hyphens between, regardless of case. This drops word puzzles like "Can-can can-can can can can can can-can." and laughter like "Ha ha ha". Two in a row stays, since "nous nous" and "had had" are ordinary grammar, and repeats split by a comma like "No, no, no!" stay too. A second check runs after tagging: the same word appears three or more times anywhere in the sentence, regardless of case and punctuation. That drops "Vaya, vaya, vaya." and "Love loves to love love." Articles, determiners, pronouns, prepositions, conjunctions, particles, auxiliaries, numbers and the negation words of the language ("ne", "pas", "no", "não", "not") do not count, so "La durée de la passion", "Es cierto que es joven, pero también es sabia" and "No sé por qué Tom no vino y no quiero preguntarle" stay. |
| Vulgar | The sentence or its translation holds a vulgar word. See [Vulgar words](#vulgar-words). |

The build writes `<dictionary>.sentence-drops.tsv` next to the dictionary: one row per dropped sentence with the filter id, filter name, Tatoeba id, text, translation and the detail of the drop (for filter 4, the unknown words with their tags). The filter ids follow the table order, 1 to 8. The per-filter counts also go into the `.sentence-tree.json` statistics file under `filters`.

### Vulgar words

Each language has a hand-made list in `scripts/dictionary/overrides/<language>-vulgar-words.tsv`. A word on it is either vulgar or ambiguous. An ambiguous word has an everyday meaning next to the vulgar one, like "queue" or "baiser".

A sentence drops in three cases:

- One of its tokens can only be a vulgar word. A token counts by the dictionary words the build resolved it to, so every inflection counts, and "fous" read as the adjective "fou" does not count as "foutre".
- Its translation holds a vulgar word of the native language's list. A translation is not lemmatized, so that list spells out every form. This also catches slang the target list misses, such as "Tout le monde s'en bat la race" translated as "Nobody gives a fuck".
- An ambiguous or vulgar target word meets an ambiguous native word. "Ma queue est énorme" with "My cock is huge" drops, "Fais la queue" with "Wait in line" stays, and "un coq chante" with "a cock is crowing" stays, since "coq" is not on the list.

The lists were built from the Wiktionary senses tagged vulgar, offensive or derogatory of the words that occur in Tatoeba sentences, then reviewed by hand. The tags alone cannot decide: "cul" and "merde" have clean senses too, and "braquemart" has no tag at all. Plain sexual vocabulary like "sexe" or "préservatif" is not on the lists. The Ukrainian list only checks the translations of the English dictionary, so it was collected from the spellings in Tatoeba's Ukrainian sentences.

The word examples of step 5 skip a vulgar pair the same way. That step has no tagger, so a token counts by every word its spelling is a form of, and a hyphenated spelling the dictionary knows, like "pique-nique", counts as one word.

A token that is dropped does not drop the sentence. It just stops being a requirement. A token is dropped when it is punctuation, a symbol, a name, a number, a per-language free token, or a free word.

A free word is a word the learner is never asked to know before seeing a sentence with it. Articles, determiners, contractions and particles are always free. A pronoun, preposition or conjunction is free only when it has no frequency rank, which means it is not in the teaching list. A ranked pronoun, preposition or conjunction is a normal required word, because the app teaches it as a card.

### What a sentence needs

Each remaining token resolves to one dictionary word, or to up to three alternatives when the choice is ambiguous. The choice prefers the part of speech the tagger suggested, then a reading whose translation shares stems with the English sentence, then the better teaching rank. Every combination of alternatives is a path. A learner who knows every word of any one path can write the sentence.

A conjugated verb adds tense keys to its path. A tense key means that the learner has trained that verb in that tense. Infinitives add none. A verb in the present also adds none when its present follows the regular pattern for its ending, so knowing the verb is enough: "Il parle" opens with `parler` known, "Il va" still needs `aller` trained in the present.

Paths are stored sorted with the rarest word first. The gap word for the missing-word exercise is the first word of the first path, so it is always the hardest word the sentence needs. A sentence with no usable gap stores none.

### Difficulty level

The build grades each sentence from 1 to 4 by the hardest grammar feature in it. `sentence_grammar.py` collects the features from the tokens and from the dictionary's own verb forms, not from the tagger's tense labels.

| Level | Features, French table |
| --- | --- |
| 1 | present, infinitive, imperative, inversion |
| 2 | past participle, imperfect, future, object pronoun, negation beyond the plain form, any question |
| 3 | conditional, subjunctive, compound tense on a non-present auxiliary, present participle, adverbial pronoun, relative pronoun, subordinate clause |
| 4 | past simple, imperfect subjunctive |

A language can move a feature to another level. The Spanish table moves past simple and the present participle to level 2 and the imperfect subjunctive to level 3, so Spanish has no level 4.

Length does not enter the level. A short question is level 2 and a ten-word statement in the present is level 1. Vocabulary does not enter it either. A sentence made only of very common words is level 3 if it holds a subordinate clause.

The level is stored per sentence and again per reference inside the tree, so the app can filter by level without reading the sentence rows.

## App stage

`SentenceTrainingController::FindCandidates` runs on every session start and on every change of difficulty or sentence count.

1. Known words: headwords of cards on the ladder or completed, plus words answered as known in Training, resolved to dictionary word ids. Known verb tenses from conjugation cards become tense keys.
2. Tree walk: a sentence qualifies when every key on one of its paths is known. There is no limit on how many word keys the path holds.
3. Level mask: Easy keeps level 1, Normal keeps level 2, Hard keeps levels 3 and 4. The picker is a set, so the levels combine.
4. Sampling: the walk keeps a random sample of at most 15 times the session length, so it never builds the full match list.
5. Usable filter: the sentence must have a gap, a translation, at least 3 words and at most 80 characters.
6. Balanced pick: candidates are ordered by word count, then by how many of its words are not in headword form. The session takes the run of consecutive candidates whose word counts, then inflection counts, differ the least. Equally good runs are chosen at random, and the candidates are shuffled first, so the same sentences do not win every session.

A session starts only when the pick fills the whole session length. Otherwise the start fails and the ready count stays below the session length.

### Logging

The `ikonika.session` category records each decision:

- every candidate search: known words, known verb tenses, usable count, how many the walk sampled, whether that was the whole pool or the cap, and the rejections by reason (`no gap`, `no translation`, `too few words`, `too long`);
- every pick: how many of the candidates were chosen and their `sentences.id` values;
- whether a start reopened the running session, resumed a saved one, or dropped a saved one that no longer fits the dictionary.

The file is `Ikonika.log` in the app's `files/logs` folder; on Android that is `/sdcard/Android/data/com.goldenphasmid.ikonika/files/logs`. Two sessions that show the same ids in a row are a repeat; a `reopened` or `saved ... session` line before them means the session was resumed, not picked again.

Difficulty is set only by the level mask. An earlier version also kept only sentences needing 3 to 6 known words; it was removed because a sentence of an easy grammar level rarely needs many words.

## Session sentence stage

`BuildSessionSentenceStage` picks the sentences of the two sentence phases of the mastery stage in Training and Review when a session starts. It is separate from `FindCandidates` above: it picks for the session's words, not for a difficulty level. A resumed session reuses the sentences saved in its checkpoint and picks nothing.

Each word gets at most one sentence. Three passes run in this order, and a word that a pass covered is skipped by the next:

1. Meaning example. A card with a meaning uses that meaning's example. A card without one uses the first meaning of its headword that has an example. The pass needs a dictionary with meaning examples.
2. Known-word sentence. The tree walk is the one from the app stage, with the known words extended by every session word, and with the present-tense keys of the session's verbs. Only sentences that contain the word qualify.
3. Phrase search. A card whose word holds a space or an apostrophe, and that is still uncovered, is searched for in the sentence text, up to 40 candidates.

A candidate in the second and third pass must also pass the usable filter of step 5 above. When the native language is English, its translation must mention one of the card's translations. For a card with a meaning, the translations of that meaning count as well.

In the second pass each word keeps up to half the session's word count of candidates, at least one, and four times that when the translation check runs, since the check discards most of them. Candidates are shuffled, then ordered by how many of the session's words they hold, most first. Words with the fewest candidates pick first. Each word takes its first candidate whose text is not yet in the session. In the third pass the candidates are ordered by level instead, lowest first.

A sentence is used once per session. The card id of a sentence comes from its sentence id, so a repeat would merge into one card with one counter. The sentence is credited to the word it was picked for, even when it holds other session words.

A word ends the three passes without a sentence when:

- its meaning has no usable example, and
- no sentence made of known words contains it, or none passes the translation check, or the only ones are already taken, and
- it is not a phrase, or no sentence contains the phrase.

The same happens for every word when the dictionary or the sentence tree is not loaded. A session where no word has a sentence has no sentence phases. Otherwise the picked sentences are shuffled and become the cards of the two phases, which run over the same list. Each picked sentence records the word it was picked for, and the checkpoint keeps that word, so an answer to a sentence counts for that word.

Every pass logs its counts to `ikonika.session`: the examples, known-word sentences and phrase sentences picked, the words that already had an example, and the known-word sentences left out because their translation missed the card's meaning.

## What this means for coverage

Qualifying sentences depend on the learner's own cards, so the same dictionary gives different counts per learner. Closed-class words such as prepositions, pronouns and conjunctions are required, so a learner with none of them in the deck sees almost nothing.

Measured on the Spanish build:

| Stage | Sentences |
| --- | --- |
| Tatoeba target sentences | 441,233 |
| With a translation | 260,841 |
| Within 2 to 14 tokens, deduplicated | 248,295 |
| Dropped for an unknown word | 7,774 |
| Dropped as empty | 71 |
| Kept in the index | 240,450 |
| Every word known, usable | 230,510 |

The last row is the ceiling for a learner who knows every word and every tense: 52% of all target sentences, 88% of the translated ones and 96% of the indexed ones. It counts sentences of at least 3 words, the app's minimum. By difficulty it splits into 69,808 easy, 112,123 normal and 48,579 hard.

About a third of the index is level 1, half is level 2 and a fifth is level 3.

A backup with 930 known headwords and no closed-class words qualifies 153 easy, 67 normal and 0 hard sentences.
