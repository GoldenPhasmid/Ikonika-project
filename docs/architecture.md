# Architecture

Feature-to-code map: which class owns which feature, and what it sits on top of. Read this before
touching a feature you haven't worked in yet, so you don't have to rediscover the owning class by
grepping.

`core/` is a static library with no UI module: card model, scheduling, scoring, dictionary
lookup, database access. `app/` is the Qt Quick app: QML screens plus the C++ controllers that
expose `core` to them. A controller in `app/src/controllers/` almost always wraps one or more
`core` types and does not reimplement their logic.

## Session drivers (Intensive Training / Review)

| Class | Location | Role |
| --- | --- | --- |
| `StageSessionControllerBase` | `app/src/controllers/StageSessionControllerBase.h/.cpp` | Shared base. Owns a list of `IExerciseFlowStage`, drives `StartStages/AdvanceToNextStage`, exposes `activeController`/`stage`/`stageProgress` to QML. Pure-virtual `HasCards/BuildStages/ApplyOutcomes/DiscardSession`. |
| `IntensiveLearningController` | `app/src/controllers/IntensiveLearningController.h/.cpp` | Learns new words from My Vocabulary. `startCards()`, `resume()`, `restartSession()`; a session is keyed by `vocabularyKey` or a forms training id. Builds one `FMasteryLoopStage` over `FlipRecallController`, `MultipleChoiceController`, `WrittenProductionController`, `AudioTranscriptionController`, `SpeedMatchController`. The mastery stage is an `FMasteryStage` (`ExerciseFlowStages.h`), which `RepetitionController` builds too through `StageSessionControllerBase::AppendMasteryStage`: the word loop, then two sentence phases from `BuildSessionSentenceStage` (`app/src/controllers/SessionSentenceStage.h/.cpp`). Each phase is an `FMasteryLoopStage` with `RepeatsToMaster = 1` over the sentence cards, the first with a `SentenceTilesController` building the French sentence, the second with one built by `SetBuildsTranslation(true)`. The stage's `FPolicy` serves every controller of the stage, so a word's count runs through `RepeatsToMaster` plus one step per sentence phase, and each sentence keeps the word it was picked for in `FSessionSentence::WordCardId`. Each word weighs up to half the session's word count of its focused candidates, at least one, four times that when the native language is English. With English, a known-word sentence is kept for a word only when its translation holds one of the card's translations or of its meaning's, in any English form: `MentionsTranslation` in `core/FTEnglishForms.h` matches regular endings, a table of irregular forms, and placeholders such as "one's" standing for up to three words. Text in parentheses is ignored. Multiple choice, the sentence stages and speed match are optional stages, appended by `AppendChoiceStage`, `AppendSentenceStages` and `AppendMatchStage`. A checkpoint stores each one's count as `choiceStageCount`, `sentenceStageCount` and `matchStageCount`, and `ResolveSavedStage` shifts a saved stage index by the difference when a count changed since the save. A checkpoint saved inside a stage that is now gone resumes at the stage after it. A checkpoint older than the choice and match settings counts both as present. It also stores the picked sentences as `sessionSentences`, so a resumed session rebuilds the stage from them without picking again, and, while a sentence stage runs, its mastery counts and active batch as `sentenceStageProgress`. `StageSessionControllerBase::SaveOptionalStages` and `RestoreSentenceStage` write and read both for Train and Review. Each word gets its meaning example, or one known-word sentence from `FTSentenceTree::FindFocusedSentences` when it has no usable example, so the stage holds one sentence per word; `SetSentenceSource` maps a sentence card back to its sentence. Known words and the usable-sentence checks are shared with Sentences training in `app/src/controllers/SentenceSelection.h/.cpp`. |
| `RepetitionController` | `app/src/controllers/RepetitionController.h/.cpp` | Works the day's due cards. `startDueToday()`. Same `FMasteryLoopStage` set as Intensive, **plus `CorrectIncorrectController`** — Review has 5 exercise types, Intensive has 4. Splits due cards via `FTRepetitionBatch`, applies outcomes via `FTScheduleManager`. |
| `FMasteryLoopStage` | `app/src/controllers/ExerciseFlowStages.h/.cpp` | The actual exercise-rotation/mastery logic (weighted picking, per-card mastery/wrong counts, batch progress). `ExerciseControllerBase` is **not** this — it's just the single-exercise-type contract. Both session drivers compose this. |
| `FProxyStage` | `ExerciseFlowStages.h/.cpp` | Wraps one `ExerciseControllerBase` as a single-controller stage. |
| `IExerciseFlowStage` | `IExerciseFlowStage.h` | Stage interface: `Start/IsFinished/GetProgress/RestoreActiveExercise`. |
| `AppState` | `app/src/AppState.h/.cpp` | Not a class in the OOP sense — a bare singleton holding the checkpoint blobs: `IntensiveLearningCheckpoints` (map, per session key), `RepetitionCheckpoint` and `SentenceCheckpoints` (map, per sentence exercise). Persisted via `AppSessionCheckpoints`. |
| `DailySessionController` | `app/src/controllers/DailySessionController.h` | Home-screen due/new/pool counts and `shortfall()`; feeds the entry points above, doesn't drive a session itself. |

