# Lufi animation: Blender export and portable Qt demo

This is the handoff guide for agents maintaining the Lufi animation workflow. The animator uses Blender and the committed Windows demo. Qt, Visual Studio, a separate Python installation, and dependency downloads are not required on the animator's PC.

The demo is the test bed for authoring and checking individual clips. Today embeds a package for its camera and uses the learner's Daily Goal to drive Lufi's animation.

## Animator workflow

1. Pull the repository, including the binary ZIPs. Open Blender 5.3 or later; 5.3 is the tested version.
2. In Blender Preferences, Extensions, use **Install from Disk** to install `animation/tools/ikonika-lufi-export.zip`. Enable **Ikonika Lufi Export**. This is a one-time installation; reinstall when the extension UI changes.
3. Open or create the animation under `animation/sources`. Keep the Lufi character parts compatible with the existing character. Save the file before the first export.
4. Open the 3D View sidebar, **Ikonika**, **Ikonika Animation**. Choose the active camera, source frame range, FPS, and Clip ID. Use `Camera_Desktop` and the intended desktop output dimensions for existing Lufi clips. Set render resolution percentage to 100.
5. Click **Export to Ikonika Demo**. Wait for the verified-export success report. Click **Open Demo** in the same Blender panel, open `animation/demo/preview.cmd`, or press **Reload** in an already running demo. Select the new clip and review it.

The Animation folder is found from a saved source under `animation/sources`. If the source lives elsewhere, choose the folder containing `demo/ikonika-demo-windows.zip` and `shared`. A blank Clip ID is derived from the Blender filename, with a leading `Lufi_` removed. Set an explicit ID to avoid accidentally replacing a different clip. Exporting an existing ID replaces that clip; another ID adds a clip.

The extension saves a temporary copy of the current Blender scene, including unsaved edits, under `animation/local/<clip>`. It exports that copy in a background process using `bpy.app.binary_path`. The working scene is not flattened or reloaded. Save the editable source separately before committing it. Keep Blender open until the export finishes.

The installed extension uses the current checkout's `scripts/anim-export/pipeline.py` when present. A bundled copy of the engine is included for an animation-folder-only handoff. No runtime download or Git clone is involved in either case.

## Files and source control

| Location | Contents | Track? |
| --- | --- | --- |
| `animation/sources/` | Editable `.blend` files, with the combined timeline in `Lufi_Animation_Main.blend` | Yes |
| `animation/sources/Lufi_Paw_Lick_Trial.blend` | Preserved earlier trial snapshot | Yes |
| `animation/shared/character.json` | Shared vector geometry, static properties, and deformation bases | Yes |
| `animation/shared/<clip>.json` | Clip tracks, timing, depth, visibility, or a held-pose reference | Yes |
| `animation/lufi-shared.zip` | Published, compressed character and clip package | Yes |
| `animation/lufi-mobile.zip` | The same clips projected through `Camera_Mobile` for mobile Today | Yes |
| `app/qml/Lufi.qml` | Today clip player and Daily Goal behavior shared by desktop and mobile | Yes |
| `animation/demo/` | Demo source, build/preview launchers, license notices | Yes |
| `animation/demo/ikonika-demo-windows.zip` | Ready-built Windows executable and deployed runtime dependencies | Yes |
| `animation/tools/ikonika-lufi-export.zip` | Installable Blender extension, engine, and vendored exporter source/license | Yes |
| `scripts/anim-export/` | Maintained export engine, extension source, developer tooling, and checks | Yes |
| `animation/local/` | Intermediates, snapshots, candidate packages, logs, extracted runtime, screenshots, builds | No |

`animation/.gitignore` ignores `local/` and `.blend1` backups. Do not put maintained entry points into the ignored working directory. The old `local/animation`, `design/lufi-lottie-trial`, and temporary `preview-verifier.cmd` are obsolete.

