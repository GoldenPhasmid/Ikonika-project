# Trainer database schema

**Schema changes migrate in place from v0.1 onward.** `AppDatabase::Initialise` opens whatever
`.db` file is already there and hands it to `MigrateSchema`; nothing deletes it on a version
mismatch any more.

The app's own database: the learner's cards and what they did with them. Not the dictionary database (see
[dictionary-schema.md](dictionary-schema.md)) — this is user data, one file per install, at
`%APPDATA%\Ikonika\Ikonika\Ikonika.db`.

`MigrateSchema` (in `core/src/FTSchema.cpp`) reads `PRAGMA user_version`, runs whatever is needed
to bring it up to `CurrentSchemaVersion`, and stamps the new version — all inside one transaction.
A fresh database (version 0) runs the full DDL. Raising `CurrentSchemaVersion` for a real change
requires adding an `if (Reached < N)` block with the actual `ALTER`/`UPDATE` statements for that
step, ending in `Reached = N`. The blocks run in sequence, so a database several versions behind
goes through every step in turn. A version bump with no matching block leaves `Reached` short of
the current version, which makes `MigrateSchema` refuse and the database fail to open, rather than
silently stamping a version it never actually reached.

Adding a table to the DDL counts as a real change and needs its own version. Adding one under a
version that already exists reaches new databases only: every database already stamped at or above
it skips the step and keeps a version claiming a table it does not have. That happened once with
the reader library, and the repair was to repeat the step under a higher version.

The DDL itself lives in [`core/resources/schema.sql`](../core/resources/schema.sql), the single
source of truth. It is turned into a C++ constant at configure time and executed verbatim, so
the schema cannot drift from what actually creates the database.

## Notes

- **Every live card is in My Vocabulary.** There are no decks in this database. A card belongs to
  the language pair it was added under, stored on the card as `TargetLanguage` and `NativeLanguage`.
  Removing a word from My Vocabulary soft-deletes the card. Decks are read-only data in the
  dictionary, so they never need to sync.
- **Schema 32 moved every card off its decks and dropped `Decks` and `DeckCards`.** A card took the
  language pair of a deck holding it, the Learning Pool first. A card held by no live deck, or only
  by a drill's deck, was soft-deleted one millisecond before its own last edit. Every copy runs the
  same migration and reaches the same deletion, and a copy where the card still lives keeps it,
  because a migration never stamps the current time on a downloaded copy.
  Category and user decks lost their grouping and kept their cards and progress.
- **`Deleted` is a soft-delete flag, not a hard `DELETE`.** Every read filters `WHERE Deleted = 0`;
  every delete action is `UPDATE ... SET Deleted = 1`. A hard delete leaves nothing to tell another
  device "this was removed"; a `Deleted = 1` row is itself that signal, and the merge carries it like
  any other edit.
- **`LastUpdatedDate` changes only when the user edits a row.** The merge keeps whichever copy of a
  row has the newer stamp, so a write that is not an edit (a review, a missed date, a part of
  speech filled from the dictionary) must leave it alone, or it would overwrite an edit made on
  another device.
- **`Journal` is the record of a card's progress**: one row per learned, reviewed, pooled or
  completed event, with the day it happened and, for a review, whether the card was remembered. Rows
  are only ever added. Each event also records the due date it produced, so a later change to the
  intervals moves only future dates, never ones already set. The id is a time-ordered UUID (version 7), so two events on the same day
  replay in the order they were made. The merge takes every row from both copies.
- **`Erasures` records when the user last erased everything, and when they last deleted each
  language.** The scope `everything` covers every language; `language:fr` covers one. Deleting a
  language again replaces its time. Erasing deletes rows outright, so without a record the next merge
  would bring them all back from the other copy. The merge keeps the later time of each scope across
  the two copies, then deletes every row older than it on this side, only that language's rows for a
  language scope: by `LastUpdatedDate`, by `MarkedDate` for triaged words and by `SearchedAt` for
  searches. A row edited after the erase survives it, and stays in My Vocabulary. Synced settings are never erased: the
  settings file keeps its values through an erase, and the table has to keep matching it. Journal events are deleted once their card
  no longer exists at all; a
  soft-deleted card keeps its events, since a newer edit can bring it back.
- **`LeitnerStage`, `DueDate`, `BoxChangedDate` and `CompletedDate` are a cache of the journal.**
  Each action applies its event to the stored card, which gives the same result as replaying the
  whole journal. After a merge every card is replayed from its events and the stored values from the
  other copy are ignored. A missed date is never an event: replay works it out from the dates, so it
  demotes a card once, the same way on every device. A card with no events replays to the pool.
- **A card that had progress before the journal existed gets one starting event.** Schema 30, and
  any older copy downloaded by sync, gives each card on the ladder a `placed` event holding its
  stage, dated on the day its box last changed, and each completed card a `completed` event on its
  completion day. A rebuild then keeps where the card stood. The event's id is built from the card,
  the stage and the day, so two copies of the same card in the same state seed the same row and the
  merge keeps one. A card already punished for a missed date is placed back on its box's normal
  date, so it loses that one pending review.
- **IDs are random UUIDs (v4), not autoincrement integers.** Two devices each issuing their own
  sequential IDs would collide the moment they sync; random IDs don't.
- **`LeitnerStage` 0 means the pool** (not yet on the ladder); 1 upward is a ladder stage.
  `DueDate`, `BoxChangedDate`, `CompletedDate` are day numbers, nullable — a pool card has no due
  date at all, which is a real state, not zero.
