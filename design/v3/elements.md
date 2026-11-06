# V3 elements

Every element in the v3 wireframes, what it means, and where the drawing and the built app
disagree. The fragments have been brought up to what the QML actually does, so this file now
records decisions rather than open questions. Marked **?** where something is still undecided.

The reader, the YouTube screen and the two rule drills are out of scope for this pass and have
no fragment.

## Shell — every screen

| Element | Meaning |
| --- | --- |
| Rail | Left nav, 240px, collapses to 56px on its own below a 1100px window. There is no collapse control. |
| Rail destinations | Today, Decks, Reader, YouTube, Dictionary, then the drills the language has, then Settings. |
| Rail counts | Today shows new words plus review together. Decks shows the deck count. |
| Rail icons | A placeholder square per destination, in both states. Filled while current. |
| Rail mark "AK" | The signed-in account's initials, and it opens the Profile screen. Filled while signed in, outlined while not. |
| Rail language "FR / EN" | The active pair. Changing it lives in Settings. |
| Topbar | Back button first when there is one, then the breadcrumb, then the screen's own buttons, then search. |
| Topbar search | Hands the word to the dictionary, from every screen that shows it. Off on Dictionary and Settings, which have their own field or nothing to look up. |

The rail carries no Calendar. Nothing in the app collapses the rail by hand.

## Session shell — the seven standalone exercises

| Element | Meaning |
| --- | --- |
| Rail | Five inert placeholder slots, dimmed. No destination is current and nothing is clickable: leaving is the back button, which says so. |
| "← Back" | The only way out, which is why it is a word rather than an icon. |
| Breadcrumb | "Exercise / <name>". |
| Result ticks | The last ten answers, oldest left. Filled is right, outlined is wrong, grey is a card declined, faint is a tick not reached yet. Past ten the window slides. |
| Counter | Cards answered out of cards to answer. Endless drops the total. |
| Score | Running points, on the three exercises that award them. Red once negative. |
| Endless toggle | Flip, multiple choice, typed, audio and correct-incorrect. Not flashcards, not speed matching. |
| "Ask:" | Which side the question comes from. Only on flip and multiple choice, which have no direction of their own. |
| Settings | Present and disabled. There is no design for in-session settings. |
| Mute | Silences speech. The same switch as Settings, not a second one. |
| Countdown bar | What the current answer is still worth, draining with time. Typed, audio and speed matching. |
| "Don't remember" | Records the card as declined rather than wrong, and costs nothing. |

Multiple choice draws no countdown on purpose: six options have to be read before one can be
chosen, and a draining bar buys speed at the cost of the thinking the exercise is for.

Each exercise ends on its own finish panel, since a finished pass leaves no card to draw.

## Training and review — the multi-stage runs

Neither uses the session shell. Both take the whole window, put a header row across the top and
hold the stage's exercise in one rounded panel below; the exercise inside drops its own frame.

| Element | Meaning |
| --- | --- |
| Stage segments | Five for training, four for review. Filled behind, hollow ahead, and the running one widens and fills as the stage progresses. |
| "Complete stage" | A development shortcut, on both runs. |
| "End session" | Leaves and keeps nothing. Leaving by the back button keeps the run resumable. |
| Finish panel | "Training complete", the word count and the elapsed time. Review has no finish panel at all. |

**?** Review ends with nothing on screen. It is the run that promotes and demotes, so it is the
one that has box moves to report.

## 01 Today

The screen answers one question and offers three presses. Everything below them is background:
it gets room, not emphasis, and none of it is allowed to compete for the eye.

| Element | Meaning |
| --- | --- |
| Goal figure | Words finished out of the day's goal, with the track under it. Review and new words both advance it, so a session spent on review is not drawn as standing still. |
| Split | Review due, new wanted, and how many are left. The parts of the figure, stated rather than drawn. |
| Train | Runs on the pool, not a chosen deck. The one filled card, since it is the press the app recommends. |
| Review | Runs on the cards due across every deck. Says Continue while a batch is unfinished, and goes quiet when nothing is due. |
| Search | The dictionary. Same card, no count, because a lookup has no figure to report. |
| Recent decks | Three columns of fixed-size tiles, most-recently-changed first, scrolling past the band's edge. Tiles keep their own proportions: stretching them to fill made a tile's shape depend on how many decks happened to be recent. |
| Recently learned | Words that have left the ladder. The one place the screen reports work already banked. Drawn as rows, the same shape the lookup history uses. |
| Last looked up | The dictionary's own search history. Clicking a row opens the dictionary already showing that word. Chips became rows: both columns are a list of words the learner can press, and drawing one as chips made them read as different kinds of thing. |
| Due next 5 days | Opposite the figure, on the same baseline. One line naming the total, then five rows forward, each carrying the day, a bar and the count. The head carried a title and a total saying the same thing twice, so the title went. The number is the element and the bar is the aside, because the learner is asking how much is coming rather than which day is busiest. A day with nothing due keeps its row. |

