# Android debug APK

The game is drawn for a phone held upright (216x468), so it should run on
one. This plan is for a **debug** APK: built from this repo with one
command into an `.apk` file that installs on the phone by opening it,
played with touch. A store release
(signing keys, icons, Play listing) is out of scope.

The desktop build, `make check` and the golden soak must not change. All
Android code sits behind `#if defined(PLATFORM_ANDROID)` or in its own
files, so the sim stays byte-identical.

## User's decisions (2026-10-30)

- Built by a plain `make apk` (NDK + aapt2 + apksigner), no Gradle.
- No typing on the phone for now: the player keeps the rolled name, and
  REFRESH gives another.
- Tab (reveal) and F5 (reload lines) are left out on the phone.
- The APK is a file installed by opening it on the phone, not over adb.
- The phone is a Samsung Galaxy S25 Ultra: arm64-v8a only, on Android 15
  or later. minSdk 26 still, targetSdk 35.
- raylib 5.5's source is fetched into `build/` by the make target on the
  first build, not kept in the repo.

## What is on this machine (checked 2026-10-30)

| Need | Have |
|---|---|
| Android SDK | `~/Android/Sdk`: platforms android-36 and 37, build-tools 36.0.0 and 36.1.0 (aapt2, d8, zipalign, apksigner), platform-tools (adb), licence accepted |
| Java (keytool for the debug key) | Host: OpenJDK 26 and Gradle 9.7 from linuxbrew. Not in the `claude-build` toolbox |
| Android Studio | Flatpak `com.google.AndroidStudio` |
| **NDK** (clang for arm64) | **Missing** |
| **raylib built for Android** | **Missing**: the toolbox's raylib is desktop only. raylib 5.5 has to be built from source with the NDK |
| SDK command-line tools (sdkmanager) | Missing; Android Studio's SDK Manager can do the same |

The NDK is self-contained (its own clang and sysroot), so it runs on the
host as well as in the toolbox. No dev headers are needed for it.

## How it will be built

No Gradle and no Java code: raylib uses Android's `NativeActivity`, so the
APK is a manifest, the game as `libthemall.so`, and the data files as
assets. Built by a `make apk` target:

1. Compile raylib 5.5's `src/` with the NDK clang for `PLATFORM_ANDROID`
   into `build/android/libraylib.a` (once; it's kept).
2. Compile `src/**/*.cpp` with the NDK clang (C++20, `-O2 -g`) and link
   `libthemall.so` against it, plus `android_native_app_glue`, `-landroid
   -llog -lEGL -lGLESv2 -lOpenSLES`.
3. `aapt2 link` the manifest (`android/AndroidManifest.xml`, `hasCode=false`,
   portrait, fullscreen) with `data/lines.txt` and `data/names.txt` as
   assets.
4. Zip in `lib/arm64-v8a/libthemall.so`, `zipalign`, `apksigner` with a
   debug key, giving `build/TheMall-debug.apk`.
5. Optional, for a phone plugged in with USB debugging on: `make
   install-apk` runs `adb install -r`, and `make logcat` shows the game's
   stderr and raylib's log. Not needed to play.

### Installing it

The APK is a normal installable file. Copy it to the phone (USB file
transfer, Quick Share, a cloud folder, e-mail), tap it, and let the app
that opened it "install unknown apps" when Android asks. Play Protect may
warn about an app it doesn't know: "Install anyway". No developer options
are needed.

A newer APK installs over the old one, keeping the obituary, only if it is
signed with the same key. So the debug key is not kept in `build/` (a
clean would lose it, and the next APK would need the old app uninstalled,
which deletes the obituary). It is the standard Android debug key,
`~/.android/debug.keystore`, made by keytool if it isn't there yet.

New files: `android/AndroidManifest.xml`, `android/build.sh` (or Makefile
rules), a small `src/core/files.{h,cpp}`.

## Code changes the phone needs

### Built (2026-10-30): build order 1 and steps 1–3

- `make apk` (on the host) delegates to `android/apk.mk`. raylib's own
  Android Makefile doesn't work with NDK r30 (it passes an unversioned
  `-target aarch64`), so `apk.mk` compiles raylib's seven C files itself.
  The first build clones raylib 5.5 into `build/android/raylib`.
