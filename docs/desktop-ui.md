# Desktop UI

Rules for every redesigned desktop screen. A screen that breaks one is wrong.

## Scope

| Feature | Desktop |
| --- | --- |
| Everything on mobile | Yes |
| Reader | Yes: the library and the reading view |
| Benchmarks | Developer-only, not in navigation |
| YouTube, drill ladder | Dropped |

## Identity

Desktop uses the mobile identity: palettes, type scale, radii, CEFR and frequency colours. The face is Inter on both, bundled with the app and loaded at startup, since neither Windows nor Android ships it.

| | |
| --- | --- |
| Problem | Desktop and mobile drifted into two looks. Redesigning look and layout together doubles the work. |
| Decision | Keep the mobile look, redesign layout only. |
| Pro | One product across devices. No colour or type work. |
| Con | Some phone sizes need a desktop step. |

- No raw values in desktop QML. Colours come from the theme singleton, sizes from the metrics singleton.
- Tint strengths, fades, tooltip delays and toast time are tokens too. A size borrowed from an unrelated token counts as a raw value.
- Desktop density lives in the desktop branch of the metrics singleton.
- Desktop dark merges into mobile dark.
- A later restyle, such as the square corners of v1, touches only the two singletons.

## Window

| | |
| --- | --- |
| Target | 1280×800 |
| Minimum | 960×640 |
| Review sizes | 960×640, 1280×800, 1920×1080, light and dark |

## Shell

**Top bar.** There is no sidebar. One bar holds the whole shell, left to right:

- The language badge, pinned to the top left corner.
- The screen name as a breadcrumb, for example My Vocabulary. It replaces the mobile back arrow.
- The destinations, icons centred on the window, the same place on every screen.
- The screen's actions: search, the primary action, overflow. When they would reach the destinations they drop their labels and keep their icons, with the label in a tooltip.
- Settings, pinned to the top right corner.

| Destination | Shortcut | Badge |
| --- | --- | --- |
| Today | Ctrl+1 | words due |
| Words | Ctrl+2 | none |
| Reader | Ctrl+3 | none |
| Decks, when the dictionary has any | Ctrl+4 | none |
| Vocabulary | Ctrl+5, or Ctrl+4 without Decks | none |
| Settings | Ctrl+6, Ctrl+, | none |
| Language switch, from the language badge | Ctrl+0, except in an open book where it fits the page | none |

- The table shows the default order. Dragging a destination sideways moves it to another place, and the order is saved. A destination's shortcut is its place in the bar, so Ctrl+1 always opens the leftmost one.
- Destinations are icons with the name in a tooltip. Holding Ctrl swaps each icon, the badge and Settings for their shortcut number.
- The current destination is marked by a short accent bar under its icon.
- Moving between destinations slides the pages sideways, toward the side the new destination sits on in the bar. The new page pushes the old one out, both easing out over 420 ms. Settings counts as the rightmost place. Sessions and their screens cut, and with Animated transitions off every move cuts.
- The theme lives in General settings, not in the bar.
- The language badge opens a popover of the downloaded languages, the current one checked; picking another switches to it. Its last row, More languages, opens Account.
- Account is the first page of Settings: the Google account and sync, every language with its word count, a retake of the placement test for the language being learned, export and import, and a Remove section. Remove deletes one language's words, and in the installer build it also offers Uninstall.
- The language you learn can also be changed from a popover on the General settings page.
- Vocabulary opens My Vocabulary, every card the learner has.
- Decks lists the dictionary's decks as covers, 1.7 times as wide as tall, in as many equal columns as fit: four at 1280, five at full HD. A cover takes its category's tint, one of the theme's eight cover tints, a darker band rising from the bottom to the right edge, and the deck's own icon, and grows slightly under the pointer; it shows the deck name with the word count on the same row, to the right, a line of the learned and learning share, and, under the pointer or keyboard focus, a list chip that opens the deck's words. Clicking the cover starts training on the deck's new words, or opens the deck once it has none; the list chip always opens it. Each category chip and section heading carries a square in the category's tint, and each section divider ends in the category's word total and how many are learned. Filter chips above the tiles pick one group of decks, such as People or Daily Life, with All first. The chips fill the row: the short ones share one width and a chip whose label needs more keeps its own. When they do not fit, they keep their own widths and the row scrolls sideways. They are hidden when the dictionary has fewer than two groups. All shows every group under its own heading, with a divider line running from the heading to the right edge. A deck opens in My Vocabulary's list-detail layout with the deck name in the breadcrumb, and Decks goes back to the list. The primary action is Learn with the count of new words, and it starts triage over the deck.