The rail's foot carries the last five days, six small bars scaled against the tallest of them.
Backward-looking numbers are habit rather than a decision, so they sit in the chrome and leave the
screen to what the learner acts on. It is also the one part of the window that was empty on every
screen.

The deck column draws its own scrollbar at the column's right edge. A real one appears only under
the pointer and steals width from the grid when it does; this one takes no layout, which is how
QML's overlay scrollbar behaves.

The topbar and the content share one 32px gutter, so the search box lines up with the right edge of
the screen below it and the breadcrumb with the figure's left edge.

Both lists and the deck grid scroll rather than stretch. The goal and the three buttons never do:
open the screen, read one number, press once, and a scroll in the way of that is the screen
failing.

The three column headings share one height whether or not they carry a button, so they sit on one
baseline. Letting a button set the height dropped the headings that had none, and four headings on
four lines is the first thing the eye catches.

There is no weekday name. The bar already says which screen this is, and the date is not something
anyone acts on.

A card is an icon, a label and a number and nothing else. The icon is the same outlined placeholder the rail uses, one step larger, so the label is not the only thing on the card to aim at. Each carried a line of description under its
title, which said what the title already said and turned a row of presses into a row of
paragraphs. The label is all caps in the sheet's mono, the register every other label on the
screen uses.

The three cards carry a hover state: the border darkens to the mid tone, and Train's fill
gets a one-pixel inset ring since a filled card cannot darken its own border. Spent takes
`pointer-events: none`, which keeps hover from ever reaching it rather than overriding it
after the fact.

The three cards are ordinary buttons. An earlier pass drew Train and Review as full-height blocks
carrying the count at poster size; it made a screen of two black slabs and pushed everything else
into a strip along the bottom. The figure is the hero, and the buttons are how it moves.

The thirteen-day strip is gone. It forecast work that could not be started, drew today on a scale
of its own, and spent a third of the screen doing it.

## 02 Decks

| Element | Meaning |
| --- | --- |
| Filters | All, Learning, Not started, Mastered, each with its count. The active one is filled. An empty deck counts as not started. |
| Sort | Most due, Name, Most words, Most recent. |
| Create tile | First cell of the grid. A muted border rather than a dashed one, which a QML rectangle cannot draw. |
| Deck tile | A colour block, the name and size, then three counts and one bar in three parts: still in the pool, on the ladder, past the last box. |
| Import list | A CSV or an Anki export becomes a deck named after the file. |
| Create modal | Name and description, both stored. |

There is no "My lists" and no deck cover image. The three counts sum to the deck's size, which
is what lets the bar be drawn from them without any other measure of progress.

## 03 Deck detail

| Element | Meaning |
| --- | --- |
| Header | An 88px colour block, the name, the description, then the same three counts the tile shows. |
| Header actions | Add word, Export, Delete deck. Delete is absent for the Learning Pool and for a subject deck. |
| Stage pips | Only while a training run on this deck is unfinished: which of the five stages it stopped on. |
| Train | One full-width block. Continue while a run is waiting. A deck of four new words still gives a full session, topped up from the pool and the dictionary. |
| Seven exercise buttons | The seven exercises, run standalone over the deck. |
| Word list | Two columns, filled down the left then down the right. Paged. |
| Word row | The word, its translation, how long until it is due, and its box as a numbered dot. Edit and Delete replace those on the row under the pointer. |
| Filters | All, Not learned, Learning, Learned. Sorted by Added, Last changed, Due or Alphabetical, either direction. |
| Find a word | Narrows per keystroke. A deck runs to hundreds of words and paging to find one is not finding it. |

Review has no entry point here on purpose: it runs over the cards due across every deck at once,
from Today. Two runs drawing from the same due cards could each decide a card's outcome.

The add-word dialog carries the word, its gender, the translation, a machine translation, the
dictionary's own translations as chips, and the deck to add to. Editing a card opens the same
dialog, with Reset progress under it.

## Calendar — dropped

The app has no calendar. Today's seven-day timeline replaced it, and the rail has no such
destination. The fragment has been deleted rather than kept as a drawing nothing matches.

## 06 Dictionary

Laid out as a reference entry, not a set of cards: boxing each meaning made a word with six of
them read as six unrelated results. One entry per part of speech.

| Element | Meaning |
| --- | --- |
| Search field | 560px, centred, with its own Search button. The bar's search box is off on this screen. |
| Entry header | Headword, part of speech, phonetic spelling, the learned mark, then Listen, +, Web. |
| Translations | Every meaning's translations flattened, strongest first, deduplicated. |
| Meaning | Numbered. Gender and the definition on the left, translation chips opposite, split 70/30. Seven senses in ten carry no translation, so that column is dropped rather than left empty. |
| Chip bars | Four rising bars on a log scale for how usual a translation is. One bar is the floor: a translation the corpus rarely saw is still a translation. |
| Forms | How the word is spelled when inflected, which is what a text shows. |
| Examples | Once per entry, not per meaning: the corpus says which word a sentence uses and nothing about which meaning. The sentence, then its translation dimmer beneath. |
| Translates to | A native word with no headword of its own. The translations table is asked instead and the target words come back in teaching order. |
| Not found | Machine translation, then Add manually, Add with translation, Search the web. |

