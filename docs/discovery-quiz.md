# Discover

Part one is the reference. Part two explains the decisions behind it; read it before changing a rule.

Prototype: [design/mobile/fragment-discover.html](../design/mobile/fragment-discover.html). It does
not show every rule below.

Discover replaces triage entirely.

---

# Part one: Reference

## Overview

A round is a quiz of 10 to 15 questions, 12 by default, over one or two decks of one category. Each question asks one meaning of one word.

## Terms

| Term | Meaning |
| --- | --- |
| **Meaning** | Headword, part of speech, gender and gist. A dictionary entry (headword + POS) has one or more. |
| **Category** | A group of decks, `decks.category`: Vie quotidienne, Personnes, Bases, and so on. |
| **Level estimate** | The learner's level in log teaching rank. Same model as the placement test. |
| **Most common meaning** | The entry's top meaning by the dictionary's meaning sort. |
| **Outcome** | Known, Almost or New. Set by the answer. |
| **Decision** | Learn or Know. Defaults from the outcome. |

## Categories and decks

Discover replaces triage, so it starts everywhere triage started:

| Started from | Opens on |
| --- | --- |
| A deck on the Decks screen | The round, over that deck alone. An unfinished round over the same deck continues; an unfinished round over other decks is dropped. |
| The Discover tile on Today, under Conjugations and Forms | The unfinished round, or deck selection when there is none |
| Train, when the pool cannot fill the session | Deck selection |
| Anywhere else | Deck selection |

Deck selection shows one category's decks with the last choice selected. The learner can change the
decks, at most two, or go back and pick another category. The first time, it opens on the category
pick. The choice is kept per language, and Start saves it.

The heading above the covers is centred. Categories and decks are deck covers, wrapped in rows, with no hover animation. A third pick replaces
the earliest one. A deck with every word in My Vocabulary or known is dimmed and cannot be picked.
Enter starts, Backspace goes back to the categories, Esc leaves.

| Category | Decks | Meaning asked |
| --- | --- | --- |
| Topic categories | Topic decks | The row's gist |
| A1–A2 | DELF A1, DELF A2 | Most common meaning |
| B1–B2 | DELF B1, DELF B2 | Most common meaning |
| C1–C2 | DALF C1, DALF C2 | Most common meaning |