**Focus mode**, for exercises, sessions, session complete, new-word triage and onboarding.

- Top bar hidden.
- Session bar: back, session name, then forms, edit card, sound and options on the right. It shares the page colour and has no divider.
- The card's buttons sit right under the card, each of one fixed width, centred on it. Their row keeps its place when it is empty, so the card never moves. Sessions have no bottom bar; triage keeps its own. Sessions show no stage progress; the counter on the card tells where you are.
- The card is lighter than the page, and anything recessed into it, the answer well, an option, a field, is darker than the card.
- Exercise card: 792×550, constrained to the available window space. The divider sits at the card's exact midpoint, with the header inside the upper half. Prompt content sits on its bottom edge and answer content starts at the top of its area. The card and divider keep their positions through typing and reveal. Overflow scrolls inside its area. Matching retains its specialised layout.
- Header: tags on the left (part of speech, gender on nouns only, or tense), mastery dots in the middle, card counter on the right.
- A click anywhere on a flashcard, flip or review card advances it, as on mobile; a click on the word itself plays it.
- Prompt zone: the word sits on the bottom edge. Long words and phrases shrink to fit. An article is drawn in the muted colour, apart from its noun. Clicking anywhere in the prompt area plays the word or sentence, and a word gives a short wiggle; Ctrl+Space does the same. Text in the language being learned is medium weight; native text is regular and smaller. A play button sits at the bottom of the zone until the answer; then the revealed word or sentence and its translation take its place. Every play button shows its key.
- A speaker icon pulses while the speech it started is playing.
- Flashcards and revealed reviews show the translation, and under it the meaning's example: the sentence in the language being learned on the left, its translation on the right, split by a thin line. Both read left-aligned, and the left one ends at the line. A meaning without an example shows its English definition instead.
- Sentence exercises show no tags.
- Answer area: options in fixed cells, the text aligned left after its key, or the typing field at the top of the area. Typed cards reserve the answer lines under the prompt before reveal, so revealing an answer does not move the prompt. A wrong answer keeps its red outline and the accepted answers show in green. Voice cards replace the voice box with a word slot of the same height.
- A sentence gap shows no play button; the sentence is already on the card.
- A match without a prompt centres its grid in the card. A forms match keeps its compact prompt above the divider and its original two-column grid below it.
- A sentence gap fills with the letters as they are typed.
- Sentence text fits the available prompt area at 18–24 px, with native text at 16–20 px. Fitting uses the complete sentence, so typing and reveal keep the same font size. Text that still exceeds the area scrolls.
- Word tiles sit in the prompt area under the play button; the sentence being built sits in a slot under the divider.
- Tile banks and answer slots grow to at most three rows, then scroll. Used bank tiles keep their positions. The answer slot follows its newest row.
- Forms study shows each form in its own cell, singular and plural in two columns, the ending in the accent colour.
- A card that hides its answer shows a well with a question mark where the answer will appear; Reveal sits in the action bar.
- Right and wrong show only as the green or red outline of the answer. There is no verdict line.
- No instructions or hints on exercise screens. The layout says what to do.
- The next card replaces the last one with no animation.
- Forms in the session bar, an info icon, opens a popover with the table of the word on screen. It shows in the match and typing stages of forms training, and in typing it waits for the answer to be revealed.
- Edit card in the session bar opens the edit dialog over the session. Cards that cannot be edited hide it, and forms training never shows it, since its cards are generated.
- Options: play words aloud, restart the stage, skip to the next stage, end the session.
- Escape and Back leave without asking. Leaving keeps the session as it is; Today shows Continue.
- A session started from a Today tile opens with the tile growing into the exercise card. The top bar slides up out of the window, the session bar slides down into its place, and Today fades out under the card. The card's content fades in once the card lands. Sentences and Conjugations grow from their start dialog. Leaving runs the same move backwards, into the tile. Sessions started anywhere else, new-word triage, and leaving a finished session cut without it, and with Animated transitions off every session cuts.
- A window shorter than the card's design height tightens the card's spacing, so the prompt keeps its room.
- Sentence exercises use the same card. Every word of a sentence is a link; clicking one opens the lookup panel over the session's right edge; the card stays where it is. It is the Reader's panel: a search field holding the clicked word, and the dictionary results for whatever the field holds. Ctrl+F opens it empty.
- Onboarding steps keep the bottom progress bar, one segment per step. New-word triage uses the action bar for Skip and Learn it.
- New-word triage is laid out like a dictionary entry, still in focus mode, in the dictionary entry's width and centred on the window: the shared entry header with the word centred and larger, the translation field, then the meanings in one column. Meanings open and close with a short height animation. A closed meaning is two lines: its gist and English definition, then its translations, with the chosen ones in the accent colour. One meaning is open at a time and shows both definitions and translation chips; clicking a closed one opens it. The first meaning opens with its first translation chosen, so Enter learns the word straight away. Picking chips of one meaning clears the others; opening another meaning keeps the choice. The list runs to the bottom bar and fades out under it; the bar holds Skip and Learn it, centred on the window. An Added chip with the counter in the session bar slides a read-only list of the cards added in this triage over the left edge; the app remembers whether it was open. A From chip beside it, showing the start rank, slides a pane with a vertical word range slider in from the right edge; the app remembers whether it was open, and both panes start closed. The slider sets where triage starts: teaching rank on a curved scale, labelled in thousands, with the CEFR levels placed at their median rank. The wide track fills from the bottom up to the handle in the level colours. It stops a thousand learnable words short of the rarest, and a move that would leave nothing to offer goes back to the previous value. The placement test sets its first value; moving it saves the new value and refills the queue from there.