Commit editable sources and both final shared JSON changes and their ZIP. The demo executable does not change when adding animations. Update its runtime ZIP only after changing demo C++/QML or its runtime dependencies. Update the extension ZIP when changing its packaged code. Keep unrelated app changes out of these commits.

## Run and reload the demo

Double-click `animation/demo/preview.cmd`, or run it from any working directory. It does **not** build anything. The legacy `scripts/anim-export/preview.cmd` forwards to it.

The launcher uses the Windows-provided PowerShell executable, .NET hashing, and ZIP extraction. It unpacks the runtime ZIP into `animation/local/demo-runtime/<archive-hash-prefix>` on first use. Later launches reuse the extracted files. A changed runtime ZIP uses a new cache directory, avoiding replacement of a running executable. Old runtime directories can be removed after their processes are closed.

The executable locates `lufi-shared.zip` by walking upward from its own directory. To review an isolated candidate instead, pass an animation folder:

```bat
animation\demo\preview.cmd --assets "D:\Work\Candidate Animation"
```

That folder must contain `lufi-shared.zip`. Captures are written below its `local/captures` directory. No compile-time checkout path is stored in the executable.

The demo discovers clip IDs, display names, frame counts, FPS, and canvas dimensions from the package. Eye choices come from character layer names, including night variants. **Reload**, or `R`, rereads the package and resets the clip selection so added or replaced clips are available in the same process. The ZIP can be replaced while the demo is running.

A changed `.blend` alone does not change playback: export it, wait for success, then Reload. A changed animation package requires no executable rebuild. Changes to the demo UI or renderer still require a developer build.

## Review a new export through the demo

### Agents: use Ghost

Launch the preview on a Ghost hidden desktop. Do not use foreground Windows automation.

1. Discover Ghost tools and create or reuse desktop `lufi`.
2. Launch `cmd /d /c D:\Projects\FrenchTrainer\animation\demo\preview.cmd` with `ghost_desktop_launch`. The repository path in this example is not a required installation path; use the actual checkout.
3. Enumerate windows and select the newly launched `Lufi — Animation Demo` instance by PID and handle. The launch PID can belong to the command shell.
4. Use `ghost_desktop_press` and the relevant input tools to select clips and frames. Inspect `ghost_desktop_capture` output. After exporting while the demo runs, press `R` and confirm that the new clip appears without a process restart.

The launcher extracts the demo on its first run. The demo decodes the compact animation data at startup and Reload. Persistent failures appear in the console or the demo's asset-error label.

### Visual acceptance

Start with **Eyes Authored** and compare the same frame against the selected Blender camera. Demo frames start at 1 even if the Blender source starts elsewhere: demo frame `n` corresponds to source frame `source_start + n - 1`.

Play the whole clip and its loop boundary. Then pause and scrub the transitions. Check connected paw/foreleg motion, head/ear seams, tongue visibility, stroke widths and caps, tail overlap, and stray geometry. Toggle light background to expose seams. Repeat face-crossing motion with runtime eye variants and Closed/Blink; foreground geometry must occlude eyes correctly.

For paw-lick regressions, inspect frames 13, 16, 20, 23, 27, 28, 29, 31, 32, 36, 48, and 75. Those are regression shortcuts, not requirements for other clip lengths. Numerical checks complement this visual review; they do not replace it.

| Control | Action |
| --- | --- |
| Play / Space | Pause or resume |
| Clip selector | Choose a discovered animation or held pose |
| Eye selector / N | Choose or cycle authored/runtime eye layers, including night variants |
| Closed / B | Override with closed eyes |
| Blink | Brief closed-eye override, 160 ms |
| Light background / L | Toggle review background |
| Reload / R | Reload character and clip data |
| C | Cycle available clips |
| Slider | Stop and select a frame |
| F1 / F4 | First / last frame |
| F2, F3, F5, F6 | Paw-lick review frames 24, 48, 60, 23 |
| F7, F8, F9, F10, F11, F12 | Paw-lick review frames 27, 28, 29, 31, 36, 32 |
| A, E, T, U | Paw-lick review frames 13, 16, 20, 75 |
| S | Save artwork under `animation/local/captures` |

