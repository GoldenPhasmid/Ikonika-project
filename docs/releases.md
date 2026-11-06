# Releases

Windows desktop distribution, self-update, and runtime asset download. Android is out of scope for
the first two: the store replaces the app there. The asset download is shared by both platforms.

## Package

| | |
| --- | --- |
| Generator | CPack `INNOSETUP`, compiled by Inno Setup 7 |
| Compiler | `%LOCALAPPDATA%\Programs\Inno Setup 7\ISCC.exe`, found via `IKONIKA_ISCC` |
| Output | `build\msvc-release\Ikonika-Setup-<version>.exe` |
| Size | 39.1 MB compressed, 148.6 MB installed |
| Install root | `{autopf}\Ikonika` → `%LOCALAPPDATA%\Programs\Ikonika` |
| Architecture | x64 only |
| Build | `.\tools\build-msvc.ps1 -Release -Package` |

Contents: app executable, Qt runtime and plugins, MSVC redistributable, `sherpa-onnx-c-api.dll`
(4.3 MB), `onnxruntime.dll` (16.6 MB), `defaults.ini`, `google-oauth.json`.

Both speech DLLs ship because `core` links them through an import library. Without them the process
fails at load, so no fallback path can run. Downloading them would require run-time loading instead.

A fresh install starts with no dictionary and no voice. Dictionary discovery returns empty and
speech reports itself unavailable. Neither is an error.

## Inno directives

Set in the packaging block of the top-level `CMakeLists.txt`.

| Directive | Value | Effect |
| --- | --- | --- |
| `AppId` | `{{89146247-DDFE-4522-BB79-05596AD96D7C}` | Identifies the existing install. Changing it installs a second copy alongside. |
| `PrivilegesRequired` | `lowest` | No elevation, and what resolves `{autopf}` to the per-user path |
| `CloseApplications` | `yes` | Restart Manager terminates the running app so locked files can be replaced |
| `RestartApplications` | `yes` | Insufficient alone, see below |

Every `[Files]` entry carries `ignoreversion`, so a reinstall of the same version replaces all
files. Installed files inherit the source build timestamp rather than the install time.

`RestartApplications` only restarts processes that called `RegisterApplicationRestart`. Qt does not.
Relaunch comes from two `[Run]` entries instead:

| Source | Flags | Applies to |
| --- | --- | --- |
| `CPACK_INNOSETUP_RUN_EXECUTABLES` | `postinstall skipifsilent` | Interactive: checkbox on the final page |
| `packaging/relaunch.iss` | `nowait`, `Check: WizardSilent` | Silent: unconditional |

CPack has no variable for arbitrary `[Run]` entries, hence the included fragment. Its contents do
not appear in the generated `ISScript.iss`, since the preprocessor resolves the include at compile
time.

An extra launch is harmless. A second process hands off to the first and exits.

## Uninstall

The Account screen has an Uninstall button when an Inno uninstaller sits next to the executable, so
never in the Store package or a debug build. It starts the uninstaller and quits.

`packaging/uninstall.iss` cleans up after an interactive uninstall. It always deletes
`%LOCALAPPDATA%\Ikonika`, which holds the dictionaries, voice models, downloads and logs, since all
of them can be downloaded again. It then asks whether to delete `%APPDATA%\Ikonika` as well, which
holds the word database and settings. The default answer is no, because a signed-out learner has no
other copy. A silent uninstall deletes nothing: the installer runs the previous copy's uninstaller
silently when the learner picks a different folder, and that must not take the data with it.

The Store removes a package's data itself.

## Store package

The Microsoft Store gets an MSIX built from the same install tree as the installer. The Store signs
it and delivers its updates, so it needs no certificate and the self-update stays out of the way.

| | |
| --- | --- |
| Build | `.\scripts	oolsuild-msvc.ps1 -Release -Msix` |
| Output | `build\msvc-release\Ikonika-<store version>.msix`, uploaded unsigned |
| Manifest | `packaging/msix/AppxManifest.xml.in`, carrying the identity Partner Center assigned |
| Logos | Resized from `app/icons/AppIcon.png` on every build |
| Local run | `python scripts	ools\package-msix.py --register`, which needs developer mode |