- The package is `com.teratoy.themall`, version 0.1-debug, debuggable,
  with no icon yet (Android shows its default one).
- `src/core/files.{h,cpp}`: `files::asset()` reads `data/lines.txt` and
  `data/names.txt` from the APK; `files::savePath()` puts the obituary in
  the app's storage. Both do nothing new on a computer. `files::TOUCH_ONLY`
  is true in the Android build.
- `ui::backPressed()` is Escape or the back button, used wherever Escape was.
- On the phone, tapping the name does nothing and the hint is hidden.
- Full screen comes from the theme (no status bar, drawn beside the
  cut-out). The navigation bar isn't hidden: immersive mode needs a call on
  Android's UI thread, which a NativeActivity with no Java code doesn't
  have. With gesture navigation only the thin handle overlaps the
  bottom strip. Check on the phone; a little Java could fix it later.
- Not run on a device yet. The emulator here is x86_64 and crashed when
  started without a window.

### Step 1. Files

- `talk.cpp` and `name.cpp` find `data/*.txt` next to the exe through
  `/proc/self/exe`. On Android they live in the APK's assets, which
  `std::ifstream` can't read. A `files::readText(name)` helper reads them
  with raylib's `LoadFileText` (which reads assets on Android) and keeps
  today's lookup on desktop.
- `obituary.cpp` reads and writes `data/obituary.txt` in the current
  folder. On Android that goes to the app's internal storage
  (`GetAndroidApp()->activity->internalDataPath`), through a
  `files::savePath(name)` helper.
- F5 reload of `lines.txt` is left out on the phone.

### Step 2. Window and lifecycle

- `InitWindow`'s size is ignored on Android: the window is the screen.
  `Canvas` already scales to the largest size that fits and centres it,
  so phones of another shape than 9:19.5 get bars at the edges.
- The S25 Ultra's screen is 9:19.5, the canvas's own shape. At its FHD+
  setting (1080x2340, Samsung's default) the canvas fits exactly 5 times
  with no bars. At WQHD+ (1440x3120) the scale is 6.67, which `Canvas`
  floors to 6, leaving bars of 72 px at the sides and 156 px top and
  bottom. Either is fine.
- The canvas only fits that well if the game gets the whole screen: hide
  the status and navigation bars (immersive mode) and draw under the
  camera cut-out (`layoutInDisplayCutoutMode="shortEdges"`). If the bars
  stay, the canvas drops to scale 4 with wide bars.
- When the app goes to the background raylib stops the loop; the world
  just waits. If Android kills the app, the run is lost (the obituary only
  holds the dead). Fine for a debug build.
- The Android back button arrives as `KEY_BACK`; it should do what Escape
  does now (pause in the world, back in menus) and must not quit the app.

### Step 3. Touch

raylib turns the first touch into the mouse, so every `ui::button` and the
log/map drags already work. What only has a key today:

| Key | Does | Phone |
|---|---|---|
| Typing in create | Name the human | No typing: rolled name, REFRESH for another |
| Enter / Esc / R in menus | Same as the buttons there | Buttons already cover it |
| 1..n | Sim speed | Speed buttons exist (`speedStep_`) |
| Tab | Reveal the world (debug) | Left out |
| F5 | Reload lines | Left out |
| L / S / M, +/- | Log, stats, map, zoom | Buttons and tapping the name already cover it |
| Mouse wheel | Scroll log / obituary | Dragging already covers it |

Pinch to zoom would be nice but isn't needed for a debug build.

### Step 4. Check on the phone

Install, play a run to a death, check the obituary survives a restart,
check the HUD text fits (the strips are measured, so it should), and note
the frame rate with a big crowd at top speed.

## Build order

0. The user installs the NDK (Android Studio → SDK Manager → SDK Tools →
   NDK (Side by side)). `make apk` clones raylib 5.5 into `build/`.
1. `make apk` builds and the app opens on the phone showing the menu
   (no data files yet: humans say nothing).
2. Step 1 (files), then steps 2 and 3.
3. Step 4 on the device.

## Open questions

All answered: 1 (plain make), 2 (no typing), 4 (left out), 5 (fetched
into `build/`).

3. Answered: Samsung Galaxy S25 Ultra (arm64-v8a, Android 15+). The APK
   is installed by opening the file on the phone; adb is optional.
