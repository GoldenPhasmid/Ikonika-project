# Google Play release checklist

Steps to ship a new Android release to Play Console. For the one-time setup this depends on, see
[docs/google-oauth-setup.md](google-oauth-setup.md) (release-key OAuth registration) and
[docs/mobile.md](mobile.md) (build tooling).

## Last release

| | |
| --- | --- |
| Git tag | `0.7.3` |
| Version name | `0.7.3` |
| Version code | `16` |
| Built | 2026-11-01 |

The next upload needs a version code above 16.

## Prepared release

Version `0.8.2`, version code `20`, was built on 2026-11-06. The signed arm64 bundle passed
`jarsigner` verification with the Ikonika upload certificate. It is ready for the Internal testing
upload in step 7; Play Console upload and rollout are pending. Version code 19 belonged to the
unuploaded 0.8.1 build and is skipped.

Release notes for Google Play (under 500 characters):

```text
New Features
The Reader can highlight the words from the learner's vocabulary in text books.
Sentence tiles exercise is submitted manually.
Training mastery stage opens with one audio batch and speaks only the missing word after a gap answer.
Practice sheet gets filter chips and correct card counts.
Dictionary and app update offers download only after an explicit tap.
```

## Prerequisites (one-time, not per release)

- `app/android/release.keystore` and `app/android/release-keystore-credentials.local.txt` exist
  locally (gitignored, never committed).
- Play Console developer account set up.
- The release keystore's SHA-1 fingerprint registered as its own Android OAuth client in Google
  Cloud Console — without it, Google sign-in/sync silently fails on release builds even though
  the rest of the app works. Get the fingerprint with:
  `keytool -list -v -keystore app/android/release.keystore`.

  Current upload key SHA-1 (registered as "Ikonika mobile" in Cloud Console, package
  `com.goldenphasmid.ikonika`): `7E:F1:0F:54:F1:23:DE:61:6C:30:F5:43:54:2F:5C:2D:6D:FF:CC:31`

- Anything installed through Play (including a tester opt-in link) is re-signed with **Play App
  Signing**'s own certificate, which is a different SHA-1 from the upload key above and needs
  registering separately. Get it from Play Console → your app → Setup → App integrity → "App
  signing key certificate," and add it as a second fingerprint on the same OAuth client. A
  sideloaded APK/AAB built locally uses the upload key directly and doesn't need this.

## Every release

1. **Bump the version.** Edit `project(Ikonika VERSION X.Y.Z ...)` at the top of the top-level
   `CMakeLists.txt`.

2. **Bump the version code.** Edit `IKONIKA_ANDROID_VERSION_CODE` in the same file. Always
   increment, never reuse a number — Play permanently reserves every version code ever uploaded,
   even from a release you later delete.

3. **Build the signed bundle:**

   ```
   .\scripts\tools\build-android.ps1 -Abi arm64-v8a -Release -Aab
   ```

   Output: `build\android-arm64-v8a-release\app\android-build\build\outputs\bundle\release\android-build-release.aab`

4. **Verify the signer** (catches a wrong or debug key before it reaches Play):

   ```
   jarsigner -verify -verbose -certs <path-to-aab>
   ```

   Look for `jar verified` and `CN=Ikonika, OU=Dev, O=GoldenPhasmid`.

5. **Generate Release Notes**

   ```
   Read commits from the last app tag to HEAD, with the lines each one changed in mobile code.
   Keep only the largest changes a phone user notices. Drop small fixes, tweaks, desktop-only
   work, docs, tests, refactors, build scripts, and reverted pairs.
   Name each change at the level of a screen or a flow, not its parts:
   "Redesigned Reader screen and book flow", not covers, sort and reading progress.
   Name the screen, session or exercise a change happens in. A reader has no other context:
   "Word forms training ends with filling the form into a real dictionary sentence", not
   "Verb forms are practised in real sentences".
   Write plain sentences, without code names or labels such as "Reader:".
   Describe the app, never address the reader: no "you" or "your". "Books sync between devices
   through Google Drive", not "Sync your books between devices".
   Do not fill the character limit for its own sake.
   Categories, omitting empty ones:
   - New Features: at most five lines
   - Dictionaries: version bumps in ikonika-releases catalog.json, one line for all languages
   - Fixes: only a crash or a broken flow
   Output a version under 500 characters for Google Play in chat.
   ```

6. **Commit and tag:**

   ```
   git add CMakeLists.txt
   git commit -m "Bump version to X.Y.Z"
   git tag -a X.Y.Z -m "Release X.Y.Z"
   ```

7. **Upload to Play Console** — your app → Testing → Internal testing → Create new release →
   drop the `.aab` → Release name (e.g. `X.Y.Z (versionCode)`) → Save → Review release → Roll
   out.

8. **Record the release.** Update the "Last release" table at the top of this document with the
   new Git tag, version name, version code and build date, then commit it:

   ```
   git add docs/google-play-release-checklist.md
   git commit -m "Record X.Y.Z as the last Play release"
   ```

## Gotchas

- **"Version code X has already been used"**, even after deleting the release/draft that used
  it — Play never frees a version code back up. Bump it and rebuild.
- **`INSTALL_FAILED_UPDATE_INCOMPATIBLE`** installing over an existing install — the device has a
  build signed with a different key (commonly: debug installed, release attempted). Android
  requires an uninstall to switch signing keys, which wipes that app's local data. If the data
  matters, export it first from in-app Settings → Data → Export database, and re-import after
  the release build is installed.
- Only `arm64-v8a` is built. That covers effectively every real device at this project's
  `minSdk 28` — it's a deliberate scope choice, not an oversight.
- The bundle is about 63 MB, and 16.5 MB of that is native debug symbols under
  `BUNDLE-METADATA/com.android.tools.build.debugsymbols`. Android Gradle Plugin adds them by its
  default, since `app/android/build.gradle` sets no `debugSymbolLevel`. Devices never download them.
  They stay, because Play Console needs them to show function names in native crash reports.
  Setting `ndk { debugSymbolLevel 'NONE' }` in the release build type would drop the bundle to about
  46 MB, at the cost of raw addresses in those reports.
- A `Warning: Android platform 'android-37' does not exist in SDK` during the build is harmless;
  the build falls back and still produces a correctly signed artifact.
- `versionName` follows `project()`'s version automatically — only the version code needs
  bumping by hand.