The Store refuses a version whose first part is 0 and reserves the fourth part, so the package
version is the project version with the first part raised by one: 0.4.3 ships as 1.4.3.0.

The app asks Windows whether it runs with a package identity. When it does, it never reports an
application update, so nothing downloads or runs the installer. Dictionary, voice and translation
packages still come from the catalog.

Launch at sign-in goes through a startup task declared in the manifest. The installer's way, a
registry value, is kept private to the package by Windows, so sign-in would never read it. A startup
task cannot carry arguments, so the app asks Windows how it was started and stays in the tray when
the answer is the startup task. A learner who switches the task off in Task Manager keeps it off:
Windows refuses to switch it back on from the app.

## Version

`project()` in the top-level `CMakeLists.txt` is the only place it is written. It reaches the app as
`IKONIKA_VERSION` and is compared against the catalog's `app.version`. Release tags are not parsed,
so their format does not matter.

## Catalog

One public file at
`https://raw.githubusercontent.com/GoldenPhasmid/ikonika-releases/main/catalog.json`, overridable
via `update/catalogUrl` in `defaults.ini`. The app compiles in that address and reads everything
else from the file, so publishing a language reaches installed copies without an app release.

No authentication anywhere. The repository is public, so anonymous HTTPS serves both the file and
the release assets.

Top level: `schema`, an optional `app` object, a `packages` array, an optional `webSearch`
array, and an optional `books` object.

| Package field | Purpose |
| --- | --- |
| `id` | Package identity |
| `kind` | `dictionary`, `voice` or `translation`. Decides which directory it unpacks into. |
| `target`, `natives` | Language codes, for filtering before download. Null target serves every language. |
| `version` | Compared against the recorded local version |
| `dictionarySchema` | Database layout version, currently 10. A mismatch blocks the download. |
| `file` | Asset name on the release |
| `unpackTo` | Path below the kind's directory. `.` unpacks flat. |
| `bytes`, `unpackedBytes` | Download size and disk needed after extraction |
| `sha256` | Of the archive, lowercase hex |
| `desktopOnly` | Hides the package from mobile onboarding and language pickers; set on voice packages |

An unrecognised `schema` refuses the whole file. A single entry is dropped when it lacks an id, url
or version, names an unknown kind, or carries a checksum that is not 64 lowercase hex characters.

`webSearch` lists the sites the Search the web button can open, so a site that moves its address is
fixed by editing the catalog. Every installed copy picks the change up on its next catalog read.

| Web search field | Purpose |
| --- | --- |
| `id` | Stored in `dictionary/webSearchSource` when the learner picks the site |
| `name` | Shown in Settings |
| `url` | Address template. `{target}`, `{native}` and `{word}` are replaced; the word is percent-encoded. |
| `languages` | Language code to the way the address spells it, `"fr": "french"`. A pair is offered only when both codes are listed. |

A source is dropped when it lacks an id, a name or languages, or its address is not https or has no
`{word}`. The app keeps a built-in copy of both sites for the time before the catalog arrives, or
when it never does. Adding the array kept schema 1, since older builds ignore keys they do not read.

## Book catalogs