- **`UserEditedFields` is a bitmask** over which of a card's fields (translation, definition) came
  from the user rather than a dictionary import, so a reimport doesn't clobber a hand-edit.
- **`MeaningGist` names the dictionary meaning a card teaches**, blank for a card that teaches the
  word as a whole. It is the meaning's gist, not a dictionary row id: a dictionary rebuild renumbers
  every row, and a synced card can meet a different dictionary version on another device. With
  `TargetWord`, `PartOfSpeech` and `Gender` it finds the meaning, and the definition, native
  definition and example are read from the dictionary, never copied. A gist the dictionary no
  longer has leaves the card teaching the word as a whole.
  A card added without a gist takes, when it is created, the gist of the meaning its translations
  belong to, or the most popular meaning when they match none. Its part of speech and gender are
  filled from the dictionary at the same moment. Nothing fills them later: a card added before the
  dictionary was downloaded keeps them blank until the user edits it.
- **Dates are milliseconds since the Unix epoch** for `CreatedDate`/`LastUpdatedDate`, day numbers
  (not milliseconds) for the scheduling dates — see `FTDayNumber` in `core`.
- **Foreign keys and double-quoted-string rejection are enforced per connection at open time**, not
  compiled into the database file — see `FTDatabase::Open` in `core/src/FTDatabase.cpp`.
- **The repository is told the active language pair and every card read is filtered to it.** Cards
  of other pairs stay in the file and come back when that pair is selected. The pool is not a table:
  it is the cards of the pair with `LeitnerStage` 0 that are not completed.
- **`DailyStats` is the only record that a day happened.** A card carries where it is now and
  nothing else: answering one moves its due date forward and overwrites the day its box last
  changed, so by the next morning there is no way to reconstruct what was owed or done yesterday.
  Two counters per day, added to rather than replaced since a day can hold several sessions. A day
  with neither count is not stored, which is what lets a day you skipped be told apart from a day
  before any of this was recorded. Each write also replaces `DueCount`, the words still owed a
  review after that session, and `GoalCount`, the daily goal that day. The week strip on Today
  reads its two rings from them: reviewed against reviewed plus still owed, and learned against the
  goal. Both are zero on rows written before schema 24. A day has one row per device
  (`DeviceId`), so two devices studying the same day never contend for a row and the day reads as
  the sum of its rows; the due count and the goal come from the row written last. Rows that
  existed at schema 26 belong to the device that migrated them.
- **`LocalState` holds what belongs to this copy of the database alone**: the device id the daily
  stats are written under, and the clock offset measured at the last sync. The sync merge
  never reads it, so a database that arrives from elsewhere cannot bring another device's
  identity along.
- **`SyncedSettings` is a dated copy of the learner's preferences**, one row per setting, the value
  stored as JSON. The settings themselves live in the workspace's `settings.ini`, which keeps no
  dates. Every change to a synced setting writes its row with the time of the change, and opening a
  workspace writes a row at time zero for any setting the file holds that has none yet. The merge
  keeps the newer row and copies the winning values back into the settings file. Launch at startup,
  the speech cache size, and everything under appearance, motion, reader, sound, audio, sentences
  and triage stay in the workspace's file on this device and never get a row.
- **The language pair syncs, so the last choice wins on every device.** A device without that
  language's dictionary keeps the choice and its setup goes straight to downloading it. A workspace
  where no language was ever chosen takes the pair of the card changed last, at opening and after
  each merge.
- **Rows edited at the same moment still converge.** When two copies of a row carry the same time,
  the merge keeps the one whose whole row compares higher, so both devices pick the same one.
- **What the user said they already know is keyed by the language being learned**, not by the pair.
  Spellings collide across languages, but whether you know a word does not depend on which language
  you would translate it into. Search history is keyed the same way.
- **`ReaderDocuments` lives in the device database, not here.** One row per document the user
  opened in the book reader: the file's path, its title, format and dates. The file is never
  imported; text is re-extracted on open and nothing derived is stored. A path is meaningless on
  another device, so the table sits in the workspace's `Device.db`, which sync never uploads.
  Existence is re-checked against the filesystem on every list, so a moved file shows as missing
  rather than opening nothing. A row that is a copy of a cloud book names it in `CloudId`. See
  [book-reader.md](book-reader.md).
- **`CloudBooks` lists the books uploaded to the cloud, and it syncs.** One row per book: its title,
  format, size, the Drive file that holds it, how many pages or paragraphs it has, and where the
  reader last left it: page or paragraph, zoom and scroll. A device writes the place when it closes the book or the app goes to the
  background, not on every page, so reading does not start a sync on every page turn. Removing a
  book is a dated soft delete, so the merge carries it to every device.
- **`WordForms` holds forms progress, one row per headword and kind, and per tense for a verb.**
  The id names all of them: `conjugation-fr-vouloir-present`, `forms-adj-fr-beau`. `LearnedDate`
  is a day and stays NULL while the word waits for a training. The row carries its own language
  pair, like a card. It syncs like the other tables. See
  [grammar-forms.md](grammar-forms.md).
- **`Placements` holds the placement test result, one row per language being learned.** `CutoffRank` is a
  dictionary teaching rank: triage skips every word at or below it, and sentence training counts those
  words, and the present tense of those verbs, as known. The test only sets its first value: the word range
  slider on the triage screen reads and writes the same column. No row is written per word. Zero means the test
  was skipped or found nothing known; the row still records that the learner was asked, so the test is
  not offered again. It syncs like the other tables.
- **`YoutubeVideos` lives in the device database too**, the same shape as `ReaderDocuments`: the
  YouTube video id, a title, and dates.
