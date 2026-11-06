# Learning model

## Vocabulary

List of terminology/definitions

| Term | Meaning |
| --- | --- |
| **Card** | One thing being learned: word, translation, definition. |
| **My Vocabulary** | Every card of the language pair. There are no user decks. |
| **Deck** | A read-only list of words shipped in the dictionary. Learning a deck runs triage over its words. |
| **Box** | Leitner box, 1 upward. Lives on the card and decides when it is next due. |
| **Session** | A sequence of exercises over a fixed set of cards. All or nothing. |
| **Intensive learning** | The session type that takes cards from the pool and puts them on the ladder. |
| **Repetition** | The session type that works through cards already on the ladder. |
| **Completed** | Past the last box. Out of rotation until demoted by hand. |

Note what is **not** here. There is no lesson-level schedule, no per-exercise progress, and no word
state machine. See the superseded section at the end.

## The card

Three fields, all supplied when the card is created:

- **Word**, in the target language, with its article for nouns.
- **Translation**, in the **native** language.
(NOT IMPLEMENTED)
- **Definition**, in the **target** language. Optional, and never asked for on its own.

The definition exists because an advanced word often has no clean counterpart in another language.
Explaining it in the language being learned works better than forcing a one-to-one pair. It is always
paired with the translation rather than replacing it, since demanding a written definition for every
card would be unusable.

> spoon — ложка — utensil with a shallow bowl and a handle

Wherever the translation is shown, the definition is shown with it.

Each field records whether it came from a dictionary or was typed by the user.

**Only the word is spoken automatically.** The definition is a full sentence in the target language, and dubbing it
on every card would be too much talking.

## The card's life

A new card starts in the Learning Pool, the part of My Vocabulary not yet on the ladder.
Finishing an intensive learning session moves the words that session taught into box 1. From
there it climbs one box at a time as sessions go well, box 1, then box 2, and so on up to the last
box, after which it is marked Completed and leaves rotation.

Two things can send a card backwards. Answering "no" at a review, or letting its due date pass
without doing the session, drops it back one box rather than all the way to the pool. That is the
softer of the two classic Leitner variants.

(VERIFY)
There is one exception. A card that sits in box 1 for more than three days without a review goes
all the way back to the pool, not just down one box. A word learned intensively but never
revisited within three days did not really stick, so it gets re-learned from scratch rather than
nudged. Three days is a setting, not a fixed rule.

(NOT IMPLEMENTED)
The user can also send any card, completed or still in a box, straight back to the pool by hand.

## Sessions

A session has a **template**: a sequence of exercises and a rule for moving between them. And it
has **runtime state**: which stage it's on, and whether it's finished.

### Intensive learning

Runs on a fixed number of words, set by the training word count (default 25). The session is
filled from the Learning Pool, then from the dictionary. The pool is taken in turns from nouns, verbs,
adjectives, adverbs and every other part of speech, oldest first within each, and a word whose
spelling is already in the session is left for a later one. Only that last step asks the
user anything - triage runs when the pool cannot fill a session, and never when it can.

Words are picked when a session starts and promoted when it finishes.

Five stages, always in the same order. Two settings control most of the numbers below:
`CardsPerExercise` (default 10) and `RepeatsToMaster` (default 3). Quitting partway through
restarts the stage you were on; earlier stages don't redo.

**Stage 1, flashcards.** A single pass through the session's words in their order, both languages shown
together. A Continue button appears once every card has been shown once. Keep going past that
instead, and the draws switch to weighted random, indefinitely, until you press Continue.

**Stage 2, flip.** Graded recall. The learner sees one side, reveals the other, and answers "I knew
it" or "I forgot". The words come in batches of `CardsPerExercise`. A "knew it" adds one to the
card's count, a "forgot" takes one away, floored at zero. The next batch starts with the cards that
have the lowest count, so a forgotten card comes back in the next batch rather than at the end of a
full round. The stage ends when every card was known twice. The number is a setting, "Times known
per word", from 1 to 5.

The shown side moves with the stage's progress. The first third shows the French word, the middle
third picks a side at random, and the last third shows the translation.

**Stage 3, choose a translation.** Three exercises of `CardsPerExercise` cards each. Each screen
has one correct card and five wrong options, drawn from the whole session the same way exercise 3
already works outside intensive learning.

**Stage 4, mastery.** Draws from a pool of three exercise types: audio transcription, typing the
French word from its translation, and filling a gap in a sentence. Each batch of `CardsPerExercise`
cards picks one of the three at random, weighted 35, 30 and 35, runs it, then picks again for the
next batch, never the one that just ran. Training opens with one audio batch and picks at random
from then on. Review opens with one of each.