`books` maps a language code to the address of that language's book catalog, `"fr":
"https://.../books-fr.json"`. An address that is not https is dropped. Each language has its own
file for two reasons. The book list changes far more often than the main catalog, and a broken edit
to it must not stop app updates or dictionary downloads. The app also reads it only for the language
being learned.

The app reads the book catalog for the language being learned when the workspace opens, after every
main catalog read, when the language changes, and when the Free books filter is picked.

Top level: `schema` (1), `target`, the language code, and a `books` array. A file for another
schema or with no `target` is refused whole, and one whose `target` is not the language being
learned is ignored.

| Book field | Purpose |
| --- | --- |
| `id` | Lowercase letters, digits and dashes. Names the downloaded file. |
| `title`, `author` | Shown on the shelf |
| `level` | Required reading difficulty: `beginner`, `intermediate` or `advanced` |
| `file` | Asset name. Its extension decides the format: EPUB, FB2, TXT or PDF. |
| `url` | Where the file downloads from |
| `bytes` | Download size |
| `sha256` | Of the file, lowercase hex |

A book is dropped when its id breaks that rule, it has no title or url, its file is not a format the
reader opens, its reading level is missing or unknown, or its checksum is not 64 lowercase hex
characters. A second book under the same id is dropped too. A book is published as the bare file,
not a zip, since an EPUB is a zip already.

Each language's books are assets of one release, tagged `books-<lang>` and never replaced, so adding a
book uploads one file and adds one entry. The catalog itself, `books-<lang>.json`, sits next to
`catalog.json` at the root of the releases repository. French is published with twelve books: three by
the Comtesse de Ségur for beginners; Daudet, Leblanc and Leroux for intermediate readers; and
Maupassant and Mérimée for advanced readers, plus the shared classics below. English is published with twenty: Potter, Williams, Wilde, Baum and Sewell for
beginners; Carroll, Burnett, Kipling, Stevenson and Doyle for intermediate readers; and Jane Eyre,
The Fall of the House of Usher, The Phantom of the Opera, Twelve Years a Slave, The Adventures of
Tom Sawyer, Wuthering Heights, White Fang, Little Women and The Secret of Chimneys.
Spanish is published with eleven: two beginner readers by Harrison and Wilkins/Luria, El gato con
botas in Teodoro Baró's 1883 translation, Ratón Pérez, La Edad de Oro, fables by Iriarte and Samaniego,
Cuentos y diálogos and Cuentos de la Alhambra. The shelf starts with the beginner readers and moves
toward the intermediate classics. These older editions retain their historical spelling.

The shared classics collection starts with six works. Each translation has its own reading level;
matching a title does not imply matching language difficulty or identical editorial content.

| Work | English | French | Spanish |
| --- | --- | --- | --- |
| The Adventures of Tom Sawyer | Published, intermediate | Published, intermediate; Hughes's 1884 adaptation, explicitly labelled | Edition still to verify |
| Alice's Adventures in Wonderland | Published, intermediate | Published, advanced; Henri Bué translation | Edition still to verify |
| Treasure Island | Published, intermediate | Published, intermediate; Paschal Grousset translation | Published, intermediate; Manuel Caballero translation |
| The Secret Garden | Published, intermediate | Edition still to verify | Edition still to verify |
| The Jungle Book | Published, intermediate | Published, advanced; Fabulet and d'Humières translation | Published, advanced; Ramón D. Perés translation, including both Jungle Books |
| The Adventures of Sherlock Holmes | Published, intermediate | Complete matching collection still to verify | Complete matching collection still to verify |

The first shared-classics addition contains seven EPUBs. Six use Project Gutenberg editions
identified as public domain in the USA. The French Tom Sawyer uses Wikisource's verified 1884
Hughes adaptation, with its transcription attribution, source/history link and CC BY-SA 4.0
notice retained in the EPUB. Its 33 chapters are preserved, together with the dedication,
preface and conclusion; the conversion omits illustrations. This is an adaptation of the novel,
not an interchangeable unabridged translation.

The remaining cells are sourcing gaps, not claims that no public-domain translation exists.
The unsourced Spanish Tom Sawyer on Wikisource was rejected because the page itself flags its
unknown translator and possible copyright. The French Sherlock Holmes collection checked has
only six of the original twelve stories; the Spanish 1909 collection is split across volumes.
Neither is published as the complete shared work without checking its full contents. French
Secret Garden has a 1921 Vallette translation, but its downloadable edition and redistribution
terms still need verification. Spanish Alice and Secret Garden also need a verified edition.
Do not substitute a freely downloadable modern translation for these gaps.
Eligibility is checked under United States copyright rules, including the translation's rights.
The release repository's `books-<lang>.sources.tsv` files record the new editions and their sources.
El gato con botas retains its Wikisource transcription attribution and license in the EPUB.
The build is
not byte-for-byte repeatable, so a rebuilt book gets a new checksum;
when adding books, keep the published entries of the books already in the release instead of
uploading them again.

`scripts/tools/build-book-catalog.py` builds a catalog and its assets from a TSV with `id`, `title`,
`author`, `level` and `path` columns. It strips every Project Gutenberg mention from a book, since a free
book that opens on a page of English license text reads badly and the license entry clutters the
contents. Stripping every mention is also what the Gutenberg license asks of a copy that drops its
license text. From an EPUB it removes the Gutenberg header and footer, any paragraph, table or
heading that names Gutenberg, the generated cover that carries the Gutenberg name, the license's
contents entry, and the Gutenberg identifier, source and generator in the metadata. A real scanned
cover stays. In an EPUB made from Gutenberg's plain text, where italics are written as underscores,
it turns underscore pairs back into italics and drops the stray ones. A book whose contents list fewer
than three entries gets them rebuilt from its chapter titles: paragraphs that read as a Roman numeral,
a dash and a title become headings. From a TXT it keeps only the text between the start and end lines. A book that still
mentions Gutenberg afterwards stops the build. `update/catalogUrl` has a counterpart for testing, `books/catalogUrl` in
`defaults.ini` or the user settings: an address with `{target}` replaced by the language code, which
takes the place of the main catalog's entry. A `file:///` address works there, so a catalog can be
tried from disk before it is published.