The three level categories split today's Examens at build time, through `build_level_decks.py
--category`.

A row without a gist asks the most common meaning.

An entry is skipped when it lacks data: no meanings, no translations, or no example in the meaning
asked. It is also skipped when the meaning already has a card or a known mark, and when its teaching
rank is at or below the placement cutoff: nothing below the cutoff is ever asked.

The question card shows the deck name. The deck fixes the meaning. A headword is asked at most once
per round, even when two ticked decks hold it.

## Question types

| Type | Shows | Options |
| --- | --- | --- |
| Word | French word with article, spoken | Four native translations |
| Sentence | The meaning's example, word underlined | Four native translations |
| Beginner | The example's native translation, its span swapped for the French word | Four native translations |

Outside beginner mode the part of speech sets the type:

| Part of speech | Type |
| --- | --- |
| Noun, interjection | Word |
| Verb, adjective, adverb, preposition, phrase, conjunction | Sentence |

## Beginner mode

Every question is a Beginner question:

> I'm eating a nice ripe **pêche** for dessert.
> peach · apricot · plum · raspberry

> Our team **a gagné** the match last night.
> to win · to lose · to play · to train

The French span replaces the native span. The app finds both when it builds the question. The
French span is the first of the word's dictionary forms found in the example. The native span is the
first of the meaning's translations found in the example's translation, in any English form:
"won" matches "to win". For a verb, both spans take in the auxiliaries in front, and the French one
also takes a reflexive pronoun, so "a gagné" replaces "won" and "s'est levé" replaces "got up". The
French span leaves out a noun's article. Options stay in dictionary form. A meaning whose spans are
not found is not asked in beginner mode. About nine in ten deck meanings have both.

When every word of the example but the asked one is known, the question is a Sentence question
instead: the French example with the word underlined. Known follows sentence training: learned cards,
known marks, words under the placement result, essential words, and the present tense of known verbs. The asked word
counts too, in the present tense when it is a verb.

The answer shows the word, the French example and its translation.

- **Entry.** The learner knows under 1,000 words: the placement result plus the learned cards and
  the known marks. No placement result counts as zero, the same as a placement result of zero.
- **Outcome.** A right answer is Known. A first wrong answer is struck out and strikes
  out wrong options until two are struck, the same as Not sure; the next wrong answer reveals the
  answer as New. Neither moves the estimate.
- **Decision.** Every answer defaults to Learn while beginner mode is on.

## Options

Exactly one option is right. It is the translation the example's translation uses, not the
meaning's first one: "hallway" for `couloir` when the example says "hallway". When the example's
translation uses none of them, it is the meaning's first translation.

| Type | Excluded from wrong options |
| --- | --- |
| Word, Beginner | Every translation of the word, in any meaning |
| Sentence | The asked meaning's translations. Up to two wrong options are the word's other meanings. |

Translations compare case- and accent-folded, without a leading "to " or parentheses. No two options
compare equal.

Wrong options share the part of speech and come from the eight words nearest the asked word's
teaching rank, same deck first. Prepositions, interjections, phrases and conjunctions draw from every
topic deck.


## Not sure

Not sure is a fifth option, centred under the four. Each press gives the next clue. After the last
clue it becomes Show me, which reveals the answer as New. A wrong answer before the sentence shows
flashes red, is struck out, and shows the sentence. A wrong answer with the sentence on screen reveals
the answer as New. Not sure with the sentence on screen strikes out wrong options until two are
struck, counting the wrong answer. The card keeps room for the example under
the word, so a clue that shows it moves nothing.

| Type | First clue | Second clue |
| --- | --- | --- |
| Word | Example sentence, word spoken again | Two wrong options struck out |
| Sentence | Two wrong options struck out | |
| Beginner | Two wrong options struck out | |

## Picking the next word

Curve: `GetPlacementKnownChance`, P(known) = 1 / (1 + exp((ln rank − L) / 0.6)), with L over 200
steps from rank 20 to 60,000.

**Prior.** Centred on `Placements.CutoffRank` with spread 0.8. Without a placement result, centred on
rank 400 with spread 2.0.

**Update.** First answer on a word, Beginner questions excluded:

| Outcome | P(outcome \| known) | P(outcome \| unknown) |
| --- | --- | --- |
| Known | 0.97 | 0.25 |
| New | 0.03 | 0.40 |
| Almost | no update | |

**Pick.** From the ticked decks' askable words above the placement cutoff, take the three with P(known) nearest 0.5 and pick
one at random.

## Outcomes and the reveal

| Answer | Outcome | Default decision |
| --- | --- | --- |
| Right, no clue | Known | Know |
| Right after a clue or a wrong answer | Almost | Learn |
| Wrong with the sentence on screen, or Show me | New | Learn |

Outside beginner mode, Continue after a right answer with no clue moves straight to the next
question with the default decision, Know, and shows no reveal.

An answer, right or wrong, stays on screen with the right option marked. The example sentence shows in full
with its translation under it. Nothing moves on by itself: Not sure turns into Continue, same size
and place, and Continue or Enter opens the reveal.

The reveal replaces the options. Above the divider: the word with its article, the phonetic, and the
card's translation in a field, set to the right option. Below it: the meaning as triage shows it, with
its definition, example and translation chips; a chip adds or removes a translation. Under the card,
Know on the left and Learn on the right, the default filled. Either one moves on. Only the word is
spoken. No other sound, no animation.

The round runs in the session screen, so it has what every exercise has: a word in a sentence opens
the translation popup, Ctrl+F opens the lookup pane, Ctrl+Space plays the word, and the forms popover
shows the revealed word's forms.

## End of a round

The round ends after its last question or when the decks run out of askable words. Leaving keeps the
round unfinished, to continue later; it lives in memory, so closing the app loses it. End session ends
it. Answers given are kept either way.

## Data read

Dictionary: `decks` (`category`), `deck_words` (`gist`), `senses` (`example`, `example_translation`,
`native_definition`, `tags`), `translations` (`rank`), `words.teaching_rank`. No dictionary change.
Trainer database: cards, `TriagedWords`, `Placements.CutoffRank`.

## Data written

When the learner moves on from a question, a Learn creates a card in My Vocabulary and a Know marks
the meaning known. The card carries the asked meaning and the translation shown as the right option.

| Decision | Write |
| --- | --- |
| Learn | A card in My Vocabulary, in the pool, with its meaning and translation filled. |
| Know | `MarkWordTriaged` for the meaning. |

**Known marks per meaning.** `TriagedWords` gains `MeaningGist`, part of the key
`(TargetWord, TargetLanguage, PartOfSpeech, Gender, MeaningGist)`, migrated in place in trainer
schema 37. Discover writes the gist of the meaning it asked, a level row included. An empty gist is a
mark on the whole entry: every pre-migration mark, and every triage mark.

`MatchDeckWords` follows. A row with a gist is known through a mark on its meaning or an empty mark,
and learning through a card of its meaning or a card with no meaning. A row without a gist is known
through any mark of its entry and learning through any card of it. `AppWorkspaceMerger` adds the
column to the row key.

## MVP

A click on a deck cover starts a round over that deck. The Discover tile on Today opens category and
deck selection. A tap on a category only marks it; the Continue button, or Enter on desktop, opens its decks. Mobile has the same flow. Its question card gives 60 percent of its height to the word and 40 to the
options. Not sure, Continue, Know and Learn stay in the button row under the card, as on every other
phone screen. The answer card is one area; swiping it left marks the word known and swiping it right
learns it, with the outline turning green or blue. The results screen lists the added words as
tiles, and a tap opens Edit card. The round length setting
is not built yet. The deck's Learn button starts a round over the deck, and Train with a short pool
opens category and deck selection, as the Discover tile does. Nothing starts triage any more.

## Code map

| Piece | Location | Role |
| --- | --- | --- |
| `FTLevelBelief` | `core` | Level estimate and update, moved out of `FTPlacementTest` and shared. |
| `FTDiscoveryPlanner` | `core` | Picks, question types, options, beginner mode and its spans, outcomes. |
| `DiscoveryController` | `app` | Plain `QObject`. Reads `FTDictionaryRepository`, writes cards and known marks. |
| `DesktopDiscoverPick` | `app/qml` | Category and deck pick, shown in place of the Today page, under the top bar. |
| `DesktopDiscoveryExercise` | `app/qml` | The round, inside `DesktopSessionScreen`. |
| `MobileDiscoverPickScreen` | `app/qml/mobile` | Category and deck pick, pushed from the Discover button on Today. |
| `MobileDiscoveryScreen` | `app/qml/mobile` | The round and its results. |

## Tests

- No option set has two right answers or two equal options, against the real French and English
  dictionaries.
- Word and Beginner questions never offer another meaning of the asked word.
- Almost answers and Beginner questions leave the estimate unchanged.
- Nothing at or below the placement cutoff is asked.
- A Beginner question whose example is known apart from the word asks the French example.
- A Beginner verb question swaps the whole verb group.
- A learner who knows no word starts in beginner mode.
- A known mark on one meaning leaves the entry's other deck rows new.
- A headword in two ticked decks is asked once per round.
- A level row's Know marks the asked meaning, not the entry.

---

# Part two: Explanation

## The problems

1. **Words have several meanings.** A learner can know `gagner` as "to win" and not as "to earn".
   Asked about the word, they say yes, and "to earn" is never offered. A typed translation cannot
   check it: several translations fit most sentences.
2. **A beginner knows nothing.** Every answer is "I don't know", which tells the app nothing and
   teaches nothing. A French sentence does not help below roughly 800 to 1,000 known words.
3. **Attention without retention tricks.** No points, streaks, hearts or celebration.

## Why meanings

Cards are per meaning, so questions are too. The deck fixes the meaning: in Nourriture, `fraise` is
the strawberry, never the drill. Other meanings live in other decks: `gagner` is "to earn" in Travail
and "to win" in Sports, `navet` a turnip in Nourriture and a flop in Cinéma. Each is asked in its own
context, so known marks must be per meaning: a Know in Travail must not hide Sports.

Exam decks have no topic and ask the most common meaning.

## Why categories and decks

A whole category is broad: twelve questions over five decks is about two words per deck. Two decks
at most keep a round focused.

## Why recognition and clues

A meaning the learner cannot pick out of four is a meaning to learn. Production is Intensive
Training's job. Multiple choice can guarantee one right answer; typing cannot.

Without a sentence, another meaning of the word is a right answer: a learner who picks "fishing" for
`la pêche` knows the word. So Word questions never offer one. In a Sentence question the other
meaning is plainly wrong, and it is the trap that catches the learner who knows only one meaning.
Difficulty comes from close neighbours: `la pêche` against apricot, plum and raspberry.

Not sure replaces "I don't know". A clue turns a dead answer into inference, the same skill reading
asks for. Almost marks partly known words, the quickest to learn.

## Why beginner mode asks in the native language

"I'm eating a nice ripe pêche" gives the meaning away. For a test that is fatal; for a beginner it is
the point: they meet the word in use instead of pressing Not sure three times. Since it measures
nothing, it leaves the estimate alone.

Spans come from the dictionary rather than from a word aligner. The meaning's translations were
themselves mined from aligned corpora, so finding one of them in the example's translation reuses
that alignment. A runtime aligner learns from a whole corpus at once: one sentence pair alone aligns
close to randomly, so it would need a corpus or a priors file per language pair on the device. The
cost of the dictionary route is that each native language needs its own word forms: English has
them now; a native language with case endings, such as Ukrainian, needs its own form matching before
beginner mode works for it.

## Why ask near 50%

An answer is most informative where the outcome is least certain, and at 50% the learner is right
often enough to keep going. Half a round goes to known words, so a round finds five to seven new
meanings, not twelve. That half still writes known marks and sharpens the estimate.

## Why two decisions

A word the learner does not want is a word they do not want again. A third "skip" choice would leave
it new and bring it back every round. Know means "do not offer this", the same as triage's known
mark.

## What holds attention

- **Every reveal teaches.** The meaning in a real sentence.
- **Nobody is locked out.** Beginners answer every question; advanced learners work at their edge.
- **Choice.** Category, decks, and stopping at any question.

Left out on purpose: points, streaks, hearts, timers, sound effects, answer animations, praise copy,
final scores, notifications.

## Decisions

**Recognition only.**
- Pro: one right answer, guaranteed. Twelve words in about three minutes.
- Con: Known overstates production. Training covers it.

**Clues instead of "I don't know".**
- Pro: the round teaches; Almost finds partly known words.
- Con: Almost can come from elimination. It goes to Learn anyway.

**Native sentences for beginners.**
- Pro: a learner with no French answers every question and meets words in use.
- Con: the answers measure nothing, so they never move the estimate.

**Ask near 50%.**
- Pro: most information per question at a fair difficulty.
- Con: half a round goes to known words.

**Learn or Know, no skip.**
- Pro: an unwanted word does not come back.
- Con: Know also covers words the learner does not know but does not want.

**Known marks per meaning.**
- Pro: knowing one meaning does not hide another.
- Con: a schema migration and a change to deck progress.