The gap exercise shows a French sentence with the card's word blanked out, and the sentence's
translation above it. Nothing is spoken until the answer is in, because hearing the sentence would
give the missing word away. Once the answer is in, only the missing word is spoken, since the whole
sentence would still be playing when the learner moves on to the next one. The sentence must contain the card's exact spelling, and for a noun the
gap leaves the article out. The sentence comes from Tatoeba first, made only of words the learner
already knows, then from the card's meaning example. A card with neither types its French word
from the translation instead.

Typing the translation from the French word is gone from this stage. The review card already asks
the French-to-native direction, and asking it again in writing tested the keyboard more than the
memory.

Every card carries one running count for the whole stage. A correct answer adds one, a wrong answer
subtracts one, floored at zero. Selection favours cards that aren't done yet; a card that's already
reached the count only comes up again to fill out a batch once every not-yet-done card has already
appeared in it.

The stage runs in up to three phases that share that count. The first phase is the exercise pool
above, and it ends once every card has reached `RepeatsToMaster`. The second and third phases are
the sentences. A word that has a sentence counts up to `RepeatsToMaster` plus 2, so 5 at the default
of 3, and a word without one counts up to `RepeatsToMaster`. The count on screen is the word's count,
whichever phase is running.

The sentences are the session's words in whole sentences, one per word. The sentence is the card's
meaning example, when the card has a meaning and the meaning has an example. Otherwise it is a
sentence made only of words the learner already knows, which contains that session word. Among
those, the stage prefers sentences that hold more of the session's words, and each word weighs up to
half the session's word count of its best candidates, at least one. A phrase card with neither is
searched for in the sentence text. A sentence never appears twice in one session. A word that finds
no sentence takes no part in the sentence phases, and a session where no word finds one has the
first phase only. [sentence-selection.md](sentence-selection.md#session-sentence-stage) has the full
pick.

In the second phase the learner hears each sentence and builds it from shuffled French word tiles.
In the third phase the learner hears it again and picks the words of its translation from native
tiles that include three distractors; the order of the picked words does not matter. On desktop and
mobile, typing a word and pressing Space places the matching tile; accents can be left out, and an
unambiguous start of a word is enough. Backspace on an empty slot takes the last tile back. Placing
the last tile does not check the sentence; the Submit button, or Enter on desktop, does. Enter with a
word typed places it and then submits when the sentence is full. A remembered option, Hard mode,
replaces the tiles with a text field. Every sentence has to be
answered correctly once in each phase, and each of those answers adds one to its word's count; a
missed sentence comes back later in the phase. A phase starts once every sentence of the one before
it is done.

Complete stage skips the running phase, and restart stage restarts it.

**Stage 5, speed match.** One exercise, a full pass over the whole session's words, target
language always on the left, native always on the right. The exercise splits into screens of five
pairs. The score does not matter.

Multiple choice and speed match can each be turned off in settings, separately for Training and
Review. A session started afterwards has no such stage. A session in progress keeps its stages
until it is restored from its checkpoint, for example after the app restarts. One restored inside a
stage that is now off continues at the stage after it. The sentences can be turned off the same
way, and then the mastery stage has its first phase only.

**Finishing.** The words this session taught move to box 1. Not the whole pool, and not the
whole pool: a session teaches a slice of both.

### Repetition

Every card due today goes into one pool, then gets split at random into sessions of N, default 25.
Splitting by box instead would produce a session of three cards whenever box 3 happens to hold
three cards, so it's random.

**Quick pass.** When cards from box 4 and up are due, Review asks about them first, with the review
card alone. These cards are weeks or months old, and a learner who still knows one does not need
the whole session for it. A "yes" promotes the card at once. A "no" puts the card into the review
that follows, where it skips the review card, goes through the practice, and is demoted when that
review ends. The follow-up review takes the forgotten cards first and fills up with the earlier due
cards. If the app closes between the two, the forgotten cards are still due and the quick pass
asks about them again.

A repetition session starts with a review: the French word shown, its meaning revealed, and a yes
or no answer. A "no" demotes the card one box, but the card still goes through the rest of the
session's exercises. The withheld part is the promotion, not the practice.

Every due session has to be done. Cards left undone are expired the next day and demoted, which is
the daily commitment the whole design rests on.

After the review come the mastery stage, which asks for three correct answers per card, and speed
match. Multiple choice and sentences are off by default for Review and can be turned on in its
settings. Only the review's yes or no moves a card between boxes.

### Decks

A deck is a fixed list of words, for example the IELTS General list. It lives in the dictionary, so it never syncs and the learner cannot change it. A deck word names a reading by headword and part of speech, and it can name one meaning by its gist.

Opening a deck shows its words in My Vocabulary's layout, each with its learning status. A word takes the furthest card of its reading, so any card of that headword and part of speech counts, even when the deck names another meaning: a learner who knows one meaning of a word is not asked about it again. A word the learner marked known, with no card of its reading, shows as learned. Triage also marks every word it turns into a card, so a mark older than a deleted card of that word does not count: deleting the card makes the word new again. Every other word is new.

Learn runs triage over the deck's new words, most common first by teaching rank, with no word limit. The deck view keeps the deck's own order. Each word becomes a card or is marked known, and triage ends when none is left. The cards go to the pool, so the next intensive learning session teaches them.

## The exercises

These are the building blocks a session template arranges. Every one plays the word's pronunciation.
Any exercise can be played separately, outside of session, over My Vocabulary.
Any exercise can be made infinite.
Some exercises have "score", that is combined from question results ('yes''no''skipped')

### Exercise 1 — Combined flashcard

Word, translation and definition shown together, browsed back and forth. No grading and no data. Loose
first exposure to the whole set.

### Exercise 2 — Flip

One side shown, the other revealed on demand, then "I knew it" or "I forgot". In Training the
answer feeds the stage's count, so a forgotten card comes back sooner. The answer never moves a card
between boxes.

### Exercise 3 — Multiple choice, six options

One prompt with six candidate answers. Direction is mixed per screen.

The five wrong options come from the other cards in the same set, deliberately. Telling apart cards
being learned right now is harder than telling them from unrelated vocabulary, and harder is the
point. Has scoring.

Options are deduplicated by text, because two cards sharing a translation would otherwise put the
same answer on screen twice and leave the question with no right answer. Has scoring.

### Exercise 4 — Typed word and gap fill

- **Type the word.** Translation shown, target word typed.
- **Fill the gap.** A target-language sentence with the word blanked out and its translation shown,
  the missing word typed. Silent until answered, then speaks the missing word.

Each can be turned off for Training and Review separately. Has scoring.

### Exercise 5 — Transcription from audio

Pronunciation played, spelling typed. Meaning is not involved.

Worth more in French than in English, because French spelling and French sound diverge badly. It is
possible to know a word on paper and not recognise it spoken. Has scoring.

### Exercise 6 — Speed matching

Six words in one column and six translations in the other, paired against the clock. Trains automatic
lookup rather than acquisition. Has scoring.

### Exercise 7 — Correct-Incorrect

Target word is shown, translation/definition revealed after "yes" or "no" answer. Same form and card UI as exercise 2 - flip. Has scoring.

## Exercise length

A **setting, independent of how many cards the session holds.** Default 10 Cards. Cards are drawn
without repeats, and a card only appears a second time once every other card has appeared, so a small
set fills the count by starting another shuffled pass rather than by clustering.

## Scoring

Applies to the graded exercises. Flashcard records nothing.

Each card is worth up to **30 points**. A buffer of N seconds runs on presentation, and answering
inside it pays the full 30. After the buffer the value counts down one point per second, to zero.

| Outcome | Points |
| --- | --- |
| Correct inside buffer | 30 |
| Correct after buffer | 30 minus elapsed seconds |
| "Don't remember" | 0 |
| Incorrect answer | −30 |

**There is no pass mark.** A run reports its score against the maximum and nothing more.

The gap between "don't remember" and a wrong answer is deliberate. Admitting ignorance costs nothing.
Confidently producing the wrong word costs the full value, because that is an actively incorrect
memory rather than an absent one.

### Buffer lengths
* configurable in code, for each exercise type.

## Typed answer comparison

Applies to exercises 4 and 5, in order:

- **Case is ignored.**
- **Whitespace is ignored**, including internal spaces. A space is accepted where a hyphen belongs.
- **Accents are not required.** `le cafe` passes for `le café`, reported so the real spelling can be
  shown.
- **The article is mandatory.** `chien` for `le chien` is wrong.

Two consequences, both deliberate. Gender is tested, because the article is required. And accent
leniency cannot distinguish words differing only by an accent, so `ou` and `où` are interchangeable.
A test pins that behaviour down so it stays a decision.

When the learner types a translation, which is the native language, two more rules apply. A wrong
answer there is most likely a slip of the keys and not a gap in knowledge, so it is not punished.

- **The opening words are enough.** `look` passes for `to look at`. Only the opening words count, so
  `at` does not pass.
- **One letter may be wrong.** A letter inserted, dropped or replaced: `louk` and `looke` pass for
  `look`. This applies to words of four letters or more, because one letter apart is a different
  word among short ones, `cat` and `car`.

Both rules apply to each alternative of a translation on its own. Typing the target word keeps the
exact comparison, since its spelling is the thing being learned.

After a wrong answer or "Don't know", the right answer stays on screen and the learner types it
again before moving on. Only the first answer is scored; the retype is not, however many tries it
takes. A screen that moves on by itself after a right answer also moves on after a right retype.

Both rules are flags, so either can be reversed without touching the comparison code.

## Boxes and intervals

Leitner, with **configurable intervals**. Six boxes by default, with gaps of 1, 2, 4, 14, 40 and
120 days. A word comes back on days 1, 3, 7, 21, 61 and 181 after it was learned. The sixth box is
a check-up half a year later, and a "yes" there marks the card Completed.

The update from three boxes to six reopens cards completed in box 3 through a one-time database
migration. They enter box 4 and return fourteen days after the migration. Changing the review
ladder in settings leaves completed cards completed.

Each interval is a gap from the day the word was last answered, not a fixed date. Answering late
shifts the rest of the schedule later too, rather than leaving dates in the past.

Today shows a short timeline: how many cards are due today and on each of the next several days.
No monthly calendar - a full grid was tried and dropped in favour of this.

## Target language rules

### Nouns carry their article

The card's word is `le chien`, never `chien`, from first exposure onwards. Learning the bare noun
forms the memory without gender, and gender then becomes permanently uncertain.

Gender is never a separate exercise. Requiring the article in typed answers covers it.

### Verbs: infinitive only for now

The infinitive is an ordinary card. Conjugation is a separate system and out of scope.

Reason is scale. `parler` has around 40 usable forms, so attaching them to every verb turns 500 verbs
into 20,000 items, which is a different application.

French makes this tractable later. Most verbs are regular `-er` verbs conjugating identically, there
is a smaller regular `-ir` group, and then roughly 30 irregular verbs that actually matter. So
conjugation becomes its own exercise teaching one pattern or one irregular verb, never a card per
form.

Requirement for now: a card can be tagged with a part of speech, and a verb can later point at a
conjugation table.

### Still to consider

- Adjective agreement.
- Prepositions and verb idiom, à / de / en.
- Liaison and silent letters, partly covered by exercise 5.

## Placement test

Writes `Placements.CutoffRank`, a dictionary teaching rank, once per language. Triage skips words at or below it and sentence training counts them as known. The triage word-range slider edits the same value.

### Algorithm

- Words come from `app/placement/<target>-<native>.tsv`, about 130 rows in bands that double in rank width from rank 100. It ships in the app because the test runs before the dictionary downloads. Built by `build_placement_words.py`, see [dictionary-pipeline.md](dictionary-pipeline.md) section 10.
- Model: P(known) = 1 / (1 + exp((ln rank - L) / 0.6)), with a 3 percent slip each way. L is a belief over 200 log-spaced steps from rank 20 to 60000, prior centred on rank 400 with a spread of 2.0 in ln rank. Each answer is a Bayesian update.
- Next word: random among the 3 unasked words nearest the posterior mean of L.
- Stop at 30 questions; or at 12 or more once the posterior deviation is under 0.35; or after 5 questions with no known word, which gives cutoff 0.

### Output

- Cutoff = exp(mean L - 0.6 * ln(0.6 / 0.4)), about 0.78 * exp(mean L). Below rank 50 it is 0. It is capped at the highest rank in the list.
- Words known on screen = cutoff, rounded to 50 below 1,000 and to 100 above.
- Level comes from that number. There is no C1 or C2, since a yes/no list test cannot separate advanced from near-native.

| Words known | Level |
| --- | --- |
| < 1,500 | A1 |
| 1,500 - 3,999 | A2 |
| 4,000 - 8,999 | B1 |
| >= 9,000 | B2 |

### Decisions

**Show the cutoff, not a vocabulary estimate.** The first version integrated the same curve into a vocabulary size, about 5.6 times the cutoff: a learner at 600 French words saw 2,200 and B1.
- Pro: the shown number is the number the app acts on.
- Con: lower than a vocabulary estimate, so learners may feel under-counted.

**Cutoff at 60 percent known, not 85.** At 85 percent the cutoff was about 0.35 * exp(mean L), and a learner who knew over 600 French words got 309. At 60 percent the same answers give about 680.
- Pro: matches what learners report.
- Con: about 4 in 10 words below the cutoff are unknown, so triage skips some the learner needs. The slider corrects it.

**List ranks must equal dictionary ranks.** The cutoff is a rank applied to the downloaded dictionary. After the French and Spanish rebuilds the list ranks were 20 to 90 percent too high. Restamp after every rebuild that changes teaching ranks.

## Dictionaries

Two are needed, not one.

- **Target language**, for word autocomplete and for definitions in the target language. For French
  that means the French Wiktionary.
- **Native language**, for translations with enough context to choose between senses. The English
  Wiktionary.

## Open questions

1. Desktop layout. The content needs a fraction of a monitor and what the rest of the window is for is
   unresolved.

## Future

- An inactivity timer that notices no interaction for a few minutes and prompts a session, since the
  whole point is the daily habit.