The renderer clamps review frames to the selected clip's range. Screenshot names include clip ID, frame, and eyes. Background variants can overwrite the same capture; rename evidence when needed. Ghost screenshots include the controls and frame label; `S` captures the artwork only.

Report the package reviewed, clip, frames/states inspected, numerical results, and any remaining limitations. A clean still frame does not establish animation fidelity.

## Export engine and Blender contract

Lufi is Grease Pencil, not a mesh animation or a raster movie. Export samples evaluated drawings, transforms, visibility, depth, variable-width foreleg strokes, and caps. The source camera, frame range, and effective FPS (`fps / fps_base`) come from Blender. Timelines are normalized to start at demo frame 1.

Visibility uses the evaluated layer opacity so sleep glyphs appear only on their authored frames.

`lufi-profile.json` declares the character-specific camera default, required object roles, protected head/ear parts, tail, foreleg, and verification landmarks. Names and compatible topology are part of the shared-character contract. The articulated foreleg currently requires one stroke per layer. Export rejects missing required objects, invalid camera/range, unsupported resolution percentage, or incompatible shared-character data instead of publishing a partial result.

The extension exports asynchronously. Its temporary scene copy is flattened only in the child Blender process. One export at a time is allowed from that Blender session. Stage logs and failed candidates remain under `animation/local/<clip>` for diagnosis. Multiple Blender sessions must not publish into the same animation folder concurrently.

| Stage | Script | Result |
| --- | --- | --- |
| Sample scene | `export_trial.py`, `export_foreleg.py` | `baked.json`, foreleg samples, depth, visibility, caps |
| Prepare geometry | `optimize.py`, `foreleg_geometry.py` | `preserved.json` and `prepared.json` |
| Compress shared representation | `build_shared.py` | Candidate character, clip, and ZIP |
| Check source fidelity | `verify_export.py` | Projected landmarks, protected geometry, foreleg/caps, and depth checks |
| Assemble with shipped runtime | Demo `--export-shared` | Candidate standard Lottie JSON |
| Check reconstruction | `verify_shared.py` | Every-frame vertices/tangents, depth, authored-eye visibility |
| Publish | `pipeline.py` | Replace published JSONs and atomically replace ZIP after successful verification |

The normal extension export always verifies using the committed demo. Existing clip JSONs are included in the candidate package. Failed export/verification leaves the published package unchanged. The final ZIP is the authoritative runtime artifact; JSONs are also kept for inspection and source control.

The pinned upstream exporter is vendored under `scripts/anim-export/vendor/io_lottie_export`. Its origin, commit, copyright, and GPL license are included. `animation/local/exporter` is an obsolete checkout from the old workflow and is not used.

## Shared assets and Qt playback

The shared JSON files use this project's schema, not directly playable Lottie JSON. The character stores reusable paths and base/deformation definitions. A clip stores property tracks, transform/deformation coefficients, visibility, depth, frame count, and FPS. Rigid motion reuses geometry. New deformation can add bases to the character while older clips retain their existing control vectors.

Compatibility checks reject changed camera dimensions, character parts/topology, or a previously rigid part becoming deformable. Adding a clip is not constant-size storage: clip tracks grow, and some clips also grow the shared character. Preserve existing assets when deliberately redesigning the character; that requires a coordinated rebake rather than disabling checks.

`rest.json` is a held-pose descriptor referencing a clip and a 1-based hold frame. It is not a separately baked animation. The builder creates a default rest descriptor only when none exists; exporting another clip does not silently replace the existing rest pose.

The ZIP uses sorted entries, fixed timestamps, and DEFLATE level 9. Every `*.json` in the shared output is included. To retire a clip, remove its JSON deliberately, update any held-pose references, and rebuild the package. Do not leave obsolete files in `shared`.