## Content width

| Content | Maximum width |
| --- | --- |
| Hub, collection | 1120, centred |
| Reader library, Decks | 1600, centred, as many covers per row as fit |
| Dictionary entry, settings page, form | 720 |
| Exercise card | session card width |

## Emphasis

- A screen has at most one filled control, and it is the next thing the learner should do. On Today the filled tile walks the path: Review, then Learn, then Conjugations once the goal is reached, or Sentences when no verbs are ready.
- A confirmation that throws away progress fills the choice that keeps it. Leaving a session fills Keep going.
- A light fill marks the chosen option in a set: filter chips, the selected button.
- The accent colour marks a state: a switch or toggle chip that is on, the current place in the sidebar or a list, and anything due.
- Level, frequency and due badges are tinted, so they never outshine the word next to them.
- A word already in My Vocabulary says so beside the entry, and its Add word button drops to outlined.

## Screen patterns

Every screen uses one of these. No new patterns.

| Pattern | Screens | Layout |
| --- | --- | --- |
| Hub | Today | One block on the page itself, no cards, a little above the middle, in three columns. Left: the daily goal as a double ring, review progress outside and today's learning inside, as on mobile. Middle: Review and Learn stacked, each title behind a dot in its ring's colour. A bracket between the ring and the tiles joins the ring at its middle and holds a dot for each tile; a finished step fills its dot and the line into it. Right of Review: the coming days as small tiles. Right of Learn: this week, Monday to Sunday, a small double ring for each day so far. Below, on the same two columns: Grammar, with Conjugations and Forms, and Practice, with Sentences and a Practice tile that opens the practice dialog. The bracket runs on, dashed, into Conjugations, since grammar sits outside the daily goal, and an arrow leads from Grammar to Practice. A locked tile has a dashed outline, a lock for its icon, and says what to learn |
| List-detail | My Vocabulary, dictionary search, settings | Fixed list pane left, selection right. No screen push. Arrow keys move selection. My Vocabulary and an open deck filter their list with the mobile progress bar: one bar split into not started, learning and learned, over three counts that each show one group, and a second click shows all again. A dictionary entry with three or more meanings shows them closed, in the same two lines as triage; a click opens one, and several can be open at once. Below the translations, a verb shows its conjugation and a noun or adjective its forms, in the cells of the forms training study card: the cue before each form and the ending in the accent colour. A verb picks its tense from a four-column grid of chips, the indicative tenses on the first row and the other moods after them, and lists its participles under the cells. Endings are marked only where training knows them; other tenses show the plain form, and a missing form shows a dash. |
| Focus | Exercises, sessions | One centred card, answer controls under it |
| Reading | Reader | The library is a grid of book covers the size of the deck covers, as many columns as fit, each crossed by the decks' darker band and growing slightly under the pointer, as deck covers do, nothing under them, Open book first as a plain + in the primary button's colours, sorted by last accessed or alphabetically. A search field above the grid, which Ctrl+F focuses, keeps the books whose title holds the typed text, ignoring case and accents, and hides Open book while it does. Each book takes one of eight cover tints the theme defines for light and dark, picked from its title. An open book fills the screen: the page in the middle, a position bar under it, contents as a pane on the left, and a translation popup under the selection. The top bar shows Reader and the book title, and Reader goes back to the library. |
| Form | Add word, edit word, settings page | Labels above fields, one column, primary button bottom right |