## Exercise types

All extend `ExerciseControllerBase` (`app/src/controllers/ExerciseControllerBase.h/.cpp`), which
holds `FTExerciseSession`/`FCardState`, checkpoint (de)serialization, answer bookkeeping, and
prompt speech. Concrete types:

| Class | Exercise |
| --- | --- |
| `MultipleChoiceController` | Pick from options; builds distractors. |
| `WrittenProductionController` | Type the answer; graded via `core/FTAnswerComparison`. |
| `FlipRecallController` | Flip/reveal flashcard; question side can be forced by the owning stage. |
| `AudioTranscriptionController` | Listen and type; graded via `core/FTAnswerComparison`. |
| `SpeedMatchController` | Timed pair-matching, 5 pairs. |
| `CorrectIncorrectController` | Binary self-graded recall (`answerYes/No`); **used only by `RepetitionController`**. |
| `FlashcardController` | Plain flip/browse with next/previous history; pulled in by `IntensiveLearningController` but otherwise not part of the mastery loop set above. |

Two more controllers live beside this hierarchy but are **not** `ExerciseControllerBase`:

- `TriageController` (`TriageController.h/.cpp`) — vocabulary triage screen (mark known/unknown,
  add to My Vocabulary). Plain `QObject`, reads `core/FTDictionaryRepository` directly.
- `ExerciseLauncherController` (`ExerciseLauncherController.h/.cpp`) — a third, ad-hoc entry point
  that constructs one `ExerciseControllerBase` by name outside the stage-session machinery
  (practice-mode style, single exercise, not a full session).

## Decks

| Class | Location | Role |
| --- | --- | --- |
| `FTDictionaryRepository::ListDecks`, `LoadDeckWords` | `core/include/core/FTDictionaryRepository.h` | Read the optional `decks` and `deck_words` tables. A deck word carries its reading, gist, gender and the translation of its meaning. |
| `MatchDeckWords` | `core/include/core/FTDeckProgress.h` | Gives each deck word its card or known state from the cards and `FTCardRepository::ListKnownWords`, the triaged words with no card of theirs deleted since they were marked. |
| `DecksController` | `app/src/controllers/DecksController.h/.cpp` | QML singleton. `available` hides the Decks tab and destination; `decks` lists each deck with its category and its new, learning and learned counts. Refreshes when the dictionary changes. The deck list and deck words are read once per opened dictionary, and `decksChanged` fires only when a count changed. |
| `VocabularyController::deckId` | `app/src/controllers/VocabularyController.h/.cpp` | Set, the controller lists the deck's words in deck order. A word without a card is a placeholder card with no id, marked completed when known; `inVocabulary` tells the screen to drop card actions. `deckNewCount` counts the words left for triage. |
| `TriageController::startDeck` | `app/src/controllers/TriageController.h/.cpp` | Deck mode: the queue is the deck's new words sorted by teaching rank, with no grade cursors, start rank or word limit. A deck word that names a meaning offers only that meaning, so its card always teaches it. `progressCount` counts added, known and skipped words. |

Mobile: `MobileDecksScreen` is the `decks` tab: a row of `MobileFilterChip` for All and each deck category with a tiles or list switch, then each category under its heading as `MobileDeck` items, covers two to a row or rows with a small cover. Tapping a deck starts deck triage while it has new words, and its words button opens it. Category tints and totals come from `Decks.js`, which the desktop screen shares, and `AppViewLayouts` remembers the tiles or list choice. The `deck` route is a second `MobileVocabularyScreen` with `deckId` set; its Learn button starts deck triage, and finishing returns to the deck. Desktop: `DesktopDecksScreen` is the `decks` destination, a grid of `DesktopDeckTile` that loads a `DesktopVocabularyScreen` with `deckId` set only while a deck is open; the top bar's Learn starts deck triage, and finishing leaves the session.