There is no per-meaning voice button and no per-meaning Learn button. Clicking a chip opens the
add-word dialog with both fields filled; the headword does the same with no translation chosen.
The two-column variant the master used to embed is gone — the single-column entry is what was
built.

## 07 Settings

Six tabs. All shows every group in order; each other tab shows one. The cards carry no headings
of their own — the tab names them.

| Tab | Rows |
| --- | --- |
| General | Theme, interface language, foreign language, native language. |
| Routine | Training word count, review word count, sessions per day; then notifications, the time, and the notification text. |
| Session | Ask with, include target-to-native, include typed translation, cards per exercise, required correct answers, require articles, require accents. |
| Sound | Speech, voice, speaker, speech speed, voice cache. |
| Advanced | Examples per meaning, the five review intervals, drop learning status after, reset intervals; then delete everything; then a debug card on a debug build. |

The daily goal is not a row of its own: it comes out of the training word count times sessions
per day, and the sessions row says so. There is no "pause learning" row.

## 00 Setup

What a fresh install opens on. An installed copy arrives with no dictionary and no voice, so
nothing can be learned until this has fetched something. It takes the whole window, since there
is nothing to navigate to yet. The `-onboarding` argument brings it back on a developer build.

| Step | Meaning |
| --- | --- |
| Account | Sign in, or not now. A build with no OAuth client file skips this step outright. |
| Another machine | The database file the other copy keeps everything in, merged rather than copied over. |
| Language | Every language the catalog publishes, plus whatever is already on disk. The right-hand text says what pressing the row costs. |
| Unreachable | A first launch behind a dead connection is the normal case, so it retries every fifteen seconds on its own. |
| Download | The dictionary is queued first and the voice behind it, since nothing needs a voice to start learning. |
| Start learning | Enabled the moment the dictionary lands, whatever the voice is doing. |

## 04 Profile

Its own screen rather than a row in Settings: this is the one place the app says who the user is,
and an account is not a preference. The rail's mark opens it, and no rail item is current while it
is up.

| Element | Meaning |
| --- | --- |
| Avatar | Initials, filled while signed in and outlined while not. The same rule the rail's mark uses. |
| Name line | The name when Google gave one, the address when it did not, and a plain statement when nobody is signed in. |
| Account | Sign in or out. Dead on a build with no client file, since a button that answers a press with nothing reads as broken. |
| Cloud sync | Sync now, and whether one is running. |
| Erase | Opens the erase dialog. Names both sides, since either alone comes back. |
| On this device | The pair being learned and the deck count. Only while signed in. |

## 05 Triage

Sorting new words before they are learned. It runs in the session shell but it is not an exercise:
nothing here is right or wrong, so there are no result ticks and no score. The run ends once
enough wanted words are collected to fill a deck.

| Element | Meaning |
| --- | --- |
| Bar | Collected out of wanted, then already-known and skipped once either happens. |
| Reading | Which sense is being asked about. "moule" is two words, and without this the question has two answers. |
| Translation field | What the card would teach, in the form it will be stored in. Editable, for a translation the grid does not offer. |
| Translation grid | Three by two, the same chip the dictionary uses. The likeliest one the learner has no card for starts ticked, so pressing straight through still produces a good card. |
| Three answers | Know it, skip, learn it. Drawn the same, since no answer here is a failure. |

## 09 Training and review runs

Both draw the frame described above. Each stage embeds an exercise with its own frame dropped, so
the exercise inside is the question and nothing else.

| Run | Stages |
| --- | --- |
| Training | Flashcards, flip, multiple choice, typed and audio mixed, speed matching. |
| Review | The yes-or-no review, multiple choice, typed and audio mixed, speed matching. |

Review's first stage is the one that moves boxes. A "no" demotes the card, and the card still goes
through the rest of the session: the withheld part is the promotion, not the practice.

## 10-12 Dialogs and the notice

| Element | Meaning |
| --- | --- |
| Languages | Opened from the foreign-language row in Settings. A row on disk switches to it, a row that is only published downloads first, and downloads run one at a time, so a waiting row says so. |
| Install from a file | The same archive the app would download, taken off a stick. Matched by its checksum, not its name. |
| Erase everything | The word ERASE has to be typed. A confirm dialog is dismissed by reflex, and this is the only thing that destroys work on every machine at once. |
| Install notice | Bottom right, over whatever screen is up, gone after six seconds. A failure keeps the shape and changes the border. |

## Still not drawn

The deck editor is a placeholder that exists to exercise the store, so there is nothing to draw
yet. The reader, the YouTube screen and the two rule drills are out of scope for this pass.