## First launch

The setup screens open instead of Today when no dictionary is installed, or when the learner has
not set up the current language yet. They offer the account, then a language, then the placement
test, then wait for the download.

Whether a language is set up is stored in the learner's own database, not on the machine. A
placement result counts, and so do cards in that language. Finishing setup writes an empty placement
result when the test was not offered. A new account, a signed-out database and Delete everything
each bring the screens back. Changing account or erasing data re-checks this at once.

A headless launch skips the check while a dictionary is installed. The `-onboarding` argument
brings the screens back on a developer build.

The language list is every language the catalog publishes a dictionary for, plus whatever is
already on disk, so a fresh install can reach a language it has not downloaded. The dictionary is
queued first and the voice behind it, since nothing needs a voice to start learning.

A machine with no network gets the failure, a retry, and a retry every fifteen seconds on its own.
A package archive already on disk can be installed instead of downloaded; it is matched by its
checksum rather than its name. A database from another machine can be merged in from the same
screen, through the merge sync uses.

## Update sequence

1. App reads the catalog at launch, after the window is up.
2. Compares `IKONIKA_VERSION` against `app.version`.
3. If newer, shows an offer on Today and in Settings. Downloading starts only after the learner presses Download, then the installer is verified.
4. Applying is a separate step, because it closes the app. Once the installer is verified, the
   desktop Settings category list offers Restart and update, and the Settings button carries a dot until it is done.
   The quit is queued, so the button that asked for it finishes its click first.
5. The installer runs with `app.silentArguments`. Restart Manager closes the app, Inno replaces the
   files, the silent `[Run]` entry starts it again.

An installer left by a previous run is adopted when it still matches the published build and its
digest. Anything else in that directory is cleared.

The catalog is read at launch, and again whenever the window comes back to the foreground at least
four hours after the last read, so a copy left in the tray for days still learns about an update.
Settings, About has Check for updates for an immediate read, and Try again when a download failed.
The Today offer survives navigation and restarts until installed or dismissed with its close button.
Dismissal is saved for that published version; Settings keeps the offer and a newer version appears on Today again.

## Package install

Download, verify, unpack, then record the version. Recording last means an interrupted install
starts over rather than believing itself finished.

Requests queue rather than run together, since two transfers share the one connection. A package
that fails before its transfer starts does not strand the ones behind it, and a voice that lands
while the app is running is loaded straight away.

Transfers write to a `.part` file and rename only once the length and the digest both match. A
network failure keeps that file, and the next attempt asks for the bytes it is missing with a
`Range` header, so a dropped connection costs only what had not arrived yet. The digest is computed
at the end over the finished file, since a running hash cannot carry across a restart of the app.
A server that ignores the range header answers with the whole body instead of the part asked for;
that reply is refused and the file thrown away, because appending it would corrupt what is held.
Cancelling discards the file outright. Stall timeouts are 30s for transfers and 15s for the
catalog.

