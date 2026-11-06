# Accounts and workspaces

How the app keeps each user's data apart, and how signing in, signing out and switching accounts
move between them, and how a signed-in workspace syncs.

## The problem

The app kept one database for the signed-out user and one per Google account, and decided which to
open from a network reply. Every launch opened the signed-out database first and switched only once
Google confirmed the account, so an offline launch showed the wrong data. A failed token request
erased the saved sign-in, so one launch without network signed the user out. Settings, books and
session checkpoints were shared between all users on the device.

## The model

### Workspace

A workspace is one folder that holds everything belonging to one user:

- the settings file: every preference, including theme, interface language and launch at startup
- the app database: cards, the progress journal, daily stats. This is the file sync uploads.
- the device database: the reader library and the YouTube library, which hold paths and positions
  that mean nothing on another device, so sync never sends them
- books the user added, and their reading positions
- session checkpoints, so an interrupted Training or Review resumes in the workspace it started in
- the reminder and the home widget's numbers, which always describe the active workspace

A workspace is closed. Nothing reads or writes another workspace's folder, and no data moves
between workspaces, including on sign-in.

There is exactly one signed-out workspace, and one workspace per Google account ever signed in on
the device.

### Device level

A few things belong to the device, not to a user, and live outside every workspace:

- downloaded dictionaries, voices and translation models: large, impersonal, shared by every user
- logs and the app's own update state
- the account registry

### Account registry

The registry is a small file at the device level. It is the only record of who is signed in, and
it is read at launch without touching the network.

| Field | Meaning |
| --- | --- |
| Active workspace | The workspace the app opens at launch. |
| Account id | Google's permanent id for the account. Names the workspace folder. Never changes. |
| Email, name | Shown on the Account screen. Refreshed whenever Google returns them. |
| Connection | Connected, or needs reconnect when Google refused the credential. |

A workspace is named by the account id, never by the email. A user can rename their Gmail address,
and their workspace must not change with it.

### Credentials

The desktop refresh token lives in the operating system's credential store, Windows Credential
Manager, under one entry per account id. The registry holds no secret. A token kept in a plain file
ends up in backups and in every copy of the data folder; the credential store keeps it encrypted to
the Windows user.

Debug and release builds use different entry names, so a debug build never signs in with the
release build's token. Tests and the headless sandbox use an in-memory store and never touch the
real one.

Android stores no credential. Play services holds the grant and hands out tokens on request.

A credential never lives inside a workspace, so a copied or exported workspace never carries a way
into the account.

Credential Manager belongs to the Windows user on that machine. Moving the data folder to another
PC or reinstalling Windows loses the credential, and the user signs in once more. The workspace is
unaffected.

### Layout

```
<data root>/
    accounts.ini              the registry
    dictionaries/ models/ downloads/ logs/
    workspaces/
        signed-out/
            settings.ini
            Ikonika.db
            Device.db
            books/
            checkpoints/
        google-<account id>/
            ...same contents
```

The data root is `local/` in a debug build, one folder under the local application data folder in
a release build, and the app's private files on Android. Release does not split data between the
roaming and local folders: roaming happens only on company domains with roaming profiles, never on
a learner's home PC, so the split buys nothing.

## Flows

### Launch

1. Read the registry and open the active workspace: its settings, then its database.
2. Build the screens.
3. Nothing in these steps needs the network. A token is requested only when something online asks
   for one, and its outcome never changes which workspace is open.

A first launch, or a registry that cannot be read, opens the signed-out workspace.

### Sign in

1. The user chooses Sign in.
2. Google's own flow runs: the browser on desktop, Play services on Android.
3. The app reads the account's permanent id, email and name from Google.
4. If the registry knows the id, its entry gets the new credential and the current email and name.
   If not, a new entry and an empty workspace folder are created.
5. The account's workspace becomes active. The registry is written before anything closes.
6. The app closes the current workspace and opens the account's.

Signing in never copies the signed-out workspace's data into the account's workspace.

If the user cancels or Google fails, nothing changes and the current workspace stays open.

### Sign out

1. The user chooses Sign out and confirms.
2. The credential is forgotten on this device only. Nothing is revoked at Google: Google's revoke
   removes the app's access for the whole account, which would disconnect every other device.
3. The signed-out workspace becomes active. The app closes the account's workspace and opens the
   signed-out one.

The account's workspace stays on the device untouched. Signing in again reopens it with everything
in it.

### Switch account

Switching is a sign-in to a different account from inside an account's workspace. The previous
account's credential stays in the credential store, so switching back needs no consent screen. Each
account keeps its own workspace.

### Closing one workspace and opening another

Nothing on disk is deleted or changed. The app closes the current workspace the way an editor
closes a document, then opens the next one:

1. The running session saves its checkpoint into the current workspace.
2. The current workspace closes: its database and settings file are closed, everything loaded from
   them is dropped from memory, and its reminder is cancelled.
3. The next workspace opens: its settings and database are read, its reminder is scheduled, and the
   screens are built from it on the Today screen.

### One owner for workspace data

A single workspace object holds every piece of runtime data that comes from a workspace: the
database connection, the settings, the loaded cards, the session state and the reminder.
Opening a workspace creates that object, and closing it destroys the object and everything it holds.

Nothing outside that object keeps workspace data. No other component opens workspace files or
caches what it read from them, so nothing can outlive a close and leak into the next workspace.

### Sync

Only a signed-in workspace syncs. Its app database goes up to Google Drive's hidden app folder as one
archive, and every other device signed in to the same account merges it into its own copy. The
signed-out workspace never syncs. Books the user uploads sit in the same folder as separate files;
see [book-reader.md](book-reader.md#cloud-books).

A sync starts when the workspace opens, a few seconds after the last change, when the app goes to the
background with changes not yet sent, when it comes back to the foreground after five minutes, and
when the network returns. A merge refreshes the screen in place and leaves a running session alone.

Changing workspace rebuilds every screen from scratch, the way a relaunch would. That is what keeps
anything read from the previous workspace out of the next one.

### Credential failures

Only an explicit sign-out or account switch changes the active workspace. A token request can fail
in two ways, and neither signs the user out:

- No network, or a timeout: the request is retried later. The connection state does not change.
- Google refuses the credential, because access was revoked, the password changed or the account
  was removed from the phone: the account is marked as needing reconnect. Drive refusing a single
  access token is not a refusal: the token is dropped and the next request asks for a new one. The workspace stays
  active and fully usable offline, and the Account screen offers to reconnect.

On Android, every silent token request names the registry's account, so Play services can never
answer for a different Google account on the phone.

## Moving existing data

Not decided yet. The path is chosen once workspaces are in place.

Cards of a signed-in account already sit in the old sync's cloud copy, which is the same database
file in Drive's app folder. Whatever replaces sync can recover them from there. A user who never
signed in has no cloud copy, so their data survives only if the update moves it.

## Decision

Closed workspaces, chosen over one shared database with an owner column on every row.

Pros:
- One user can never see or change another user's data, because no code path reaches across
  folders.
- The app knows which data to open from a local file, so launch works offline.
- Changing user is closing one workspace and opening another, with no per-table filtering to get
  wrong.
- Removing a user from the device is deleting one folder.
- Sync, when it returns, works on one workspace's files and needs no notion of other users.

Cons:
- Progress made while signed out does not follow the user into their account. This is deliberate:
  the signed-out workspace is a different user.
- Today's workspace data is spread over several app-wide objects, and all of it has to move into
  the one workspace object. Changing user then costs about as much as a relaunch.
- Removing a downloaded dictionary removes it for every user on the device.