The demo reads the ZIP on startup and Reload, then decodes the shared paths, basis vectors, and clip tracks. Each frame applies the selected clip's transforms and deformation weights to the visible parts. Changing clips or eyes selects data that is already loaded. Held poses select a fixed frame from their referenced clip.

The demo and Today share the renderer. It draws body parts, outlines, and eyes in frame depth order, drawing each visible layer once. Today maps elapsed time to poses at the clip's authored frame rate and rasterizes at the displayed size. Today prepares the embedded bundle on a worker and reuses the renderer across clip switches. The export verifier uses the standard Lottie composition assembled by `FTSharedAnimation`.

Desktop builds embed both camera bundles so the desktop and mobile layouts each use their own framing, including when launched with `-mobile`. Mobile builds embed the mobile camera bundle.

Mobile Today places one player over whichever study card is expanded. The card's bottom edge clips Lufi's lower body. The viewport extends above the card so the head can rise past its top. Tapping Lufi queues an idle break. The rest of the card starts the study session. Playback pauses when Today is hidden, the app leaves the foreground, or Lufi scrolls out of view.

On Android and Windows, Lufi rasterizes each frame as one job in Qt's GUI thread pool. The resulting image supplies the whole viewport, including transparent pixels.

Lufi stays asleep for two seconds when Today first becomes visible each learning day, then wakes. Today records the wake when playback starts. Completing the Daily Goal takes priority over other reactions and keeps Lufi asleep for the rest of that learning day, including after a restart. Today saves the wake and sleep state and completed session count in workspace settings.

Awake idle uses Normal eyes until a Review or vocabulary Learn session is completed, Excited after one, and Happy after two or more. Blinks run independently of breathing. A completed session queues Interested for the next visit to Today. An awake idle tap picks Interested or Paw Lick, and Paw Lick can also play during a longer idle. A queued reaction waits for the neutral end of the idle cycle. The player can shorten the remaining breath to 250 ms and lets any active blink finish.

Interested and Paw Lick use the same progress eyes as awake idle and keep their authored closed-eye frames. Wake and sleep clips keep their authored expressions. Between 20:00 and 06:00 local time, the renderer substitutes the matching Night eyes, using the original layer for expressions such as Closed and One-eye peek.

The compatibility Lottie composition expands the compact assets into per-frame geometry. Inspect `build-shared.log` for the compact package's entry sizes.

### Rendering decisions to preserve

- Keep a single renderer so eye selection does not break paw/face occlusion or introduce seams.
- Preserve evaluated source geometry and per-frame depth. Do not approximate paw/head motion independently.
- Build variable-width foreleg silhouettes from per-point widths and source caps. Averaged strokes and separate cap ellipses caused extra outlines and misplaced pieces.
- Keep protected head/ear geometry intact through preparation.
- Preserve held geometry keys and the Qt-specific per-frame numeric start/end convention. Split animated X/Y position channels; animated ellipse positions use group transforms to avoid Qt spatial-path timing behavior.
- Copy JSON values before replacing their source container. Use an explicit `QJsonArray` or `const auto Value = ...toArray()`; direct-list `auto` initialization previously nested arrays under Android Clang.

## Developer build and packaging

Animators do not run these commands. Developers need Blender, Qt Quick and CorePrivate, CMake, and Visual Studio 2022 x64. The demo links Qt's private ZIP reader API, so ship the runtime matching the Qt build.

From any directory, run `animation/demo/build.cmd`. It builds and packages the Windows runtime and Blender extension. Outputs are `animation/demo/ikonika-demo-windows.zip` and `animation/tools/ikonika-lufi-export.zip`. Build/staging directories remain under `animation/local`.

Tool discovery uses Blender on `PATH`, the Windows `.blend` association, or the standard Program Files Blender installation. `LUFI_BLENDER_EXE` overrides discovery. The animator extension uses the running Blender directly and bypasses discovery entirely.

Developer overrides are `QT_ROOT` (or `QTDIR`), `LUFI_WINDEPLOYQT`, and `LUFI_CMAKE`. Otherwise Qt is located from `windeployqt` on `PATH`; CMake is found on `PATH` or in the Qt installation's Tools directory. Visual Studio is discovered by CMake and `vswhere`, not a personal installation path. No separate Python executable is invoked.

