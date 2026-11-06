# Profiling

Profiling means a sampling profiler. Log timestamps, timing prints, `FTQueryTimer` output and
qmlprofiler are not profiling.

Record a baseline before changing code, then repeat the same scenario after the change. Compare
main-thread sample counts between the two runs. Percentages shift when the total changes.

## Mobile: simpleperf on the emulator

simpleperf ships with the NDK at `%LOCALAPPDATA%\Android\Sdk\ndk\30.0.15729638\simpleperf`.
Set `ANDROID_SERIAL=emulator-5554` first, because a real phone may be attached too.

1. Install the Debug APK and launch the app.
2. Record while you repeat the slow action, by hand or with `adb shell input tap`:

   ```
   python app_profiler.py -p com.goldenphasmid.ikonika --unrepresentative_profile_debug_app -r "-g -f 2000 --duration 25" -o perf.data
   ```

   The flag `--unrepresentative_profile_debug_app` is required for a debug build.
3. Give the report symbols from the local build:

   ```
   python binary_cache_builder.py -i perf.data -lib D:\Projects\FrenchTrainer\build\android-debug
   ```

4. Find the Qt main thread, `qtMainLoopThrea`, and read inclusive costs for it:

   ```
   python report.py -i perf.data --sort comm,tid
   python report.py -i perf.data --tids <main thread id> --children --sort symbol
   ```

A debug build on the emulator inflates `clock_gettime` and Qt startup, so judge app code by its
own symbols.

## Desktop: Visual Studio's command-line collector

`VSDiagnostics.exe` ships with Visual Studio 2022 and samples CPU without an elevated prompt. It
lives in `C:\Program Files\Microsoft Visual Studio\2022\Community\Team Tools\DiagnosticsHub\Collector`.
The app needs Qt's `bin` on `PATH`.

1. Launch the app under the collector, repeat the slow action, then stop it:

   ```
   VSDiagnostics.exe start 1 /launch:D:\Projects\FrenchTrainer\build\msvc-debug\app\Debug\IkonikaApp.exe /loadConfig:AgentConfigs\CpuUsageBase.json
   VSDiagnostics.exe stop 1 /output:ikonika.diagsession
   ```

   `/attach:<pid>` replaces `/launch` for a running app.
2. Open `ikonika.diagsession` in Visual Studio, which resolves symbols from the build's `.pdb`
   files and shows the hot path per thread.
