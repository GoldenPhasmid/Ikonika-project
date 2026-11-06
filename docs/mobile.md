# Mobile

`core` has no UI dependency, so it is untouched by anything below. Mobile is the same app layer as
desktop, with an Android-specific slice swapped in behind `NOT ANDROID` (CMake) and
`#if defined(Q_OS_ANDROID)` (C++) guards. Where the two platforms genuinely diverge — sign-in,
speech, background scheduling — the app layer picks the platform's own mechanism rather than
emulating the desktop one.

## Building

`scripts/tools/build-android.ps1` configures and builds against the Qt Android kit and the NDK,
alongside the desktop preset.

| Flag | Effect |
| --- | --- |
| `-Abi <abi>` | `x86_64` (default) matches the Windows emulator; `arm64-v8a` targets a real ARM device. Each non-default ABI gets its own build tree (`build/android-<abi>-debug`). |
| `-Apk` | Packages the APK after the native build. |
| `-Aab` | Packages an Android App Bundle instead — the format Play Store upload requires. |
| `-Install` | Implies `-Apk`, then runs `adb install -r`. Fails with "more than one device/emulator" if several devices or emulators are connected — install manually with `adb -s <serial> install -r <path-to-apk>` in that case. |
| `-Release` | Release build, signed with the release keystore instead of debug. |

**Use the debug build for active development, not release.** They're signed with different keys,
so switching between an installed debug and release build requires uninstalling first — which
wipes local app data. Debug is also the only variant `run-as` and `adb pull` can read app-private
files from on an unrooted device, which matters when inspecting the database or settings live.

`-Release` with `-Apk` or `-Aab` reads the keystore path, alias, and passwords from
`app/android/release-keystore-credentials.local.txt` (gitignored, not checked in) and signs through
Qt's `QT_ANDROID_SIGN_APK`/`QT_ANDROID_SIGN_AAB`. The Android `versionCode` a Play upload needs
bumped every time lives at `IKONIKA_ANDROID_VERSION_CODE` in the top-level `CMakeLists.txt`;
`versionName` follows the project version automatically.

## What's different on Android, and why

**Speech** uses the platform's own TTS engine rather than the desktop model pipeline — no model
download, quality depends on what's installed on the device. See [docs/voice.md](voice.md) for why.

**Sign-in** goes through Play Services' account picker for a Drive-scope token, not desktop's
browser-redirect flow — Google deprecated redirect-based OAuth for Android clients. Each signing
key (debug, release) needs its own registration in Google Cloud Console; sign-in fails outright for
an unregistered one.

**The dictionary and voice packages download after install** rather than shipping in the package —
both exceed Play Store's size cap. See [docs/releases.md](releases.md) for the catalog, download,
and update-offer mechanics, which are shared with desktop.

**Reminders** are an OS alarm plus a notification, not an in-process timer — this is what lets a
reminder fire when the app isn't running, including after a device reboot.

**Excluded entirely:**

- The reader. It depends on persistent file paths; Android hands out a revocable content handle
  instead, which doesn't survive a reinstall.
- The YouTube screen. It depends on `WebEngineQuick`, which has no Android build.

Drills are not excluded — they already work as ordinary cards, no mobile-specific effort needed.

## Features that need an Activity

A handful of things — notifications, alarms, the account picker, status bar theming — can't be
reached from C++ or QML at all; only an Android `Activity` or system service can do them. The
pattern is the same every time: a static Kotlin object owns the platform call, and C++ reaches it
over JNI, passing the current `Activity` as context. The Kotlin side never holds a reference back
into C++ — it's called, it does one platform thing, and it returns.

`IkonikaActivity` (a thin subclass of Qt's own `QtActivity`) is the only Activity in the app. It
exists solely to forward `onActivityResult`, which sign-in's account-picker flow needs; everything
else goes through the static Kotlin objects without any Activity subclassing of its own.

Two more entry points exist outside any Activity, for callbacks that must fire even when the app
isn't running: a boot receiver that re-arms reminders after a device restart, and an alarm receiver
that posts the reminder notification itself. Both are declared in the manifest and started by the
OS directly.

Speech and sign-in above reach the platform this same way; so do a few smaller helpers not covered
elsewhere — session checkpoints persisted to disk, and status bar / navigation bar theming that
follows the app's own light/dark setting rather than the system one.

## Status

| Area | State |
| --- | --- |
| Onboarding, dictionary/voice download, update offer | Done |
| Sign-in and sync | Done |
| Reminders | Done; reboot survival not yet verified on a device |
| Build tooling | Done |
| Play Store release | Not started — needs a developer account, a privacy policy, a data-safety form, and Google's review of the Drive OAuth scopes |
| Sideload | A signed `arm64-v8a` release ships as a GitHub release; see [docs/releases.md](releases.md) |

Next: verify a scheduled reminder survives a real reboot; confirm sync merges correctly from a
second Android device; start the Play Store submission process.