## Word forms training

Two trainings share one path: Conjugations for verbs, Forms for irregular nouns and adjectives.
A kind is named by its part of speech, `verb`, `adj` or `noun`, and belongs to one training.

`core/FTWordForms.h` makes one forms card per word (`MakeFormsCard`), expands it into session
items (`ExpandFormsCard`) and reads an item back into subject, base word, form and marked ending
(`ReadWordForm`). It also holds the per-kind table (id prefix, training), the per-training table
(words per session) and the per-language table (pronouns, cues, regular endings, and the verb
tenses with their unlock order). A verb card carries its tense as the last part of its id, read
back with `GetFormsTense`.

Progress lives in the `WordForms` table, one row per forms card id, through
`FTCardRepository::AddFormsCards`, `MarkFormsCardsLearned`, `LoadFormsCard` and `LoadFormsCards`.
A row waits until it is marked learned. Nothing about forms is stored in `Cards`.

A training is addressed by an id built from its name and the language pair
(`MakeFormsTrainingId`), which stands where `vocabularyKey` would: `IntensiveLearningController` keys
the session and its checkpoint on it, and `DailySessionController::collectSessionCards` and
`shortfall` answer for it. `MobileTodayScreen.qml` lists the two trainings and opens one only
when `DailySessionController::canStartFormsTraining` says a full session is in reach,
`MobileMain.qml` starts them, and `DailySessionController::collectFormsCards` fills a session
from the waiting words and, at random, from `AppCards::ListFormsCandidates`, the studied words of
My Vocabulary. `AppCards::ListTenseOffers` says, per tense, which verbs
wait and which are ready; `DailySessionController::formsTenses` and `collectTenseCards` expose
that to the tense sheet (`MobileTenseSheet.qml`). `AppCards::ExpandForms`/`LoadSessionCard` turn
forms cards and checkpointed ids into session cards.

The session takes its stages from `BuildFormsStages` (`app/src/controllers/FormsStages.h/.cpp`):
a `FlashcardController` over the words with `SetFormsTable`, then a `SpeedMatchController` with
`SetPairText` and a `WrittenProductionController` with `SetContentBuilder` over the expanded
forms. Forms are not reviewed. See [grammar-forms.md](grammar-forms.md).

The last stage is missing word. Each asked form becomes two round cards, `MakeFormsRoundId` adding
`@1` or `@2` to the item id; `GetFormsItemId` strips it, and `ReadWordForm` and
`AppCards::LoadSessionCard` read round ids as their form. `PickFormSentences` reads each word's
stored sentences with `FTDictionaryRepository::FindFormSentences`, keyed by the headword, kind and
tense `ReadFormsSentenceKey` reads from the card id. It matches the rows to the forms by the cell,
person or gender and number, and places the gap with `PlaceFormGap` (`core/FTWordForms.h`). A dictionary
without the `form_sentences` table takes the old path: `FTSentenceTree::FindSentencesWithWords`
finds every sentence of the word, each is filed under every form `LocateFormGap` fits it to, and
the forms with the fewest candidates pick first. The content builder returns the gapped
sentence with `bSentence` set, which `WrittenProductionController::sentencePrompt` exposes so the
typing screens switch to sentence layout per card; a round with no sentence gets the bare form
prompt. The stage is an `FMasteryLoopStage` with `RepeatsToMaster = 1`. `MakeFormRounds` orders the
round cards word by word, round 1 first, with the word order shuffled per round from the same seed,
and hands each word's rounds to `SetBatchGroups`. A batch is then the unmastered cards of the first
group that still has one, so a word repeats its missed forms before the next word starts.
`SetCardGroup(GetFormsItemId)` keeps a batch from opening with the form answered last.

## Sentence training

Sentence exercises over dictionary sentences built only from words the learner studies. Nothing here touches the ladder. A session left halfway is saved and resumed, one save per exercise.

| Class | Location | Role |
| --- | --- | --- |
| `SentenceTrainingController` | `app/src/controllers/SentenceTrainingController.h/.cpp` | Session driver, a `StageSessionControllerBase`. Picks the sentences and builds the stages. |
| `SentenceTilesController` | `app/src/controllers/SentenceTilesController.h/.cpp` | Word tiles exercise, an `ExerciseControllerBase`. |
| `FTSentenceTree` | `core/include/core/FTSentenceTree.h` | The loaded prefix tree and its query, `FindWritableSentences(FTWordIdSet, FTSentenceQuery)`. |
| `FTSentenceCards.h` | `core/include/core/` | Free functions: `MakeSentenceCard`, `SplitSentenceWords`, `SplitAtGap`, `MakeGapPrompt`, `CompareSentence`, `PickBalancedSentences`. |
| `AppDictionary::GetSentenceTree` | `app/src/AppDictionary.h` | Loads the tree on first use, drops it when the dictionary closes or changes. |

