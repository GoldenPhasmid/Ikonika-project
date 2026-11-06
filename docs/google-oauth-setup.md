# Google OAuth setup

The app signs in with Google's OAuth 2.0 installed-app flow. That flow needs a client id and
secret, which are per-developer secrets and are never committed. You create them in Google Cloud
Console and drop them into a file the app reads at runtime.

## Steps

1. Open [Google Cloud Console](https://console.cloud.google.com), create a project.
2. Enable the **Google Drive API** for the project (APIs & Services > Library).
3. Under **OAuth consent screen**, pick *External* and fill in the app name and a support email.
4. Under **APIs & Services > Credentials > Create credentials > OAuth client ID**:
   - Application type: **Desktop app**.
   - Note the client id and client secret.
5. Give the app the client id and secret. Copy the JSON the console offers for download
   (`client_secret_*.json`) into the project's `data/` folder, or save the credentials there as
   `google-oauth.json`. The download wraps the pair in an `installed` object; the hand-written
   file keeps them at the top level:

   ```json
   {
       "client_id": "xxxx.apps.googleusercontent.com",
       "client_secret": "xxxx"
   }
   ```

   Both shapes are accepted. `data/` is git-ignored, so the download is never committed.

Until a client file exists the app stays signed out and refuses sign-in.

## Where the app looks

In order: next to the database in `%APPDATA%\Ikonika\Ikonika\`, then beside the executable, then
the project's `data/` folder in a debug build only. Packaging copies the file from `data/` to sit
beside the executable under the documented name, which is how sign-in works on a machine that
never had the repository. The AppData copy overrides it, so a different client can be used without
repackaging.

None of this applies to Android. Google has deprecated both the loopback and custom-URI-scheme
redirects for Android OAuth clients, so the desktop browser-redirect flow described here does not
work on a phone at all, with any client. Android sign-in instead goes through Play Services'
`AuthorizationClient`, wired up in `app/android/src/com/goldenphasmid/ikonika/GoogleAuthorization.kt`
and called from `AppGoogleAccount` over JNI - it needs no client id, no secret, and no file here.
What it does need is the Android-type OAuth client (package name + SHA-1 signing fingerprint)
registered separately in the same Google Cloud project, since that is what Play Services checks
the request against.

## Caveats

- A *Web application* client will not work; the desktop flow needs an installed-app client.
- On desktop, the refresh token survives restarts, so sign-in happens once. On Android, the
  access token only lives in memory for the running process - nothing restores it across a
  relaunch yet, so sign-in currently happens once per app session.
- Reuse the same project for every machine; each user gets their own Drive app data folder.
- The pair ships inside the package. Google's installed-app flow does not treat a desktop client
  secret as confidential; the loopback redirect is what secures it. Keep it out of the repository
  all the same, since the repository is shared more widely than the build is.
- Building a package without a client file under `data/` warns and produces an archive whose
  sign-in refuses. This warning is desktop-only; Android needs no client file at all.