## Mobile sheets on desktop

Use the lightest surface that fits.

| Mobile | Desktop |
| --- | --- |
| Filter, sort | Inline chips on one row that scrolls sideways with the mouse wheel, popover on the sort button |
| Word card swipe actions | Hover buttons on the row, context menu |
| Exercise options | Popover from the session bar |
| Practice one exercise over My Vocabulary | Dialog from the Practice tile on Today: exercise tiles; New, Reviewing and Mastered chips that pick which words to practise, any number of them on, each with its word count; cards and repetitions side by side; Cancel and Start |
| Tense picker | Dialog of tense tiles, each with its verb count; a tense not ready yet shows how close it is as a bar. A click starts the tense. |
| Language picker | The language badge for downloaded languages, the Account page of Settings for the rest |
| Word explain | Side panel over the session's right edge |
| Sentence start | Dialog from the Sentences tile on Today: exercise tiles, difficulty chips and sentence count side by side, Cancel and Start. A popover anchored to a tile low on the page ran past the window edge. |
| Time picker | Popover |
| Dictionary update | A card at the bottom of the Settings category list |
| App update | A card at the bottom of the Settings category list, and a status row on the About settings page |
| Onboarding | Focus screens, one centred form per step, the next step's button pinned bottom right |
| Sync result | Toast, bottom right |
| Reader translation | Popup under the selection, opened by a Translate button beside it: the dictionary word row for an exact match, otherwise the machine translation, or one download button without the model. One row of icon buttons under it. Opening the word in Words keeps the book open. |
| Reader contents | Side pane on the left of the page |
| Reading settings | A page colour switch in the top bar while a PDF is open, cycling paper, sepia and night. Ctrl+plus and Ctrl+minus change the text size of other formats. The full list is the Reader page of Settings. |
| Reader row swipe actions | Hover buttons on the tile: upload, show in folder, remove from this device, delete from the cloud |
| Reader open button | An Open book tile, first in the grid, and files dropped on the library |
| Reader full screen | Hide controls in the top bar: the top bar and position bar go away until Escape |
| Add word, edit word | Centred dialog |
| Delete data | Modal confirmation |

**Start dialogs.** Practice, Sentences and Tense show their choices as tiles in two columns, recessed into the dialog like exercise options. With an odd count the first choice, the default, spans the row. The chosen tile takes the accent outline. Settings sit under a divider, two to a row.

**Welcome.** Every launch that lands on Today opens on a welcome screen: a greeting by time of day with the first name of the signed-in account, and today's review and new word counts. It holds for a second and a half, or until a click or key press, then gives way to Today. Onboarding, walks and the dictionary benchmark skip it, and with Animated transitions off it cuts straight to Today.

- Desktop shows Today's daily goal ring at twice its size, on Today's background, with the greeting to its right: the date above it, and the two counts under it, each behind a dot in the colour of its ring. The rings fill as the screen opens and the lines rise in one after another.
- Leaving, the greeting fades and the ring shrinks into its own place on Today. Today appears around it once it lands.
- Mobile keeps the centred greeting with the two counts under it.

**Updates.** App updates and newer versions of installed dictionaries, voices and translation models appear on Today and at the bottom of the Settings category list. Each card has a download arrow, a title, the size or progress under it, and its action across the card. Downloads start only after a click. Today cards stay across navigation and restarts until installed or closed with ×; closing records the offered version and leaves its Settings card available. The Today row and Settings list scroll when offers exceed the available space. While an update waits, the Settings button in the top bar carries a dot.
- Settings, About keeps an Updates row with the status and Check for updates.

## Interaction