Packaging deploys Qt/QML/plugins, MSVC runtime DLLs, offscreen verification support, and license notices. It records Qt version and demo source hashes in `build-info.json`. Refresh license notices and Qt inventories when changing Qt versions. Commit the source and regenerated distribution together. The runtime archive is a one-time toolkit payload shared by all animations; it does not grow when adding clip data.

Developer commands from repository root:

```bat
scripts\anim-export\bake.cmd --verify
scripts\anim-export\bake.cmd --source "animation\sources\Another Clip.blend" --clip another-clip --verify
scripts\anim-export\bake.cmd --skip-export --clip paw-lick --verify
scripts\anim-export\bake.cmd --package-extension
animation\demo\build.cmd
```

Full export reads the selected source's camera and timeline unless `--camera`, `--start`, or `--end` overrides them. `--skip-export` requires that clip's saved intermediate settings and is only for changes after scene sampling. Do not use it after editing Blender. The default source is the baseline paw-lick; another file gets its own clip identity and working directory.

For a mobile bundle export, use a separate `--assets` directory and `--camera Camera_Mobile` with the same source scene and timeline marker ranges. Start with `paw-lick` and pass `--rigid-layout animation/shared/character.json` so parts that deform in later clips stay deformable. Export the other five clips there, then copy the resulting `lufi-shared.zip` to `animation/lufi-mobile.zip`.

For command-line inspection or assembly without a UI:

```bat
animation\demo\preview.cmd --inspect
animation\demo\preview.cmd --clip paw-lick --export-shared "animation\local\assembled.json"
```

The executable also accepts `--eyes "Eyes Normal"` and `--assets <folder>`. `--inspect` emits discovered metadata as JSON. `--export-shared` writes a standard Lottie composition and exits. Automated checks run with `QT_QPA_PLATFORM=offscreen` and use the portable runtime, not an installed Qt directory.

## Verification and troubleshooting

The shared verifier checks all frames of the selected export, comparing vertices and tangents against `preserved.json` with a 0.04-pixel tolerance. It verifies depth ordering, visible part counts, and authored eyes. The Blender verifier additionally compares protected head/ear geometry, selected other landmarks, and foreleg samples/caps. Neither is a raster comparison; Ghost visual review remains required.

`verify_portable.py` is an integration check run with Blender's `--background --python` options. It invokes the packaged extension operator in a disposable checkout with spaces in its path, exports a 20-frame clip starting at source frame 10 with fractional FPS, removes developer tools from `PATH`, disables dependency networking, and checks the preserved scene, old/new clip availability, and unchanged executable hash. Its output stays under `animation/local/portable-check`. The sample clip is test data, not a shipped animation.

| Symptom | Check |
| --- | --- |
| New animation absent | Export success, chosen animation folder/Clip ID, then Reload |
| Failed export | `animation/local/<clip>/export.log` and `logs/<stage>.log`; published ZIP should be unchanged |
| Wrong crop or geometry | Active camera, desktop dimensions, resolution percentage, profile/topology, exact source frame |
| Wrong duration | Source range and `fps / fps_base`; inspect clip metadata |
| Demo missing | Pull the committed runtime ZIP; do not ask the animator to build |
| Runtime extraction/launch failure | Writable `animation/local`, complete ZIP, console error; extracted cache can be recreated |
| Developer tool not found | Discovery/override variables above; no path edits in source |
| Persistent blank demo | Asset-error label, package validity, runtime/module deployment |
| Seams, detached paw, stray caps, incorrect overlaps | Compare exact Blender frame and both intermediate/reference compositions; preserve the rendering decisions above |

Before reporting completion, inspect the scoped diff, commit maintained code and final artifacts, and report actual verification. Keep captured images, intermediate JSONs, temporary scene copies, extracted runtime, and build output ignored.