**Picking sentences.** `SentenceTrainingController::FindCandidates`:

1. `FTCardRepository::ListKnownHeadwords()` returns the headwords of cards on the ladder or completed, plus the words answered as known in Training that have no live card, for the active language pair.
2. `FTDictionaryRepository::ResolveWordIds` turns them into an `FTWordIdSet`, a bitset indexed by `words.id`. A headword matching several words sets every id.
3. `FTCardRepository::ListKnownVerbTenses()` returns every conjugation learned, as verb and tense. Each becomes a tense key in the same set: `FTSentenceTree::MakeTenseKey(word id, GetFormsTenseCode(tense))` for every id `ListWordIds(verb)` returns. A sentence with a conjugated verb is writable only when that key is known, except a regular present, which the build gives no key, so a verb's sentences open tense by tense as its conjugation cards are trained. A form the dictionary gives no tense needs all of that verb's trained tenses.
4. `FTSentenceTree::FindWritableSentences` walks the tree with `MaxCount` 15 times the sentence count (`CandidatesPerSentence`), `LevelMask` from the difficulty, and a fresh `Seed`. The query can also bound `MinLength` and `MaxLength`, the number of word keys on the path with tense keys left out; the controller sets neither, so a sentence of any length qualifies. With `MaxCount` set the walk keeps a reservoir sample as it goes, so it never builds the full match list. A sentence with several paths has a slightly higher chance, since each path offers it once.
5. `LoadTreeSentences` reads the rows, and `FindUnusableReason` keeps those with a gap, a translation, at least 3 words and at most 80 characters. It counts what it rejects by reason.
6. `PickBalancedSentences` shuffles the candidates with a fresh seed, then keeps as many as the sentence count asks for, from a run whose word counts, then `ChangedCount`, are closest to each other. Equally close runs are chosen at random.

`start()` fails when fewer than the sentence count remain. Each sentence becomes a temporary `FTCard` with id `sentence:<sentences.id>`, the sentence as `TargetWord` and its translation as `Translation`, so the existing exercise controllers take it unchanged.

**Stages.** `start("training")` builds four stages: flashcards, then the three exercises. `start("tiles" | "meaning" | "gap" | "dictation")` builds one. `meaning` is not part of Training yet. The flashcard stage is an `FProxyStage` that shows every sentence once. Each exercise stage is an `FMasteryLoopStage` over a single controller with `RepeatsToMaster = 1` and `FixedLength` set to the number of sentences, so a missed sentence is drawn again until it is answered right.

| Stage | Controller | How it is configured |
| --- | --- | --- |
| Flashcards | `FlashcardController` | Training only. The sentence card has the sentence as its word and the translation as its answer, so the controller runs unchanged: it speaks the sentence and steps with `next` and `previous`. |
| Word tiles | `SentenceTilesController` | Speaks the sentence, deals its words shuffled, grades when `submit` is called with every answer tile placed (`canSubmit`); `placeTile` never grades and takes no more tiles than the answer holds. `tiles`, `placed`, `lastTranslation`, `canSubmit`; `placeTile`, `removeTile`, `submit`, `replay`, `skip`, `advance`. With `SetTypingAllowed(true)`, as in the Train and Review sentence stage, `typing` switches to a typed answer graded by `submitTyped`; the choice is stored in `QSettings` under `sessions/sentenceTyping`. |
| Listen and translate | `SentenceTilesController` | `SetBuildsTranslation(true)`. Speaks the sentence and deals the words of its translation plus `DistractorCount` (3) words taken from other sentences' translations, none equal to a translation word ignoring case. It grades once as many tiles are placed as the translation has words: the answer is right when the placed words are the translation's words in any order, since the order of a free translation cannot be checked. The distractors are saved with the checkpoint. |
| Missing word | `WrittenProductionController` | `SetAskWord(false)`, and `SetContentBuilder` returns the gapped prompt, the gap word as the one accepted answer, and the full sentence as `PromptSpeech`. The screen marks the gap word in the revealed sentence from the position of `___` in the prompt. `replay()` speaks the sentence again. |
| Dictation | `AudioTranscriptionController` | `SetAnswerCheck` replaces `CompareAgainstAny`, which would split the sentence at its commas, with `CompareSentence`, which compares word by word without punctuation. |