| Input | Effect |
| --- | --- |
| Hover | Every clickable element shows a hover state |
| Tab | Visible focus ring |
| Escape, click outside | Closes popovers and dialogs. A confirmation closes on Escape only. |
| 1 to 9 | Picks an exercise option. In a match, 1 to 5 pick the left column and 6 to 0 the right. A six-row match uses 1 to 6 on the left and Q to Y on the right. |
| Ctrl+D | Don't know, on every exercise that offers it |
| Backspace | Skips the word in new-word triage, unless the translation field is focused |
| Up, Down, Left, Right, Space in triage | Up and Down open the previous and next meaning, Left and Right move between its translations, Space adds or removes one. Not while the translation field is focused. |
| 1, 2 | Forgot, knew it, in review. No other key grades a review. |
| Enter | Presses the filled button on every screen: Reveal, Check, Continue, Next, Learn it, Done |
| Left, Right | Previous and next flashcard, and nowhere else in a session |
| Ctrl+Space | Plays the word or sentence on every screen that can speak it: exercises, triage, the dictionary entry, the lookup panel. It works while typing. |
| Up, Down | Move the selection in a list from its search field |
| Ctrl+F | Focuses the screen's search. It does nothing in an open book. |
| Ctrl+N | Creates the item the screen lists. In the Reader library it opens a book. |
| Ctrl+1 to Ctrl+5 | The destinations from left to right |
| Ctrl+6, Ctrl+comma | Settings |
| Click, drag in a book | A click selects the word and a drag selects whole words. Any selection shows a Translate button beside it, which opens the translation under the selection. |
| Left, Right, Page Up, Page Down, Space in a book | Previous and next page |
| Up, Down, wheel in a book | Scroll. At the end of a PDF page the wheel turns the page. |
| Ctrl+wheel, Ctrl+plus, Ctrl+minus, Ctrl+0 in a PDF | Zoom in, zoom out, fit the width |
| Ctrl+plus, Ctrl+minus in a text book | Larger and smaller text |
| F11 in a book | Hides and shows the controls |

Lists show a thin scroll bar.

Every side pane whose width is not set by its content drags to resize from its inner edge: the Settings categories, the My Vocabulary and Words lists, the triage Added list, the Reader contents, and the dictionary pane in sessions. Each pane remembers its own width.

## Build order

Core screens set the patterns. The rest follow them.

| Step | Screens | State |
| --- | --- | --- |
| 1 | Shell and navigation | Built |
| 2 | Today: hub | Built |
| 3 | My Vocabulary: list-detail | Built |
| 4 | Words, dictionary entry: reading layout | Built |
| 5 | Training and review sessions, the eight exercises they run, session complete: focus. The MVP. | Built |
| 6 | Settings: category list beside a form page | Built |
| 7 | Reader: library and reading view | Built |

Every mobile feature in scope has a desktop screen.

## Code

- Location: `app/qml/desktop/`, `Desktop` prefix, one component per file.
- A look used in two places becomes a component: the scrolling page, the side pane, the selected row, the dropdown button, the divider.
- Shared with mobile: the two theme singletons, icons, controllers, and the unprefixed files at the root of the QML folder. Nothing else.
- Each old desktop file is deleted when its replacement lands.
- Every interactive item declares an accessible role and name. Windows UI Automation sees nothing else, so screen readers and the review tool would miss it.
- Review: launch the app through the Ghost MCP server, which starts it on a hidden desktop, so a review never takes the user's focus. Ghost reads the same UI Automation tree, clicks and types by accessible name, and screenshots the window while it stays hidden. Screenshot each screen at every review size. Ghost cannot resize a window, so set each size with the `size W H` request below.
- A seeded sandbox launch: `-headless -database-file=<path to a database>`. Adding `-onboarding` shows onboarding in it.
- A locked desktop never paints the window, so outside screenshots go stale. Set `IKONIKA_SNAPSHOT_DIR` to a folder and write a file named `request` into it: each line is `size W H`, `click objectName`, `hover objectName`, `clickat objectName x y`, `key Return`, `type text`, or an image name the app renders its own window into. Window shortcuts need an active window, so drive buttons by name, not by keys.
- To take a screenshot through Ghost, follow these steps in order. Never use `ghost_desktop_capture` for screenshots: it returns the PNG as about 195,000 characters of base64, which exceeds the tool result limit.
  1. Create the desktop: `ghost_desktop_create` with id `ikonika`.
  2. Launch the app on it with `ghost_desktop_launch`, desktop `ikonika`, and this command. The variable must be set here, since the app reads it once at startup.
     ```
     cmd /c "set IKONIKA_SNAPSHOT_DIR=<folder>&& <path to IkonikaApp.exe> -headless"
     ```
  3. Wait for the window with `ghost_desktop_wait_for_window`, title `Ikonika`.
  4. Write the `request` file into `<folder>`, with the size on the first line and the image name on the second.
     ```
     size 1280 800
     today.png
     ```
  5. Wait about three seconds. `today.png` appears in `<folder>`. It renders at the display scale, so 1280×800 comes out as 2240×1400.
  6. Send the PNG to the user with `SendUserFile`.
  7. Close the desktop with `ghost_desktop_close`.
- `-Walk` runs `DesktopNavigationWalk` and a screen walk over today, vocabulary, words, reader and settings. Controls the walk presses carry an `objectName`.
