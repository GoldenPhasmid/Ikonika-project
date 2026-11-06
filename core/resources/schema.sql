-- Trainer database schema. Single source of truth for the user-data database: cards and what
-- the learner did with them. FTSchema.cpp executes this file verbatim through a header generated
-- at configure time. Keep the notes in docs/trainer-schema.md in sync with the columns declared here.

-- DROP rather than CREATE IF NOT EXISTS: Cards keeps its name across a schema change, so the old
-- columns would quietly stay. The date columns are days, the two sync columns are milliseconds.

DROP TABLE IF EXISTS CloudBooks;
DROP TABLE IF EXISTS Erasures;
DROP TABLE IF EXISTS Journal;
DROP TABLE IF EXISTS DailyStats;
DROP TABLE IF EXISTS LocalState;
DROP TABLE IF EXISTS SyncedSettings;
DROP TABLE IF EXISTS TriagedWords;
DROP TABLE IF EXISTS SearchHistory;
DROP TABLE IF EXISTS ReaderDocuments;
DROP TABLE IF EXISTS YoutubeVideos;
DROP TABLE IF EXISTS WordForms;
DROP TABLE IF EXISTS Placements;
DROP TABLE IF EXISTS DeckCards;
DROP TABLE IF EXISTS Cards;
DROP TABLE IF EXISTS Decks;

CREATE TABLE Cards
(
    Id               TEXT PRIMARY KEY NOT NULL,
    TargetWord       TEXT NOT NULL,
    Translation      TEXT NOT NULL,
    Definition       TEXT NOT NULL DEFAULT '',

    -- 0 = masculine, 1 = feminine, NULL for a card with no gender (not a noun, or gender was
    -- never supplied). TargetWord never carries the article itself, added dynamically from
    -- this column wherever the word is shown, spoken or checked against a typed answer.
    Gender           INTEGER,

    -- 'noun', 'verb', 'adj', 'adv', or blank for a card typed in by hand, where nothing said what
    -- part of speech it was. What decides whether the translation reads "to need" or "the need":
    -- a verb takes "to", and a noun takes "the" only where the same spelling is also a verb, which
    -- is a question about the headword rather than this reading and is asked of the dictionary
    -- when the card is shown. Stored bare either way, like TargetWord and its article.
    PartOfSpeech     TEXT NOT NULL DEFAULT '',

    -- The gist of the dictionary meaning this card teaches, blank for a card that teaches the word
    -- as a whole. With TargetWord, PartOfSpeech and Gender it finds the meaning in the dictionary,
    -- whose definition, native definition and example are read from there and never copied here.
    -- A gist the dictionary no longer has leaves the card teaching the word as a whole.
    MeaningGist      TEXT NOT NULL DEFAULT '',
    Hint             TEXT NOT NULL DEFAULT '',

    UserEditedFields INTEGER NOT NULL DEFAULT 0,

    LeitnerStage     INTEGER NOT NULL DEFAULT 0,
    DueDate          INTEGER,
    BoxChangedDate   INTEGER,
    CompletedDate    INTEGER,

    CreatedDate      INTEGER NOT NULL,
    LastUpdatedDate  INTEGER NOT NULL,
    Deleted          INTEGER NOT NULL DEFAULT 0,

    TargetLanguage   TEXT NOT NULL DEFAULT '',
    NativeLanguage   TEXT NOT NULL DEFAULT ''
);

-- Words looked up in the dictionary, newest first, for the Today screen's recent list. Lives here
-- rather than in the dictionary database because it is the user's own history and has to outlive
-- a dictionary file being regenerated or swapped for another language pair.
-- Keyed with the language being learned, since the same spelling is a different word in another one
-- and a French search should not fill the list while Spanish is being studied.
CREATE TABLE SearchHistory
(
    Term           TEXT NOT NULL,
    TargetLanguage TEXT NOT NULL DEFAULT '',
    PartOfSpeech   TEXT NOT NULL DEFAULT '',
    WordId         INTEGER NOT NULL DEFAULT 0,
    SearchedAt     INTEGER NOT NULL,
    PRIMARY KEY (Term, TargetLanguage)
);

-- Words the user has been asked about, so triage stops offering them. Not cards: saying you know
-- five thousand words should not create five thousand things to study. This table's only job is to
-- be a skip list.
--
-- Keyed on all three, because a spelling with a different part of speech or gender is a different
-- word: "moule" is a mould as a masculine noun and a mussel as a feminine one, and knowing one says
-- nothing about the other. Gender is -1 rather than NULL where the word has none, since NULLs do not
-- compare equal and a primary key holding one would admit duplicates.
--
-- The language being learned is part of the key too. Spellings collide across languages, and knowing
-- French "pan" says nothing about the Spanish word. The native language is not: whether the user
-- knows a word does not depend on which language they would translate it into.
-- Only words the learner reported knowing. Turning a word down is a decision about today rather
-- than a claim about what they know, so it lives in the triage run and never reaches this table.
CREATE TABLE TriagedWords
(
    TargetWord     TEXT NOT NULL COLLATE NOCASE,
    TargetLanguage TEXT NOT NULL DEFAULT '',
    PartOfSpeech   TEXT NOT NULL,
    Gender         INTEGER NOT NULL DEFAULT -1,
    MarkedDate     INTEGER NOT NULL,
    PRIMARY KEY (TargetWord, TargetLanguage, PartOfSpeech, Gender)
);