Every stage controller gets `SetSpeechRateScale(0.75)`; see [voice.md](voice.md#sentence-rate).

**Checkpoints.** The kind is `Sentences` and the key is the exercise name, so Training and each single exercise keep separate saves. `SaveCheckpoint` runs on every `stateChanged` and writes the JSON to `AppState::SentenceCheckpoints` and `AppSessionCheckpoints`.

| Field | Content |
| --- | --- |
| `exercise` | `training`, `tiles`, `meaning`, `gap` or `dictation`. |
| `sentences` | `{id, text}` for each sentence of the session. |
| `stageIndex`, `stageCount`, `elapsedMs` | Stage reached, number of stages the session was built with, and time spent so far. |
| `masteryCount`, `batchCount`, `activeCandidateIndex`, `activeBatchCardIds` | The progress of the current `FMasteryLoopStage`, fed back through `RestoreProgress`. Absent on the flashcard stage, which is restored through `RestoreActiveExercise`. |
| `activeExercise` | The stage controller's own `CheckpointToJson`, including its `SaveExtraState`. |

`SentenceTilesController` saves `tileOrder`, `placed` and `lastTranslation`; `WrittenProductionController` saves `lastCardTranslation`.

`start(exercise)` resumes the active session when it is the same exercise, loads the save when there is one, and builds a new session otherwise. `LoadCheckpoint` reads every saved sentence back from the dictionary and drops the save when a text no longer matches or when `stageCount` differs from the stages the exercise builds now. It starts only the saved stage, so no earlier stage speaks. Finishing or `endSession()` clears the save of that exercise. `forgetSessions()` clears all of them and runs when the language pair changes; `AppSyncController::ClearExerciseSessionState` clears them too.

**Difficulty.** `difficultyMask` is a bit set: 1 Easy, 2 Normal, 4 Hard. `MakeGrammarLevelMask` maps it to tree levels: Easy to level 1, Normal to level 2, Hard to levels 3 and 4. It is stored in `QSettings` under `sentences/difficulty`, defaults to Easy, and can never be empty. Setting it calls `refresh()`, which recomputes `readyCount` and `canStart`.

**Sentence count.** `sentenceCount` is the length of a new session: 5 to 20 in steps of 5, default 10, stored in `QSettings` under `sentences/count`. A value outside the range is clamped and snapped to the nearest step. Setting it calls `refresh()`. A saved session keeps the length it was started with.

**QML surface of `SentenceTrainingController`.** Properties `available` (the dictionary has a tree), `readyCount`, `sentenceCount`, `minimumSentenceCount`, `maximumSentenceCount`, `sentenceCountStep`, `canStart`, `unlocked` (easy, normal and hard each have at least ten usable sentences; the Today button needs this or a saved session), `difficultyMask`, `elapsedSeconds`, `sessionSentenceCount`, `savedExercises`, plus `stage`, `finished`, `hasActiveSession` and `activeController` from the base. Invokables `refresh()`, `start(exercise)`, `endSession()`, `forgetSessions()`, `completeStage()`, `restartStage()`.

**Mobile screens.** `MobileMain` owns one `SentenceTrainingController` and routes two screens:

| Screen | Route | Role |
| --- | --- | --- |
| `MobileSentenceTrainingScreen` | `sentences` | Lists Training and the three exercises. A tap opens `MobileSentenceStartSheet`, which holds the difficulty chips, the sentence count slider and Start. An exercise named in `savedExercises` reads Continue and resumes without the sheet. |
| `MobileSentenceSessionScreen` | `sentencesession` | Shows the screen of the active stage, chosen by the type of `activeController`, then `MobileExerciseCompleteScreen`. Back leaves the session saved; End session in the menu discards it. |
| `MobileSentenceTilesScreen` | inside the session | The tiles exercise. The answer box ends with a typing chip: Space places the tile that matches the typed text, Enter places it and submits when the answer is full, the Submit button grades a full answer, Backspace on an empty chip takes the last tile back, and a tap on the box focuses the chip. A tap on a word of the revealed sentence raises `wordTapped`. |
| `MobileSentenceAudioSlot` | inside the three exercise stages | One fixed-height slot under the sentence: the play button before the answer, the translation after it. The sentence and the divider stay where they are when the answer is revealed. |
| `MobileCenteredFlow` | inside the tiles exercise | Wraps its children into rows and centres each row. Holds the tile bank. |
| `MobileSentenceText` | inside the four stages | The sentence itself. Renders every word as a link without link styling, so `linkAt` finds the word under a long press; emits `wordHeld(word)`. `gapStart` and `gapLength` colour the gap word. |
| `MobileWordExplainSheet` | over the session | The hold popup: a bottom sheet listing the dictionary search results for the held word. A tap on a card speaks the headword. |

**Tap to look up.** `MobileLookupSheet` is the Reader's dictionary search sheet as its own component, with its own `DictionaryController`: a search field, result cards with swipe-to-add, and an add-by-hand button for a word the dictionary lacks. `MobileReaderScreen` owns one, and `MobileMain` owns one for every exercise. Every sentence is drawn by `MobileSentenceText`, and a tap on one of its words raises `wordTapped`: the exercise sentence in the tiles, written-answer and dictation screens, and every example, `MobileExampleBlock` in the exercises and `MobileMeaningDetails` and the Examples section on the dictionary, card and triage screens. `MobileIntensiveLearningScreen`, `MobileRepetitionScreen` and `MobileSentenceSessionScreen` pass it on as `lookupRequested`, and every other screen connects it in `MobileMain` directly. `MobileMain` opens its sheet, and from the sheet the add-word screen or the dictionary entry, reusing either screen when it is already on top. A flashcard's own sentence keeps a long press only, since a tap on the card turns to the next one; a tap on a word of its example looks the word up instead.

**Click to look up on desktop.** `DesktopSentenceText` draws the exercise sentences and every example, and raises `wordClicked`. Inside a session `DesktopSessionScreen` shows the word in its lookup pane; elsewhere the word opens on the Words screen, through `showWord` on that screen and `openDictionary` from My Vocabulary, the card dialog and triage. Qt cannot elide rich text, so the example in an exercise's reveal well is clipped to `definitionLineCount` lines instead.

**Hold to explain.** A long press on a word of a sentence, or on a tile, raises `wordHeld` on the stage screen. `MobileSentenceSessionScreen` hides the keyboard, runs the word through its own `DictionaryController::search` and opens `MobileWordExplainSheet` once the results arrive; closing it gives the answer field its focus back. The flashcard screen has a tap area over the whole card, so there the long press is taken from that area's `pressAndHold` and mapped into the sentence. On desktop a click on a sentence word fills the session's lookup pane with the same search.

`DictionaryController::search` strips punctuation around the text, then runs `FTDictionaryRepository::Search`, which also matches inflected forms and marks the matches spelled as searched. Only when none is spelled as searched is a word with an apostrophe or a hyphen split: each part is searched on its own and only its exact matches are kept, an elided part is restored first (`l'` to `le` and `la`, `s'` to `se` and `si`, any other to the stem plus `e`; French only), the euphonic `t` is skipped, and the full parts are listed before the elided ones, ahead of whatever the whole spelling found.

Multiple choice, typed translation and dictation move on by themselves only after a correct answer. A wrong answer shows a Continue button and waits for it. Sentence stages never move on by themselves.

The Sentences missing word and dictation exercises reuse `MobileWrittenProductionScreen` and `MobileAudioTranscriptionScreen` with two properties set: `sentenceText` switches to sentence-sized fonts, and `manualAdvance` replaces the advance timers with a Continue button that is always shown and enabled once the answer is revealed. `MobileChipSelectRow` and `MobileToggleChip` are the chip row the sheet shares with the Allowed exercises setting, `MobileExerciseTypesRow`.

The index itself is described in [dictionary-pipeline.md](dictionary-pipeline.md), step 7, and [dictionary-schema.md](dictionary-schema.md#sentences-sentence_tree).

## Grammar forms

Not a UI feature. `FTDictionaryEntry.h` (core) defines `FTDictionaryForm` (text/mood/tense/
person/gender/number) and `FTDictionaryEntry::Forms`, gathered at dictionary-build time and
surfaced through `DictionaryController::entry`'s `QVariantMap` on the dictionary word view. No
dedicated controller exists.

## Dictionary lookup / add word

| Class | Location | Role |
| --- | --- | --- |
| `DictionaryController` | `app/src/controllers/DictionaryController.h/.cpp` | Search/entry-view controller for the Dictionary screen: `search()`, `openEntry()`, `speak()`. |
| `AppDictionary` | `app/src/AppDictionary.h/.cpp` | Which dictionary package is open; owns the `FTDatabase` connection + `FTDictionaryRepository`. |
| `FTDictionaryRepository` | `core/include/core/FTDictionaryRepository.h` | SQL-backed lookup (`LookUp`, `Search`, `LoadLearnableWords`). Used directly by `TriageController` too. |
| `FTDictionaryEntry` / `FTDictionaryPackage` | core | Pure data: entry shape, installed-package metadata. |
| `VocabularyController` | `app/src/controllers/VocabularyController.h/.cpp` | My Vocabulary: the list, its filters and counts, and the add-word path (`addCard()`, `updateCard()`), not `DictionaryController`. |
| `GroupRowsByHeadword` | `app/src/controllers/WordRowGrouping.h/.cpp` | Collapses list rows that share a headword into the first one: furthest stage, easiest CEFR level. `DictionaryController` and `WordListController` always apply it, so every word list, lookup sheet and explanation shows a word once whatever its parts of speech. |

Every word list lists one row per headword, with no part-of-speech tag. The entry, on both, shows a tab row under the headword block, one tab per entry in `DictionaryController::readings`, each with its own learned mark; the selected tab drives gender, phonetic, translations, meanings, examples and Add word. The frequency and CEFR badges show the best value across all parts of speech. A translation chip counts as learned only when a card of the same part of speech carries it. A card with no part of speech counts under every reading. A word the dictionary lacks gets an entry built from its own cards, through `DictionaryController::cards`. Edit opens the selected reading's card, or lets the learner pick one when the reading has several: a menu on desktop, a sheet on mobile.

## Book reader

`ReaderController` (`app/src/controllers/ReaderController.h/.cpp`) is library + open-document
state only — it explicitly does not render PDFs (QML's `ReaderPdfPage` self-renders via a worker
thread, `FTPdfRenderWorker`, off a `QThread`) and does not own dictionary lookup (reuses the shared
`DictionaryController`). Non-PDF formats go through `core/FTDocumentExtractor` (txt/fb2/epub
concrete extractors) into `FTParagraphModel` for QML. `core/FTReaderRepository` is the local SQLite
library table, explicitly excluded from Google-account sync merge (a file path is meaningless
across devices).

Free books: `AppFreeBooks` (`app/src/AppFreeBooks.h/.cpp`) reads the book catalog for the language
being learned (`core/FTBookCatalog`), downloads a book through `AppFileTransfer` into the workspace's
books folder and adds it to the library. `ReaderController` merges the catalog with the library into
`freeBooks`, matching a downloaded book by its file path, and deletes a free book's file on remove.

## YouTube reader

`YoutubeController` (`app/src/controllers/YoutubeController.h/.cpp`) is architecturally a twin of
`ReaderController` by design (its header cross-references Reader's): library + open-video +
caption-cue state, no playback ownership (screen embeds the IFrame player, calls `reportTime()`),
shared `DictionaryController` for lookup. `AppYoutubeCaptions` fetches title/caption track over the
network; `core/FTYoutubeCaptions.h` parses WebVTT and picks manual-over-auto-generated tracks;
`core/FTYoutubeRepository` mirrors `FTReaderRepository`'s local-only pattern.

## Google account sync

| Class | Location | Role |
| --- | --- | --- |
| `AppSyncController` | `app/src/controllers/AppSyncController.h/.cpp` | Orchestrates the cycle: sign-in → find remote → pull → merge → push. Local-first, pull-driven. Switches `AppDatabase` between local and per-account files. |
| `AppGoogleAccount` | `app/src/AppGoogleAccount.h/.cpp` | OAuth2 flow only; knows nothing about Drive or sync. |
| `AppDriveSync` | `app/src/AppDriveSync.h/.cpp` | Raw Drive v3 REST calls for one fixed `appDataFolder` file. Takes `AppGoogleAccount*` for tokens. |
| `FTSyncMerge` | `core/include/core/FTSyncMerge.h` | Pure last-write-wins row merge across Cards/Journal/DailyStats/TriagedWords/etc. No Qt/network dependency. |

`AppFileTransfer` (checksum-verified async download) is a shared utility used here and by
`AppPackageDownloader`/`AppUpdater` — it has no Google/Drive dependency despite the adjacency.

## Core session/scheduling primitives

| Class | Location | Role |
| --- | --- | --- |
| `FTExerciseSession` | `core/include/core/sessions/FTExerciseSession.h` | Per-exercise draw/advance engine `ExerciseControllerBase` sits on. `SetResult()` is an unimplemented stub (`// @todo`) — actual grading happens per-controller via `RecordAnswer`/`CompareAnswer`, not here. |
| `FCardState` | `core/include/core/sessions/FCardState.h` | Immutable wrapper over a card handle list. |
| `FCardSelectorBase` | `core/include/core/FCardSelectorBase.h` | Card-ordering strategies: `FSequenceSelector`, `FEachOnceSelector`, `FWeightedSelector` (used by `FMasteryLoopStage`). |
| `FTScheduleManager` | `core/include/core/FTScheduleManager.h` | Leitner core: `LoadDueCardsForToday`, `ReportSessionOutcomes`. |
| `FTSchedule.h` | core | Box/stage math: `GetIntervalDays`, `Promote/Demote/ReturnToPool/ApplyExpiry`. |
| `FTRepetitionBatch.h` | core | Splits due cards into batches for `RepetitionController`. |
| `FTCard.h` | core | The card struct every controller reads through; `GetDisplayWord/GetDisplayTranslation`. |
| `FTCardRepository` | core | The big SQLite card CRUD/query surface. `AppDatabaseService`, `AppCards`, `DailySessionController`, `VocabularyController`, `FTScheduleManager` all sit on top of it. |
| `FTAnswerComparison.h` | core | Grades typed/spoken answers (`Exact/ArticleOmitted/AccentsWrong/Wrong/Skipped`). |
| `FTScoring.h` | core | `CalculateCardScore()` — not referenced by any controller found while writing this doc; verify before relying on it. |
| `FTSpeech.h` | core | `FTSpeechSynthesizer`/`FTSpeechService`, wrapped 1:1 by `AppSpeechService`. Desktop only: Android builds core without it and without sherpa-onnx. |

## App-level singletons

| Class | Role |
| --- | --- |
| `AppReminder` | The notification/reminder singleton (tray icon, study-streak polling). Not named `AppNotifier`. |
| `AppSpeech` / `AppSpeechService` / `AppPlatformSpeech` | TTS interface, which also holds the request queue that waits out screen transitions. `AppSpeechService` speaks through `core/FTSpeechService` on desktop; `AppPlatformSpeech` speaks through Android's own TextToSpeech over JNI. |
| `AppSingleInstance` | Second-launch detection, restores the app from minimized. |
| `AppCatalog` / `AppPackageDownloader` | Remote package catalog and the download+unpack pipeline for dictionaries/voices described in [AGENTS.md](../AGENTS.md). |
| `AppDatabase` / `AppDatabaseService` | The app's own database (cards and their progress) — separate from the dictionary database. |
| `AppSessionCheckpoints` | Static JSON checkpoint save/load backing `AppState`. |
| `AppOnboarding` | First-run language pick and initial downloads. |
| `AppUpdater` | Self-update check/apply, shares `AppFileTransfer` with the package downloader. |
| `AppTranslator` / `FTInContextTranslator` (core) | Machine translation, one `AppTranslator` interface with two implementations. Desktop (`AppTranslator.cpp`) wraps an int8 ONNX OPUS-MT model per language pair, found in the downloaded models directory first, and downloads it as the catalog package `AppCatalog::FindTranslationPackage` names; `Reset` releases the model around unpacking it. Android (`AppPlatformTranslator.cpp`) calls ML Kit through `PlatformTranslation.kt`, which downloads its own model from Google. `AppMachineTranslation` exposes installed, offered, downloading and progress to QML for both. `DictionaryController` translates a search that found nothing, as `machineTranslation`. See [machine-translation.md](machine-translation.md). |
| `AppLocalization` / `AppTheme` / `AppMetrics` | UI language, colour tokens, and non-colour design tokens (spacing/sizing) respectively. `AppMetrics` is a design-system companion to `AppTheme`, not analytics. |
| `SettingsStore` | The Settings screen's backing model: audio, schedule intervals, session size, language pair, database reset. |
| `AppCards` | Free-function namespace, not a class. Fills the dictionary-derived facts a card cannot answer itself: whether the spelling is also a verb, Spanish first-syllable stress, French elision before h. `AppCards::AnnotateLoadedCards` registers `Annotate` on `FTCardRepository` (`SetCardAnnotator`), so every card the repository loads is annotated in one batch per load, whichever screen asked. `Prepare` remains for cards built outside the repository. |

Two QML-singleton registration styles exist side by side: most singletons use `QML_ELEMENT
QML_SINGLETON` directly; a few (`AppReminder`, `AppOnboarding`, `AppUpdater`, `AppCatalog`,
`AppPackageDownloader`) instead pair `QML_FOREIGN` with a separate `XSingleton::create()` struct,
specifically to keep QML's default-construction path from colliding with a `Get()`-based true
singleton. Use the `QML_FOREIGN` form for a new singleton that needs a non-default constructor.
