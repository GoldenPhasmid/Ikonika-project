# Road to iOS

What stands between the current code and an iPhone release, built and shipped from a Windows
machine through GitHub Actions. Read [mobile.md](mobile.md) first: iOS follows the same rule as
Android, where the app layer uses the platform's own mechanism wherever the platforms diverge.

## Decisions

**The app is open source under the GPLv3.** Development stays in the private repository. Each
release's source is copied into the public `GoldenPhasmid/Ikonika-project` repository and
committed there by hand.

**Every iOS build runs on the public repository's GitHub Actions.** Public repositories get the
standard macOS runners for free, so the GitHub bill is zero.

**Day-to-day iOS work happens in a live session on the runner,** not through repeated CI runs. See
[Development loop](#development-loop).

## Blockers

| # | Blocker | Status |
| --- | --- | --- |
| 1 | Qt Network Authorization is GPLv3 or commercial, not LGPL | Resolved by the GPLv3 release. The iOS build still leaves the module out. |
| 2 | Qt for iOS links statically, which the LGPLv3 allows only if the user can relink | Resolved by the public source |
| 3 | No Mac: no local build, no simulator, no debugger | Worked around by the live session and the CI simulator |
| 4 | A test iPhone | Owned. It needs iOS 17 or newer, which means an iPhone XS, XR or later. Not yet checked. |

Blocker 4 is the only one still open.

### Licences

The Qt documentation lists Qt Network Authorization under "Commercial or GPLv3". Every other Qt
module the app links is LGPLv3. The app links `Qt6::NetworkAuth` on every platform, so the app as a
whole has to be released under the GPLv3. Sherpa-onnx (Apache-2.0), miniaudio, SQLite and miniz
(public domain or MIT) are all compatible with it.

Qt for iOS ships only as static libraries. The LGPLv3 allows static linking when the user can
relink the app against their own build of Qt. The published source satisfies that.

One risk remains. Some people read the App Store's usage rules as an added restriction, which the
GPL forbids. That reading took VLC off the App Store in 2011. It matters only for GPL code
distributed through the store by someone other than its copyright holder. The iOS build leaves Qt
Network Authorization out, so only LGPL Qt ships. Many apps already ship LGPL Qt through the App
Store.

Leaving the module out costs nothing on iOS. The desktop flow does not work there anyway, because
Google refuses loopback redirects for iOS clients. See [Sign-in](#sign-in).

### No Mac

Everything Xcode does on a Mac happens on a GitHub-hosted macOS runner: configuring, compiling,
signing, uploading, and running the simulator.

- No debugger. A crash on a device is diagnosed from the app's own log file and from TestFlight's
  crash reports. App Store Connect symbolicates those reports when the upload carries its debug
  symbols.
- The verification rule in AGENTS.md assumes a local emulator. On iOS, the CI simulator takes its
  place for screenshots, and the iPhone through TestFlight takes its place for real use. Neither can
  be driven through mobile-mcp.
- Qt's simulator libraries are x86_64 only. The simulator job therefore runs on GitHub's Intel
  Mac, `macos-15-intel`, where they run natively. On an Apple Silicon Mac the simulator would need
  Rosetta and, with Xcode 26, the universal iOS platform.

A macOS virtual machine on the PC is not an option. Apple's licence allows macOS only on Apple
hardware, and such a machine breaks with every Xcode update.

## Cost

| Item | Cost |
| --- | --- |
| Apple Developer Program | $99 per year, renewed yearly |
| GitHub Actions | Free, as long as every macOS job runs in the public repository |

An individual developer account shows your legal name as the seller on the App Store.

EU storefronts require a trader declaration under the Digital Services Act. A trader's address,
phone number and email are shown publicly on the product page. A free app with no monetisation can
declare itself a non-trader.

## Repositories

| Repository | Visibility | Holds |
| --- | --- | --- |
| `GoldenPhasmid/Ikonika` | Private | Full history, all development |
| `GoldenPhasmid/Ikonika-project` | Public | One snapshot commit per release, the iOS workflow and the signing secrets |
| `GoldenPhasmid/ikonika-releases` | Public | The catalog, the book catalogs, the privacy page and the release packages, unchanged |

The source stays out of `ikonika-releases` on purpose. Every installed copy of the app reads
`catalog.json` from it, and a 1,000-file snapshot commit would bury the catalog history.

**A snapshot is made with `git archive` from a release commit.** That exports tracked files only.
The dictionary pipeline and its data stay private: `scripts/dictionary`, `scripts/opus` and
`third_party/eflomal` are left out. The app builds without them, since the dictionaries reach it as
downloads.
Gitignored secrets never leave the PC: the Android keystore, its credentials file and
`google-oauth.json`. The private history is never published either. That history still holds the
Android release keystore, committed in a8984418 and deleted in adc15c08.

**Every shipped build must match a published snapshot.** The GPLv3 requires the exact source of
each released binary, build scripts included. iOS releases are built from the snapshot in the
public repository, which makes them match by construction. Windows and Android releases need their
snapshot published at the same time as the build.

**Every commit to `Ikonika-project` is made by hand.** `publish-snapshot.py` only copies files
into the clone's working tree and prints the commands to commit them.

**Scan each snapshot before the first push.** Run a secret scanner such as gitleaks over the
exported tree.

**The signing secrets live in the public repository's Actions secrets.** GitHub never passes
secrets to workflows started by pull requests from forks. The iOS workflow runs only on a release
tag or a manual start, never on `pull_request`.

## Development loop

A CI run that installs Qt and builds from scratch is far too slow to fix compile errors one at a
time. A live session on the runner replaces that.

1. Copy the work in progress into an `ios-dev` branch of `Ikonika-project` and commit it there.
2. Start the workflow by hand with the session option. A step such as `mxschmitt/action-tmate`
   pauses the job and opens an SSH shell on the Mac. With `limit-access-to-actor: true`, only your
   GitHub SSH key can connect.
3. The session lasts up to 6 hours, the job limit for GitHub-hosted runners. Qt is installed once,
   and `xcodebuild` rebuilds incrementally.
4. Each fix is pushed to `ios-dev` and pulled inside the session.

Claude Code can drive the session over SSH, so an iOS compile error costs seconds.

Most of the port does not need the Mac at all. The desktop, Android and iOS split compiles and runs
its tests on Windows. Only the iOS-only code needs the session: the Objective-C++ files, the iOS
branch of the CMake files, and `Info.plist`.

## The code today

The app layer treats "not Android" as "desktop". 17 source files carry `Q_OS_ANDROID` guards, and
the CMake files carry 12 `ANDROID` checks. In nearly every one, the `#else` branch is the desktop
path. An iOS build compiles all of those desktop paths: sherpa-onnx speech, the single-instance
lock, the tray-icon reminder, the YouTube screen, ONNX Runtime and the installer-based updater.

The first change, then, is a three-way split: desktop, Android, iOS. A single `IKONIKA_DESKTOP`
definition in CMake replaces the `!defined(Q_OS_ANDROID)` checks that mean "desktop". Everything
else becomes `Q_OS_ANDROID` or `Q_OS_IOS`. CMake sets `IOS` when it targets iOS. `AppMetrics`
returns a mobile layout on Android only; iOS joins it.

`core` needs two changes:

- `FTSettings` reads `defaults.ini` from beside the executable. On iOS it reads the resource copy,
  as on Android.
- `core` links its own SQLite amalgamation just to call `sqlite3_db_config` and
  `sqlite3_total_changes64` on Qt's connection handle. On Windows and Android, Qt's SQLite lives
  inside a shared plugin, so the two copies never meet. On iOS both are static, so the link fails on
  duplicate symbols. The fix is to skip the vendored library on iOS. The calls then reach Qt's own
  copy, which is the copy that owns the handle.

## Platform services

Every Kotlin object in `app/android/src` needs an iOS counterpart or a decision to drop it. iOS code
is Objective-C++ in `.mm` files, called directly from C++. There is no JNI-style bridge and no
Activity. Only the home widget needs Swift.

| Feature | Android | iOS | Release |
| --- | --- | --- | --- |
| Speech | `PlatformSpeech`, the platform TTS | `AVSpeechSynthesizer`. French, Spanish and English voices ship with iOS | First |
| Read aloud | `PlatformSpeech` and `ReadAloudService` | A second `AVSpeechSynthesizer` that queues the reader's sentences. Narration pauses when the app leaves the screen | First |
| Sound effects | `PlatformSoundEffects` | The desktop path, miniaudio playing the bundled WAVs | First |
| Audio session | Not needed | Set an `AVAudioSession` category once at launch. It decides whether speech plays with the silent switch on | First |
| Status bar theme | `StatusBarController` | Qt's colour scheme override, or `overrideUserInterfaceStyle` | First |
| Splash | `SplashGate` | A launch storyboard, set through `QT_IOS_LAUNCH_SCREEN` | First |
| Reduced motion | `animator_duration_scale` | `UIAccessibilityIsReduceMotionEnabled` | First |
| Session checkpoints | `SessionCheckpoints`, shared preferences | One JSON file beside the workspaces, holding every save under its kind and key | First |
| Reminders | `PlatformNotification`, an alarm, `BootReceiver` | `UNUserNotificationCenter` with a calendar trigger. It survives a reboot by itself, so iOS needs no boot receiver | First |
| Sign-in | `GoogleAuthorization`, Play services | `ASWebAuthenticationSession` and an iOS OAuth client, see [Sign-in](#sign-in) | First |
| Credential store | None, Play services holds the grant | The Keychain, as a third `AppCredentialStore` beside the Windows and in-memory ones | First |
| Export a database | `PlatformShare` | `UIActivityViewController`, the share sheet | First |
| Reader file picker | `ReaderFilePicker` | `UIDocumentPickerViewController`, which hands over a copy. The copy moves into the workspace's `books/` folder | First |
| Open in another app | `ReaderFilePicker.showDocument` | The share sheet with the book file | First |
| Keep screen on | `ReaderWindow` | `UIApplication.idleTimerDisabled` | First |
| Open with | `ReaderIntake`, `ReaderIntakeActivity` | Document types in `Info.plist`, PDF, EPUB, FB2 and plain text. Qt hands the file URL to a `file` URL handler, which copies the book into `books/` | First |
| Machine translation | `PlatformTranslation`, ML Kit | Off, see [Machine translation](#machine-translation) | Later |
| Home widget | `HomeWidget` | A WidgetKit extension in Swift, sharing data through an App Group | Later |
| Volume keys turn pages | `ReaderKeys` | Dropped. iOS has no public API for the volume buttons | Never |
| YouTube screen | Excluded | Excluded. Qt WebEngine has no iOS build | Never |

### Sign-in

Google still accepts a custom URI scheme redirect for iOS clients, unlike Android. The iOS OAuth
client lives in the same Google Cloud project as the desktop and Android clients. Its redirect is
the reversed client id. It has no client secret, so the flow uses PKCE.

`ASWebAuthenticationSession` opens Google's page in a system sheet inside the app. It hands back
the redirect address without any URL scheme registration in `Info.plist`. The app then exchanges
the code for a refresh token and stores the token in the Keychain under the account id, just as the
desktop stores it in Credential Manager. The workspace model in [workspaces.md](workspaces.md)
carries over unchanged.

None of this uses Qt Network Authorization. The token exchange is one POST request through
`QNetworkAccessManager`, so the iOS build does not link the module.

### Sync in the background

iOS suspends an app a few seconds after it leaves the screen. Sync uploads when the app goes to the
background, so that upload has to run inside a background task
(`beginBackgroundTaskWithExpirationHandler`). That task gets about half a minute. The same applies
to a dictionary download interrupted by switching apps. The resume support described in
[releases.md](releases.md) already covers that case, since the next launch requests only the
missing bytes.

### Storage

`QStandardPaths` resolves to the app's sandbox, so `AppPaths` works without changes. Two additions
are needed:

- Mark `dictionaries/`, `models/` and `downloads/` as excluded from backup. App Review rejects apps
  that put re-downloadable data into the iCloud backup. The workspace databases stay in the backup,
  since a signed-out learner has no other copy.
- Write the log file under `Documents/` and enable file sharing in `Info.plist`. The Files app then
  shows the log, which is the only way to read it on a device without a Mac.

### Machine translation

The iOS release ships with `IKONIKA_ENABLE_MACHINE_TRANSLATION` off. The two candidates for later:

- **ML Kit.** This is the Android engine, with the same model downloads. Its iOS SDK is distributed
  only through CocoaPods, which does not fit a CMake-generated Xcode project well.
- **Apple's Translation framework.** It is free, on-device, needs no attribution, and downloads its
  models through iOS itself. It is Swift-only, and it is not covered by the benchmark in
  [machine-translation.md](machine-translation.md).

OPUS-MT through ONNX Runtime is ruled out for the same reason as on Android: 229 MB per language.

### Qt on iOS

Qt Quick, Quick Controls, SQL, Network and Widgets are all supported on iOS. The `SafeArea` attached
property the mobile QML already uses works on iOS too.

Check these on the first CI build:

- Qt PDF in the iOS kit. The reader's PDF pipeline depends on it. If it is missing, PDFs are off on
  iOS while EPUB, FB2 and TXT keep working.
- Static plugins. The QML modules import through `qt_add_qml_module` without changes. The SQLite
  driver and the image format plugins EPUB pictures need may need `qt_import_plugins`.
- TLS. Qt on iOS uses Apple's own TLS backend, so the OpenSSL libraries Android fetches are not
  needed.

## Pipeline

One workflow, `.github/workflows/ios.yml`, started by hand or by an `ios-*` tag. It lives in this
repository and reaches `Ikonika-project` through the snapshot. It refuses to run in a private
repository, so it never spends billed minutes.

| Start | What runs |
| --- | --- |
| By hand | An unsigned device build |
| By hand with `upload` | A signed archive, uploaded to TestFlight |
| By an `ios-*` tag, such as `ios-0.8.2` | The same as `upload` |
| By hand with `session` | Either of the above, then an SSH session, also after a failed build |

The workflow reads seven secrets of the public repository:

| Secret | Holds |
| --- | --- |
| `IOS_CERTIFICATE_P12` | The Apple Distribution certificate and its key, as base64 |
| `IOS_CERTIFICATE_PASSWORD` | The `.p12` password |
| `IOS_PROVISIONING_PROFILE` | The App Store provisioning profile, as base64 |
| `IOS_TEAM_ID` | The developer team id |
| `APPSTORE_API_KEY_P8` | The App Store Connect API key, as base64 |
| `APPSTORE_API_KEY_ID` | The key id |
| `APPSTORE_API_ISSUER_ID` | The issuer id |

One repository variable, `IOS_GOOGLE_CLIENT_ID`, holds the iOS OAuth client id. It goes into
`Info.plist` as `IkonikaGoogleClientId`. A build without it has sync switched off. The id is not a
secret, since every copy of the app carries it.

`scripts/tools/publish-snapshot.py` makes the snapshot; see [scripts.md](scripts.md).

The steps:

1. Check out the repository on a `macos` runner with an Xcode that meets Apple's current minimum
   SDK. Apple raises that minimum every spring; in 2026 it is the iOS 26 SDK.
2. Install Qt 6.11.1 for iOS and the macOS host Qt it needs for its tools, through `aqtinstall`.
   Cache both.
3. Configure with `<Qt>/ios/bin/qt-cmake -G Xcode`. Set the bundle id, the team id and the version
   through `XCODE_ATTRIBUTE_*` properties on the app target.
4. Import the signing certificate into a temporary keychain and install the provisioning profile,
   both from repository secrets.
5. `xcodebuild archive`, then `xcodebuild -exportArchive` with `destination` set to `upload`. That
   sends the build straight to TestFlight, authenticated with an App Store Connect API key.
6. On a manual run, `simulator-build` compiles a Release app for the simulator on an Apple Silicon
   Mac, without QML ahead-of-time compilation, and uploads it as an artifact.
7. `simulator` starts at the same time on an Intel Mac, where Qt's x86_64 simulator libraries run.
   It boots an iPhone simulator while the app compiles, then polls the run's artifacts until the
   app appears. `.github/actions/simulator-walk.py` installs it and runs these phases:
   - the first launch, until the log shows the first frame;
   - a dictionary install, then a database integrity check;
   - the screen walk and the navigation walk, failing on any warning;
   - a narration check that queues two sentences and waits for both to finish;
   - a shared book: an FB2 file handed to the app through `simctl openurl`, until the reader opens it.

   Screenshots, every phase's log, the system log and crash reports go into the `simulator` artifact.

Every job restores Qt from one cache entry, about 3 GB, through `.github/actions/install-qt`, and
compiles through ccache, set up by `.github/actions/setup-ccache`.

### Signing without a Mac

Every step runs on Windows and in the browser:

1. Generate a private key and a certificate signing request with `openssl`.
2. Upload the request in the Apple Developer portal and download an Apple Distribution certificate.
3. Combine the certificate and the key into a `.p12` with `openssl`.
4. Register the bundle id `com.goldenphasmid.ikonika`, the same as Android's package name, and
   create an App Store provisioning profile for it.
5. Create an App Store Connect API key with the App Manager role.
6. Store the `.p12`, its password, the profile and the API key as base64 secrets of the public
   repository.

The version comes from `project()` as on every other platform. The build number must rise with
every upload, like `IKONIKA_ANDROID_VERSION_CODE`, so it needs its own value in the top-level
`CMakeLists.txt`. Using the workflow run number for it also works.

## App Review

The rules most likely to cause a rejection, in order of risk:

- **Sign in with Apple, guideline 4.8.** An app whose primary account uses a third-party login must
  also offer Sign in with Apple. The exemption covers apps that sign in only to reach the user's own
  content on that third-party service. Sync to Google Drive fits it. Present the feature as Google
  Drive sync rather than as an account, and say so in the review notes.
- **Privacy manifest.** An app has to declare why it calls certain APIs: file timestamps, free disk
  space, user defaults. The download path checks free space, and Qt touches the rest. Check which
  manifest Qt 6.11 provides, then add the app's own `PrivacyInfo.xcprivacy`.
- **Backup of re-downloadable data**, covered under [Storage](#storage).
- **Self-update.** The installer-based updater must not compile for iOS. An "update available"
  message that opens the App Store page is allowed.
- **Export compliance.** The app uses only standard HTTPS, so `ITSAppUsesNonExemptEncryption` is
  `false` in `Info.plist`, which skips the question on every upload.

App Store Connect also needs a privacy policy URL, the privacy labels, an age rating, and
screenshots for the 6.9-inch iPhone. The simulator job produces those screenshots. Declaring the app
iPhone-only skips the iPad screenshots; iPads still run it in compatibility mode.

## Order of work

Each step ends in something visible on TestFlight or in the CI artifacts.

| Step | Work | Done when |
| --- | --- | --- |
| 0 | Check the iPhone's iOS version. Add a `LICENSE` file, then copy the first scanned snapshot into `Ikonika-project`. Developer account, bundle id, certificate, profile, API key, App Store Connect record, iOS OAuth client. | Secrets are in the public repository |
| 1 | The workflow with its live session option. Platform split in CMake and C++, the SQLite link fix, stubs for every service in [Platform services](#platform-services) | A build reaches TestFlight and opens on the first setup screen |
| 2 | Paths, `defaults.ini`, backup exclusion, the log in `Documents/`, speech, sound effects, the audio session, theme, checkpoints | A dictionary downloads, and Training and Review run with sound on a device |
| 3 | Reminders, sign-in, the Keychain store, sync in a background task, the share sheet | A reminder fires with the app closed, and sync merges with the Android copy |
| 4 | The reader file picker, keep-screen-on, the Qt PDF check | A picked EPUB and a picked PDF open and add words |
| 5 | Privacy manifest, privacy labels, screenshots, review notes | The build is submitted for review |
| 6 | Home widget, machine translation, Open with | Each ships in its own update |