-- What was actually done on a day, so the calendar can show the past.
--
-- Nothing else records it. A card carries only where it is now: answering one moves its due date
-- forward and overwrites the day its box last changed, so by the next morning there is no trace of
-- what was owed or done the day before. Two counters per day are what the calendar reads, and a
-- column nobody fills would read as data rather than as an absence.
--
-- Keyed by language pair like the cards, since a French count would otherwise show while Spanish is
-- being studied. Days are day numbers, as everywhere else.
--
-- One row per device as well. Each device only ever writes its own rows, so two devices studying
-- the same day never contend for a row, and a day is read as the sum of its rows.
CREATE TABLE DailyStats
(
    Day             INTEGER NOT NULL,
    TargetLanguage  TEXT NOT NULL DEFAULT '',
    NativeLanguage  TEXT NOT NULL DEFAULT '',
    DeviceId        TEXT NOT NULL DEFAULT '',

    -- Words moved onto the ladder, and words worked through in a repetition run. Added to rather
    -- than replaced, since a day can hold several sessions.
    LearnedCount    INTEGER NOT NULL DEFAULT 0,
    ReviewedCount   INTEGER NOT NULL DEFAULT 0,

    -- Words still owed a review after the day's last session, and the daily goal that day. Both
    -- replaced on every write. Zero on a row written before these columns existed.
    DueCount        INTEGER NOT NULL DEFAULT 0,
    GoalCount       INTEGER NOT NULL DEFAULT 0,

    LastUpdatedDate INTEGER NOT NULL,
    PRIMARY KEY (Day, TargetLanguage, NativeLanguage, DeviceId)
);

-- The learner's preferences that follow them to every device: the schedule, session sizes, which
-- exercises run, how answers are judged, the reminder. The settings themselves live in QSettings,
-- which has no timestamps, so the sync copies them in here, dated when a change is noticed, and
-- copies the winners back out after a merge.
CREATE TABLE SyncedSettings
(
    Key             TEXT PRIMARY KEY NOT NULL,
    Value           TEXT NOT NULL,
    LastUpdatedDate INTEGER NOT NULL
);

-- What belongs to this copy of the database and to no other: the device's own id and the clock
-- offset measured at the last sync. Never merged, so a copy that arrives from elsewhere cannot
-- bring another device's identity along.
CREATE TABLE LocalState
(
    Key   TEXT PRIMARY KEY NOT NULL,
    Value TEXT NOT NULL
);

CREATE INDEX IdxSearchHistoryTime ON SearchHistory (SearchedAt);
CREATE INDEX IdxCardsLanguages ON Cards (TargetLanguage, NativeLanguage);
CREATE INDEX IdxCardsUpdated ON Cards (LastUpdatedDate);
CREATE INDEX IdxCardsWord ON Cards (TargetWord COLLATE NOCASE);
CREATE INDEX IdxCardsStage ON Cards (LeitnerStage);
CREATE INDEX IdxCardsDue ON Cards (DueDate);
CREATE INDEX IdxCardsCompleted ON Cards (CompletedDate)
    WHERE Deleted = 0 AND CompletedDate IS NULL;
CREATE INDEX IdxCardsCompletedRecent ON Cards (CompletedDate)
    WHERE Deleted = 0 AND CompletedDate IS NOT NULL;

-- The forms a learner is working on or has learned, one row per headword and kind, and per tense
-- for a verb. The id names all of them: "conjugation-fr-vouloir-present", "forms-adj-fr-beau".
-- LearnedDate is a day and stays NULL while the row waits for a training.
CREATE TABLE WordForms
(
    Id              TEXT PRIMARY KEY NOT NULL,
    TargetWord      TEXT NOT NULL,
    Translation     TEXT NOT NULL DEFAULT '',
    TargetLanguage  TEXT NOT NULL DEFAULT '',
    NativeLanguage  TEXT NOT NULL DEFAULT '',
    LearnedDate     INTEGER,
    CreatedDate     INTEGER NOT NULL,
    LastUpdatedDate INTEGER NOT NULL,
    Deleted         INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IdxWordFormsLanguages ON WordForms (TargetLanguage, NativeLanguage);

-- The placement test's result, one row per language being learned. CutoffRank is a dictionary
-- teaching rank: every word at or below it counts as known, without a row of its own. Zero means
-- the test was skipped or found nothing known, and the row still says the question was asked.
CREATE TABLE Placements
(
    TargetLanguage  TEXT PRIMARY KEY NOT NULL,
    CutoffRank      INTEGER NOT NULL DEFAULT 0,
    LastUpdatedDate INTEGER NOT NULL
);

CREATE TABLE Journal
(
    Id         TEXT PRIMARY KEY NOT NULL,
    EventId    TEXT NOT NULL,
    CardId     TEXT NOT NULL,
    EventValue INTEGER NOT NULL DEFAULT 0,
    Day        INTEGER NOT NULL,
    DueDate    INTEGER
);

CREATE INDEX IdxJournalCard ON Journal (CardId, Day);

CREATE TABLE Erasures
(
    Scope      TEXT PRIMARY KEY NOT NULL,
    ErasedDate INTEGER NOT NULL
);

CREATE TABLE CloudBooks
(
    Id              TEXT PRIMARY KEY NOT NULL,
    Title           TEXT NOT NULL DEFAULT '',
    Format          TEXT NOT NULL DEFAULT '',
    FileSize        INTEGER NOT NULL DEFAULT 0,
    DriveFileId     TEXT NOT NULL DEFAULT '',
    AddedDate       INTEGER NOT NULL,
    LastOpenedDate  INTEGER NOT NULL,
    LastPosition    INTEGER NOT NULL DEFAULT 0,
    LastZoom        REAL NOT NULL DEFAULT 0,
    LastScrollX     REAL NOT NULL DEFAULT 0,
    LastScrollY     REAL NOT NULL DEFAULT 0,
    PositionCount   INTEGER NOT NULL DEFAULT 0,
    LastUpdatedDate INTEGER NOT NULL,
    Deleted         INTEGER NOT NULL DEFAULT 0
);