Free space is checked against `bytes` plus `unpackedBytes`, since both are on disk at once.

Voices are found by scanning the model directories rather than from a list in the source, so a newly
published voice needs no app change. A directory that is not a voice model is skipped quietly.

## Dictionary update

A dictionary already installed reaches a newer published one through the same download path. The
catalog's `version` is what decides it; a republished archive under the same number changes
nothing, whatever its checksum.

On mobile, a launch that finds the catalog ahead of the installed dictionary for the language being
learned raises a sheet offering the update and its size. Passing on it records that version and the
sheet stays away until a later one is published. Settings keeps a row under Foreign language for
anyone who passed. The offer covers dictionaries only; voice and translation packages have no
equivalent yet.

On desktop, newer versions of installed dictionaries, voices and translation models appear on Today
and in Settings. Downloads require a click, including replacements for dictionaries whose schema the
app can no longer read. Today offers stay until installed or explicitly closed. Closing an offer records
that package version, survives navigation and restarts, and leaves the update available in Settings.
A later published version gets a fresh Today offer. Settings carries a dot while any update waits.

Unpacking rewrites the file an open connection is reading, so the dictionary is released first and
the same language pair opened again once the new file is in place. Decks, cards and scheduling live
in the app's own database and are untouched.

## Storage layout

Everything downloaded lives under the per-user data directory, never inside the installation. The
installer only removes what it wrote, so anything downloaded beside the program would be orphaned.

| Directory | Holds |
| --- | --- |
| `dictionaries/` | Dictionary databases. Searched ahead of the ones beside the executable. |
| `models/` | Voice and translation models. Searched ahead of the ones beside the executable. |
| `downloads/` | Archives in flight. Deleted after unpacking. |
| `updates/` | The pending installer |
| `workspaces/<account>/books/` | Downloaded cloud books, and free books as `free-<id>.<format>` |

A debug build puts all of it under `local/` in the checkout.

## Publishing

One public repository, `GoldenPhasmid/ikonika-releases`. It holds `catalog.json` in git and every
payload as a release attachment. The code repository stays private and the app never reads it.

Tags name the payload rather than an app version. The trailing number and the catalog `version`
field move together, so republishing under the same number reaches nobody. See `catalog.json` for
the current package list.

Everything is zipped. Directories have to be, since an asset is one file. Dictionaries halve. ONNX
models compress at roughly 1.2:1.

Built zips, published or not, sit in `D:\Projects\ikonika-releases\archives`, alongside the catalog
repository. That directory is not source controlled.

Supertonic is the default desktop voice: one package with no language target, listed first among the voice packages, so first-launch setup downloads it for every language. Piper is one model per language, so a second language's Piper voice needs its own catalog entry and its own download; it stays in the catalog as an alternative. Android speaks through the platform's own TTS and downloads no voice. See [docs/voice.md](docs/voice.md) for the performance case.

## Status

Done:

- Installer, per-user, no admin prompt
- Silent update verified on a real install at a non-default location
- Catalog reading, version comparison, package download, digest verification, unpacking
- Voice discovery by scan
- Setup screens on a first launch, and a language list in Settings that reaches undownloaded ones
- One download queue, worked through in order
- Install from a local archive, and merge a database from another machine
- Resume after a failed transfer, verified on the emulator against a real release asset
- An update offer for an installed dictionary, on mobile and desktop
- An application update offer on desktop, verified against a local catalog and a stand-in installer
- Store package built, registered and launched locally with the self-update off

Outstanding:

- Not every built package is published; see `catalog.json`
- No code signing, so SmartScreen warns on every download of the installer
- The Store package has not been submitted, and its start at sign-in has not been watched across a
  real sign-in
- Nothing offers an update for a voice or translation package
- The update sheet has no metered-connection wording and no way to defer until Wi-Fi
