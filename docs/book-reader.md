# Book reader

## Summary

The reader opens a local document, extracts or renders its content, and lets the user pick a word or phrase for the same dictionary lookup the Dictionary screen uses. A picked word or phrase, or one typed by hand, ends at the same add-word popup the rest of the app shares.

The reader runs on the phone and on the desktop. Both share the library, the extraction and the PDF rendering described below; each has its own screens, and [Desktop](#desktop) lists where they differ.

## Library

A document picked inside the app is never copied. Opening it stores its path, title and format; the file stays where it is, and the app keeps the read permission Android grants for it.

A document sent from another app through "Open with" is different. Most senders grant access for that one launch only, so a stored path would stop opening after a restart. When the sender allows a lasting permission the app keeps it; otherwise it copies the file into its own storage and stores that path. The app stores nothing derived from it: a flowable document is re-extracted on open, and a PDF is re-rendered on open.

The library is local-only. Cloud sync merges the card tables, but a document's path is meaningless on another device, so the library never leaves the machine. Existence is re-checked on every list read; a missing file is marked as such rather than dropped, and the user removes the dead reference explicitly.

The library view is a grid of covers, two columns on the phone, with Open book always first. The phone can switch it to a list. A search field keeps the books whose title holds the typed text, ignoring case and accents, and chips sort by last accessed or alphabetically.

## Cloud books

A signed-in learner can upload a book to the cloud, and every device signed in to the same account lists it and can download it.

- **Where it lives.** The file goes to Drive's hidden app folder, the same place as the workspace copy, so it needs no new permission. It counts against the user's Drive storage and does not show in Drive. Its name starts with `ikonika-book-`, and the workspace copy lookup leaves those files out.
- **What syncs.** A synced table lists each book: title, format, size, the Drive file, and where the reader left it. The device library links a local copy to its cloud book.
- **Upload.** A resumable upload sends the file in one stream with progress. The book keeps its local file where it is.
- **Download.** The file streams into the workspace's books folder with progress, and the book opens at the place it was left on any device.
- **Reading place.** A device writes the place to the cloud when it closes the book, when the app leaves the foreground, and on quit. On opening, a cloud place newer than this device's last opening wins. A book that cannot be read, or is still loading, writes nothing, so it never overwrites the real place.
- **Book length.** The reader records how many pages or paragraphs the book has, on the device and in the cloud row, so a cover shows how far it was read even on a device that has not downloaded it.
- **Cancel.** A running upload or download can be cancelled from its cover. A cancelled download leaves no partial file.
- **Remove from this device** forgets the local copy and deletes the file when the app downloaded it. The book stays in the cloud.
- **Delete from the cloud** asks first, deletes the Drive file, and marks the book deleted. Each device then drops its copy at the next sync.

An app version from before cloud books treats every file in the app folder as a workspace copy, merges it and deletes it. A device on an old version deletes uploaded books, so every device needs this version first.

## Free books

Ikonika publishes public-domain books for each language, and the learner downloads them from the
Free books shelf. The list comes from the language's book catalog; see
[releases.md](releases.md#book-catalogs).

- **The filter.** A dropdown beside the sort controls switches between My books, the learner's own
  library as it was, and Free books, the catalog for the language being learned. The dropdown shows
  only when that catalog lists at least one book. Free books keeps the catalog's order, has no Open
  book tile, and the search field still filters it. Free books replaces the sort controls with a
  reading-level filter: All levels, Beginner, Intermediate or Advanced. The level filter combines
  with title search and keeps the catalog's order. A level badge sits at the top right of each
  free book's cover and stays on downloaded copies in My books. Badges use A, B and C for Beginner,
  Intermediate and Advanced; filters and tooltips keep the full names. On desktop, available
  actions replace the badge in the same corner on hover or keyboard focus. Sorting is dimmed while My books
  holds fewer than two books.
- **Download.** A free book that is not downloaded shows faded with the download icon. Clicking it
  downloads the file into the workspace's books folder, checks its checksum and adds it to the
  library under the catalog title. The cover shows the same percentage and Cancel as a cloud
  download.
- **After download.** The book shows in both filters as a normal book on this computer. It cannot be
  uploaded to the cloud, since every device can download it from the catalog.
- **Remove** drops the book from the library and deletes the downloaded file. The book stays on the
  Free books shelf to download again.

The phone has the same shelf. Two chips, My books and Free books, sit under the search field when the catalog lists a book. A reading-level picker replaces the sort chips on Free books. The same level badge appears on covers and in the list layout. A downloaded cover opens its actions sheet on a long press, keeping the badge's corner clear. A catalog book has a download icon and no actions menu; a tap downloads it with the same percentage and Cancel on the cover, and a failure shows as a toast.

## Pipeline

A PDF page is a fixed visual composition: columns, headings, images, exact typography. TXT, FB2 and EPUB carry no fixed layout; they are a stream of text meant to reflow to whatever width shows it. One pipeline cannot serve both without loss: reflowing a PDF's extracted text discards the layout it came from. The two formats route through two different mechanisms, converging on the same add-word popup.

| | PDF | TXT / FB2 / EPUB |
| --- | --- | --- |
| What is shown | vertically stacked pages, rendered and scaled | extracted text, reflowed to the view width |
| Rendering | this app's own renderer, on a thread of its own | a virtualized list, one paragraph per row |
| Layout source | the PDF itself | the format's own extractor |
| Word selection | the PDF's own text layer | the paragraph text |

The format is detected from the file extension and stored with the library entry; it also decides which of the two views opens.

## PDF Implementation

The phone and desktop show a continuous vertical stack, with only the previous, current and next page active. Crossing the middle of the view changes the current page and recycles the farthest slot without moving the text on screen. On the phone, a pinch zooms around the point between the fingers, and a double tap zooms around the tapped point. The page cannot be zoomed out below the width of the view. Horizontal dragging pans a zoomed page.

Each library entry keeps its last page, zoom and scroll offset, so a reopened book shows what was on screen when it was left. The offset is stored in PDF points, which depends only on the zoom: the page's own size is measured asynchronously after opening, and a fraction of the page would land wrong until it arrived. The offset is re-applied once that measurement lands, unless the reader has already moved the page.

A page that paints no background of its own renders transparent, so every bitmap is composited onto white. A page whose size differs from the first page's is drawn at the first page's size until it is measured; a render made before that measurement is discarded on arrival and asked for again.

An earlier scrolling stack realised many pages at once, so one zoom step re-rendered every one of them. The current stack reuses three page slots sharing one document and render worker. Text lookup uses a separate document, so it does not queue behind rendering.

The current design:

- **A dedicated render thread owns its own copy of the document.** Nothing the interface asks can queue behind a render, because the interface asks a different document object entirely.
- **Rendering clips to the visible region**, padded slightly so small pans need nothing new. Cost then scales with window size rather than zoom level, so a deep zoom costs what a shallow one does.
- **Every frame redraws the bitmap already on hand, at the current zoom.** A wheel notch changes geometry, applied within a frame; the sharper bitmap replaces the stretched one once the render finishes. Blocking on the new bitmap before drawing anything is the alternative, and reads as latency.
- **A cheap whole-page bitmap sits behind the sharp one**, so zooming out or scrolling past the sharp region still has a preview to show. Each of the three active page slots requests its own preview. Recycled slots discard cached pages that are too far from their new page.
- **A newer request supersedes a queued one of the same kind.** A spun wheel produces a burst of render requests; only the last is worth completing.
- **Page size is measured lazily, the first time a page is shown.** Measuring every page's size at document load cost one and a half of the two seconds it took to see anything, because asking the PDF library for a page's size forces it to parse that page.

The three slots change their page assignments as the reader scrolls. Additional neighbour prefetching is disabled because both neighbours already have their own slots.

The reader does not reflow a PDF's text to a narrower column. That requires reconstructing reading order from visual layout, a lossy, heuristic feature of its own rather than a rendering fix.

A PDF is named from its own title metadata when that reads as a title. A file handed over by Android often has a meaningless name, so the metadata is the only real title there is. A title that is empty, a file name, or a tool's placeholder such as "Microsoft Word - ..." is ignored, and the file name is used. A book added before this rule is renamed the next time it is opened. This applies on the phone only. The desktop names a PDF by its file name, since a file the user picked keeps the name they gave it, and a library entry from before is renamed when the library is read. A downloaded cloud book keeps the cloud title, since its file is named by its id.

## Supported Formats

TXT, FB2 and EPUB each have their own extractor, producing a title and an ordered list of paragraphs. Extraction is cheap text processing, not rendering. The reader shows the paragraph list in a virtualized view, which realises rows only near the viewport and recycles them on scroll; live row count stays roughly constant regardless of document length. A click resolves the word under the cursor; a drag resolves a selected range as a phrase.

A click or a tap picks a word only when it lands on one of the word's letters. The space between two lines is split halfway, so a click just under a line belongs to that line. A click on the space between words, beside a short line or between paragraphs picks nothing; on the phone such a tap shows or hides the reading controls, and on both it clears the selection. Only one word or phrase is selected at a time. A drag selects whole words: on desktop, a selection that starts or ends inside a word grows to cover that word when the mouse is released, so sentence translation never receives a cut word. The reader resolves the letter itself rather than asking Qt, because Qt counts only a line's glyph height as the line: with line spacing above 100% the extra space under each line belonged to no line and resolved to the first word of the next one, and a click on the right half of a word's last letter resolved to the space after it.

The unit is the paragraph, not the sentence. Sentence segmentation belongs to in-context translation and to read aloud, not to the layout.

FB2 and EPUB are laid out from their own styles. Each paragraph comes out twice: as plain text, which word picking, selection and search work on, and as a small piece of rich text with the inline styling and line breaks, which the desktop and the phone views both show. The two line up character for character, since a line break is one character in the plain text. Text on both sides of an inline tag joins without an added space.

An EPUB brings its own stylesheets, and the extractor applies them the way a browser would, while it walks each chapter. A paragraph is matched against the book's rules with its real parents and the element before it, so rules such as `body > p` and `h2 + p` work, which the text renderer could not do alone, since it only ever sees one paragraph. The result is a resolved style per paragraph: first-line indent, alignment, space above and below, side margins, font size, bold, italic, small capitals and letter case. Side margins and padding add up through nested blocks, as in a browser, so a hanging indent such as `padding-left: 1.5em; text-indent: -1.5em` and a letter boxed in 5% margins both come through. The margins of `html` and `body` are left out, since the reader's page width already sets the column. Inline elements carry only what differs from the text around them. A browser's default styles come first, such as italic for `em` and larger bold headings, then the book's linked and embedded stylesheets, then each element's inline style. An FB2 has no stylesheet, so it gets a small default one: centred bold titles and indented paragraphs. Opening an EPUB logs, at debug level, the CSS the reader ignored: each unknown property and each value it could not parse, with the number of elements it reached, and each selector or at-rule it skipped. Run a new book through that log before publishing it, to catch styling the text depends on.

What the extractor understands: type, class and id selectors, joined by descendant, child and adjacent-sibling combinators, and `@media` rules for screens. A rule with a pseudo-class, an attribute selector or a general-sibling combinator is skipped, and so is a print rule. Widths are not applied, so a block centred by `width` and `auto` margins, such as a verse block, sits at the left margin. Lengths in em, rem, ex, px and pt resolve against the text size, and a percentage against the width of the text column, so `text-indent: 2%` widens with the page. An element hidden with `display: none` is left out.

An EPUB's pictures become rows of their own: an `img`, or an `image` inside an `svg` as Gutenberg uses for scanned covers, that starts a block. The row holds the picture's path inside the book and its size, read from the PNG, JPEG or GIF header while the book is extracted, so the list knows the row's height before the picture loads and scrolling stays steady. Both the desktop and the phone views load a picture only when its row comes into view, straight out of the EPUB through an image provider, and scale it to the text column and to at most 80% of the view's height. On the phone the book is usually a content URI rather than a file path, so the provider opens the archive through Qt's file layer and reads it in place instead of with a plain file open. A tap on a picture shows or hides the reading controls, like a tap on empty space. A horizontal rule, `hr`, becomes a row of its own too, drawn as a faint centred line a third of the text width, with the rule's own margins above and below. A picture inside a line of text, such as a decorative initial, is left out, and so is a picture the book does not contain. Captions are ordinary paragraphs.

The reader keeps its own colours, line spacing and page width over the book's, and the font the reader picked. The space between two paragraphs is the larger of the first one's bottom margin and the second one's top margin, as in a browser. A TXT file has no styles and keeps the plain layout: equal space between paragraphs and larger headings.

## Popups

A tap on a word, a drag across a phrase, and the search button all open the same sheet: the Dictionary screen's own search, with its field and result list. The search lowercases, folds accents and matches inflected forms, so a capitalised or conjugated word in the text finds its headword. A result opens the dictionary entry or the shared add-word screen, which writes the card into My Vocabulary.

For a scanned PDF with no text layer, the search button opens the sheet's field for typing a word. The same field accepts words encountered outside the document.

The sheet opens only once the search has answered, so it never shows the previous word's results first.

A selected phrase is rarely a dictionary headword, so the search usually finds nothing. In that state the sheet offers "Add as a word" with the translation typed by hand.

The reader has no search through the document's own text. The search button looks words up in the dictionary.

The desktop has no dictionary pane and no search button in a book. A click on a word opens the translation popup under it straight away. A drag selection shows a Translate button beside it once the mouse is released, and the button opens the popup once the dictionary has answered. The popup shows one of three things:

- **Dictionary match.** Up to three words are looked up first. When a result matches the selection exactly or as an inflected form, the popup shows that result as the same word row the dictionary lists use: headword, translations, level and learning progress. A click on the row opens the add-word dialog for it, the same as the add button.
- **Machine translation.** A selection with no exact match, or a longer one, is translated by the offline model and shown in large text.
- **Download.** Without the translation model, the popup is a single download button. When the download finishes with the popup still open, the selection is translated without being selected again.

The dictionary match and the machine translation end in one row of icon buttons: listen, copy for a machine translation, open in the dictionary for a match, and add to My Vocabulary. Add opens the add-word dialog with the translation and the hint. The hint is the part of the sentence around the word, cut at the nearest comma, semicolon, colon or sentence end, and a hint in all capitals is written in sentence case. A selection of several words with no dictionary match is added as a phrase, and a whole sentence gets no hint, since it would repeat the card itself. The icons name themselves in a tooltip only.

The hint sentence and machine translation split text into sentences by the rules of the language being learned. Qt's own splitter ends a sentence at every period followed by a capital letter, which cut "M. Dupont entra." after "M.". A period after one of the language's abbreviations, such as "M." and "av." in French, "Sra." and "pág." in Spanish, or "Mr." in English, does not end the sentence. Neither does a period after a single capital initial, as in "J. R. R. Tolkien". A question or exclamation mark that the same sentence continues in lowercase, as in "Vraiment ? dit-il.", does not end it either. A dash or opening quote that starts the next line of dialogue belongs to that next sentence. "etc." is left off the abbreviation lists, since it usually ends the sentence it closes.

A word popup is as wide as a menu. A sentence popup is at least half the text column wide, and as wide as the selection when the selection is wider. The popup has no close button: clearing the selection or pressing Escape closes it.

## Vocabulary highlighting

EPUB, FB2 and TXT books can mark the learner's own words on the page. The setting is called Highlight my words, it sits with the other text reading settings, and it is off by default. A PDF is never marked.

- **What is marked.** Every word in My Vocabulary. A word in the pool or on the ladder is shaded, and a finished word is underlined in the ink colour.
- **The shade fades with progress.** A word in the pool gets the strongest shade, and each ladder stage fades it a step, down to a third of that strength at the last stage. The darker a word, the less the learner knows it, the same idea as LingQ's fading levels. Unlike LingQ, a word moves only through exercises, never by turning a page.
- **Blue on light pages, amber on dark ones.** The shade is the accent blue on white, sepia and the light theme. A faint blue is dark, so on a dark page it sinks into the page and dims the text on it. A faint amber is bright and lifts off a dark page, so dark pages use amber, a little stronger than the blue. A page counts as dark when its ink is light.
- **Forms count.** Every inflected form the dictionary lists for a card's headword is marked too, so "allait" shows as "aller" and "petites" as "petit". A card with a part of speech takes only that reading's forms.
- **Phrases.** A card of several words, such as "tout à fait", is marked only where its words follow each other with nothing but spaces between them.
- **Elision and hyphens.** "L'homme" marks "homme", and "dit-il" marks "dit". A word written whole, such as "aujourd'hui" or "peut-être", is matched whole first.
- **Two cards, one spelling.** When cards share a spelling, the least known one decides: a shaded card wins over a finished one, and the earlier stage wins between two shaded ones.
- **Case and accents.** Case is ignored, accents are not: "a" and "à" are different words.

The marks are rebuilt when a book opens, when the reader screen comes back into view, and when the app returns to the foreground, but only when the card data or the dictionary changed since the last build. A build for a few thousand cards reads one forms query per five hundred headwords. Marking a paragraph is a lookup per word, done for the paragraphs on screen only.

## Read aloud

EPUB, FB2 and TXT books can be read aloud with the voice the app already uses for words: Supertonic on the desktop, the platform voice on the phone. A PDF has no read aloud.

- **Starting.** The headphones button in the reader's top bar starts reading at once and opens the play bar. Pressing it again closes the bar, the same as the bar's close button.
- **The play bar.** Previous sentence, play or pause, next sentence, the speed, and close. Previous replays the current sentence when it has played for more than two seconds, and goes one sentence back otherwise, so a second press steps back. The speed steps through 50, 75, 100, 125 and 150 percent. It is the sentence speech rate from the sound settings, which the reader's Appearance panel also shows as a slider. A new speed restarts the sentence being read, so it applies at once.
- **Where reading starts.** Play reads what is on screen. It resumes the paused sentence when that sentence is visible, continues from the sentence read last in this book when that one is visible, and otherwise starts from the sentence at the top of the screen. The sentence read last is kept per book on this device.
- **A tap always translates.** Tapping a word opens the translation, the dictionary sheet on the phone and the popup on the desktop, whether the voice is reading or not. A tap while reading pauses it, and Play then restarts the paused sentence, so the learner hears it again after the lookup. The phone's voice cannot pause in the middle of a sentence anyway. The popup and the sheet offer Read aloud from here, which starts at the tapped word's sentence.
- **Back to reading.** Scrolling by hand while the bar is open stops the view from following. When the sentence being read leaves the screen, a Back to reading button above the bar scrolls to it and follows again.
- **Keys on the desktop.** While the bar is open, Space plays or pauses, and Ctrl+Left and Ctrl+Right move by a sentence. Without the bar, Space turns the page.
- **No voice.** When speech is turned off, or the desktop voice is not installed, the headphones button opens a notice in place of the bar: Turn on, or Download with its progress. Once the voice becomes available, reading starts.
- **Dialogue dashes.** The spoken text puts a space on both sides of every em-dash. The rule was written for Piper, which paused at a comma or an exclamation mark only when whitespace follows it, and the app still applies it. Spanish dialogue writes them as "¡Oh!—dice el pollo,—el cielo", so without the spaces the voice ran straight through.
- **The sentence is shaded, not the word.** Neither desktop voice reports when each word starts, and an estimate drifts with French liaisons and fast Spanish. Shading the sentence is exact on every platform.
- **The view follows.** When the read sentence goes past the bottom of the visible text, the view scrolls it near the top. Scrolling by hand stops following until play is pressed again.
- **Leaving.** Closing the book stops reading. Leaving the reader screen pauses it. On the desktop a minimised window keeps reading.
- **The phone in the background.** Reading goes on with the screen off or another app open, the way music does. A media notification shows the book's title with play or pause, and stop, and the same controls appear on the lock screen. Headphone buttons and other media keys work. A phone call or another app's audio pauses reading, and it resumes when the call ends. Unplugging headphones pauses it. Swiping the app away from recent apps stops it.

The phone's background reading needs four things. Qt normally freezes the app's event loop in the background, which would stop the queue from refilling after 15 seconds, so the manifest turns that off. A foreground service of the media playback type keeps Android from killing the app, and it stays in the foreground from the first play until stop, since Android does not let a background app start one later. A media session carries the notification and the lock screen controls. The platform voice plays from its own process, so Android would send media keys to whichever app last played audio of its own; a silent audio track plays in the app while reading, which makes the app the one media keys go to.

Sentences are queued about 15 seconds ahead, measured in estimated seconds of audio, not in sentences. A short "Oui." before a long sentence would leave a gap if only one sentence were prepared, since the long one takes longer to synthesise than the short one takes to play. The desktop synthesises the queue on a worker and appends each sentence to the one still playing, with a short pause between them. That audio skips the speech cache, so a book never pushes out the exercise audio. The phone hands the queued sentences to the platform voice, which plays them in order.

## Screens

The reader holds two states, the same pattern as a list and its detail:

- **Library** - a search field, the sort chips (Last opened, A to Z) with a button that switches between covers and a list, then the books. Open book comes first, a plain + in the primary colours: a cover-sized tile, or a full-width bar in the list. Covers come two to a row, 1.7 times as wide as tall, and nothing sits under them. A cover takes the book's tint, crossed by the darker band the desktop covers carry, and shows the format and where the book is at the top, a menu button in the top right corner, the title at the bottom, and a line along its bottom edge for how far the book was read. The list shows one row per book: a small cover in the same tint, the title, and a second line with where the book is and how far it was read, in percent. Where the book is takes one icon: a phone for a book only on this phone, a cloud with a check for one on the phone and in the cloud, a cloud with an arrow for one only in the cloud, which also shows its size. A book whose file is gone is not shown, since the phone cannot point it at a moved file. Tapping a book opens it, or downloads a cloud book. The menu button, or a long press, opens a sheet with Download, Upload to the cloud, Open in another app, Remove in red and Delete from the cloud in red, whichever apply; both red actions ask first. A download shows its percentage over the cover, and the menu button turns into Cancel while an upload runs. The app remembers the layout.
- **Reading** - the page view or the paragraph view, selected by format. A top overlay holds back, read aloud, contents, appearance and search, with the title beneath. A bottom overlay holds previous page, page number and next page, with the page scrubber beneath. Tapping a PDF's page number turns it into a numeric field; entering a page and confirming jumps there. Contents and the dictionary search open as bottom sheets.

The reader is a rail destination, a shell screen at the same level as the Dictionary screen. A document opens under the currently active language pair, and its cards go to that pair's pool.

## Measurements

An environment variable, set to a PDF's path, opens the app directly on the reader, opens that document, and runs a scripted zoom, un-zoom, and scroll sequence by posting real wheel events into the window. It logs per-step duration, frame timing, and total interface-thread unresponsiveness. A rendering change is evaluated against the last recorded set of numbers, not in isolation.

## Reading comfort

- **Full screen.** A tap that lands on no word hides the controls and the Android system bars; another brings them back. The tap may land on the page, beside it, or between paragraphs. The phone's title, navigation and lookup controls overlay the reading area, so showing or hiding them keeps the page size and reading position unchanged.
- **Screen stays on** while a readable book is open.
- **EPUB, FB2 and TXT** have a contents sheet built from the book's own table of contents, or from its headings when it has none, and a scrubber by percent. Page colour (Theme, Paper, Sepia or Night), font, text size, line spacing and margins are reading settings; the font list holds the app's own font and the book fonts the phone has, such as Noto Serif and Roboto. Only page width is desktop-only.
- **Reading settings** are listed twice: in the sheet behind the gear button, limited to the open book's format, and in full on the Reader page of Settings. Both are the same rows over the same stored values.
- **PDF** preserves the document's original margins by default and fits the paper to the screen width. Turning off Page margins in the phone's reading settings crops the horizontal whitespace and fits the text block instead. At open the worker samples eight pages to find where the text sits; a page whose content is wider, such as a cover, fits its own content when cropping is enabled. Pages render in paper, sepia or night colours.
- **Volume keys** turn pages while a book is open, when that setting is on. It is off by default, since it takes the keys away from music volume.
- **A book that cannot be read** opens on a "Cannot read this book" screen with Back and Remove, rather than doing nothing.

## Desktop

The desktop reader keeps the two states, the library and the open book, and replaces every touch gesture and bottom sheet.

| Phone | Desktop |
| --- | --- |
| Open book tile, first in the grid | An Open book tile, always first in the grid, a plain + with the label in a tooltip. A file dropped on the library opens too. |
| Two columns of covers with search and sort | A grid of covers, three columns, each 1.7 times as wide as tall. A cover is the book's tint from the theme, the size of a deck cover and crossed by the same darker band. Its top left shows the format and where the book is: a monitor for a book only on this computer, a cloud with a check for one also in the cloud, a cloud with an arrow for one only in the cloud, which shows faded. The title sits at the bottom. Nothing sits under the cover. Chips sort it by last accessed or alphabetically, and a search field keeps the books whose title holds the typed text, ignoring case and accents. |
| A menu button or a long press opens a sheet of the book's actions | The whole cover is the button: it opens the book, or downloads a cloud book. A row of buttons appears on the cover's top right under the pointer or keyboard focus, where the phone has its menu button: show in folder, upload to the cloud for a book only on this computer, remove in red, delete from the cloud in red. Remove and Delete from the cloud both ask first. A download shows its percentage, the megabytes so far and a Cancel button over the cover. An upload shows its progress along the cover's bottom edge, with Cancel in the top right corner. Otherwise that edge shows how far the book was read. A book whose file is gone stays in the library on the desktop, unlike the phone, since the desktop can point it at the moved file: its cover says so, and clicking it asks where the file went and keeps its place. Enter opens the focused book and Delete removes it. |
| Tap a word, long-press and drag a phrase | Click a word, drag across a phrase. Ctrl+C copies the selection, from a PDF too. A click outside the text, in the margins or between paragraphs, clears the selection. |
| Dictionary sheet | A Translate button next to the selection, which opens a translation popup under it. See [Popups](#popups). |
| Contents sheet | A pane over the left of the page. Its right edge drags to resize it, and the width is remembered. |
| Reading settings sheet | The text-size button in the top bar, the same icon the phone's reader uses, opens an Appearance menu with the settings for the open book's format. A PDF has page colour and cropped margins: Paper, Sepia or Night. Other formats have their own page colour, which starts at Theme, the app's own colours, plus the font, text size, line spacing, page width and margins. The font list holds the app's own font and the book fonts installed on the computer, such as Georgia, Cambria and Palatino. The minus and plus buttons at the right of the bottom bar, and Ctrl+minus and Ctrl+plus, zoom a PDF and change the text size of other formats. The Reader page of Settings holds the same rows. The page width there is counted in characters, 110 by default, so a larger text size widens the column rather than fitting fewer words on a line. |
| Swipe to turn a PDF page, pinch to zoom | Arrow and page keys turn the page. The wheel scrolls through the pages without a break. Ctrl with the wheel zooms around the pointer. In other formats a wheel notch glides a fixed step, notches in quick succession add up, and a touchpad scrolls as it moves. A middle click drops an anchor and the text scrolls at a speed set by how far the pointer is from it; another click or any key stops it, and the click that stops it picks no word. Picking a word or phrase hands the keys back to the page, so Ctrl+C and the page keys keep working. Holding the middle button and dragging stops on release. |
| Tap an empty spot for full screen | Hide controls in the top bar, or F11. Escape brings them back. |
| Bottom overlay with page navigation and scrubber | One bar under the page: previous, position, next, scrubber, and zoom for a PDF |

A PDF opens fitted to its page width, capped at a comfortable width, because a wide window would otherwise blow the page up. When margins are cropped, it fits the text block instead. The reader may zoom out below that fit. Fit returns to it.

The contents pane floats over the page. Opening it never moves the page or changes its zoom, so the text under the reader's eye stays where it was. On a narrow window it covers the edge of the text.

A desktop PDF uses the same three-page stack as the phone. The three share one open book: one render thread with its copy of the file, one copy for reading text, one measurement of the margins. A newer render request cancels only the same slot's older one, so scrolling one page never drops another slot's render. A page that fits within three viewports is rendered whole and sharp while it is still out of view.

A zoom step can re-render three pages on the render thread. Word lookup uses the separate text document.

## Future features

- **Bookmarks.**

## Core entities

| Entity | Role |
| --- | --- |
| `MobileReaderScreen` | The phone reader's shell screen; hosts the library and reading states. |
| `DesktopReaderScreen` | The desktop reader's screen; hosts the library, the page, both panes and the position bar. |
| `DesktopReaderPdfView`, `DesktopReaderTextView` | The desktop page view for a PDF, and the paragraph view for the other formats. |
| `ReaderPdfPage` | Renders a PDF page on a worker thread; the item described in [PDF Implementation](#pdf-implementation). |
| `FTPdfRenderWorker` | The worker-thread object that owns the render-side `QPdfDocument` and produces bitmaps. |
| `FTPdfDocumentExtractor` | Reads a PDF's title metadata when it is added to the library or opened. |
| `AppReaderSettings` | The stored reading settings, shared by the reader and the Settings screen. |
| `MobileSettingsReader` | The reading settings rows, used by the reader's sheet and by Settings. |
| `FTVocabularyMarks` | Finds the learner's words, forms and phrases in a paragraph; see [Vocabulary highlighting](#vocabulary-highlighting). |
| `ReaderVocabulary` | Builds the marks from the cards and the dictionary's forms, and rebuilds them when either changes. |
| `ReaderTextHighlighter` | Paints the vocabulary marks and the sentence being read into one paragraph's text, on the phone and the desktop. |
| `ReaderReadAloud` | Read aloud: the place in the book and the one kept for it, the sentences queued ahead, pause, resume, stop, previous and next sentence, and the speed. |
| `FTNarration` | Finds the sentence under a tapped character and the next sentence worth reading. |
| `ReadAloudSession`, `ReadAloudService` | The phone's media session, notification, audio focus and foreground service. |
| `FTSpeechService` narration | Renders a sentence outside the speech cache and appends it to the voice that is playing. |
| `FTReaderDocument` | The output of a flowable-format extractor: a title and an ordered `Paragraphs` list. |
| `FTReaderRepository` | Reads and writes the `ReaderDocuments` table. |
| `ReaderDocuments` | The library table: `Id`, `FilePath`, `Title`, `Format`, `AddedDate`, `LastOpenedDate`, `Deleted`. |
| `DictionaryController` | Runs the dictionary lookup; shared with the Dictionary screen. |
| `MobileCardDetailsScreen` | The add-word screen every entry point in the app opens into. |
| `UiProbe` | Reads the `IKONIKA_READER_BENCHMARK` environment variable described in [Measurements](#measurements). |
| `addWordRequested(word, translation)` | The signal a double-click, a phrase drag, and the typed field all emit, described in [Popups](#popups). |
| `DesktopReaderTranslation` | The desktop Translate button and its popup, described in [Popups](#popups). |
