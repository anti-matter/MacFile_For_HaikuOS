# MacFile Installer + Release — Progress Checkpoint

Goal: (A) native Haiku C++ GUI installer/uninstaller, (B) single-command release build.
Governing constraints: do NOT rewrite the AFP server, alter AFP behavior, add Qt/GTK/Electron,
duplicate install logic, hard-code unconfirmed paths, remove user config, claim success after
partial failure, package stale binaries, or make unrelated stylistic changes. The existing
`distribution/install-macfile.sh` is the authoritative reference. Implement fully (not just propose).

## Architecture decision (DONE)
**Option A — Native GUI + refactored shell backend.** The shell script stays the single
authoritative install/uninstall implementation; the C++ GUI is a thin frontend that spawns it
non-interactively and parses a machine-readable protocol. Rationale: script already works, handles
Haiku-specific ops (`quit application/...`, unzip, symlinks, lib renames); avoids duplicating
install logic; Phase 4 explicitly contemplates keeping the shell as a non-interactive backend;
backward-compatible (no-arg interactive flow preserved); clean failure semantics.

## Task status
- #1 Phase 1 analysis — DONE
- #2 Phase 2 architecture — DONE
- #3 Phase 3-4 backend + GUI — DONE: backend verified; all 8 GUI files written (makefile, Resource.rdef, main.cpp, InstallerApp.{h,cpp}, InstallerWindow.{h,cpp}, InstallWorker.{h,cpp})
- #4 Phase 5 build integration — DONE: `build-release.sh` builds all 3 components (afpserver, afp_config, installer), detects arch from objects dir, verifies all 3 binaries exist before packaging, `set -euo pipefail`, mktemp staging + trap cleanup. Old `build_macfile.sh` left in place (superseded, not deleted).
- #5 Phase 6 single-command release — DONE: `distribution/MacFile_<arch>_Release.zip` = MacFileInstaller + install-macfile.sh + install.zip + ReadMe! (no CreateAfpShare, matches current release). Syntax-checked with `bash -n`.
- #6 docs + Haiku build-server test — DONE. Docs (README.md Installation + Distribution sections; distribution/ReadMe! both point at MacFileInstaller GUI; terminal fallback noted). Full `build-release.sh` run on Haiku R1 beta6 (x86_64) succeeded: all 3 components (afp_server, MacFile, MacFileInstaller) compiled + linked + xres + mimeset clean; `distribution/MacFile_x86_64_Release.zip` produced and verified (see below).

## DONE: refactored backend `distribution/install-macfile.sh` (347 lines, saved, git-modified)
Two thin entry points over one shared core (`installFiles`/`removeFiles`).
- No-arg = interactive (original behavior preserved; archive-missing now `exit 1` not `exit 0`).
- `install`/`uninstall`/`status`/`help` = non-interactive, `PROTOCOL=1`, emits lines on stdout:
  `PROGRESS <0-100> <label>`, `INFO <msg>`, `ERROR <msg>`, `STATUS installed|not_installed`,
  `DONE success|failure`. Exit 0 on success, non-zero on failure.
- `fail()` → protocol: `ERROR`+`DONE failure`; interactive: `alert --stop`. Always `exit 1`.
- `waitForServer()` = bounded liveness (poll `ps` up to ~10s) — replaces blocking `waitfor`.
- `start()` = `"$1" "$2" &`.
- Constants: BIN=`/boot/home/config/non-packaged/apps`, LIB=`.../lib`,
  LINK_PREF=`.../data/deskbar/menu/Preferences`, LINK_APPL=`.../data/deskbar/menu/Applications`,
  LAUNCH=`/boot/home/config/settings/boot/launch`, `ARCHIVEDIR=$(dirname "$0")`, `ARCHIVE=install.zip`.
- install: check archive → `quit application/x-vnd.afp_server` → removeFiles → installFiles
  (unzip→mv libcrypto111v.so→libcrypto.so.1.1, libssl111v.so→libssl.so.1.1 → verify → 3 symlinks)
  → start → waitForServer → DONE success.
- uninstall: check archive → `quit` → removeFiles (rm binaries+libs+3 links) → DONE success.
- Config in `~/.settings/` (afpServerPrefs, afpLogonMessage, afpHostname, user DB, .afpdesktop.db)
  is NEVER touched → preserved on install/uninstall.
- Verified end-to-end in a sandbox (zip via python3; one `ln` failure was a test-harness artifact, not a bug).

## Confirmed codebase idioms (for the C++ GUI — all de-risked)
- Path resolution: `find_directory(B_USER_SETTINGS_DIRECTORY, &path)` is the idiom. Use
  `find_directory(B_APP_DIRECTORY, &appDir)` for the release dir (NOT CWD — Tracker double-click
  may not set expected CWD). `B_APP_DIRECTORY`/`AppPath` not used in codebase but standard+safe.
- Threads: `spawn_thread(func,name,B_NORMAL_PRIORITY,arg)` + `resume_thread(tid)`. Worker =
  `static int32 worker_loop(void* arg)`.
- App signature lives in `.rdef` as `resource app_signature "..."` (or `.rsrc`). Existing sigs:
  `application/x-vnd.MacFile` (afp_config), `application/x-vnd.afp_server` (afpserver),
  `application/x-vnd.afpcreate` (afp_createshare). New: `application/x-vnd.MacFileInstaller`.
- main(): `app = new <App>(); app->Run(); delete be_app; return 0;` window made in `ReadyToRun()`.
- Window pattern (afpAboutWindow.cpp / afpMsgWindow.cpp): fixed `BRect` BWindow
  `B_NOT_RESIZABLE|B_NOT_ZOOMABLE`, center via `MoveTo`, `mainView=new BView(0,0,right,bottom,
  "MainView",B_FOLLOW_ALL_SIDES,B_WILL_DRAW|B_FRAME_EVENTS)` + `SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR))`,
  `new BButton(rect,"b1","Label",new BMessage(CMD))` + `SetFontSize(font_size)` + `AddChild`,
  `SetDefaultButton`, BTextView in `new BScrollView("s",tv,B_FOLLOW_LEFT|B_FOLLOW_TOP,0,false,true)`,
  `MessageReceived` switch, `(new BAlert("",msg,"OK"))->Go()`, `const float font_size=14.0f;`.
- BStringView: `new BStringView(rect,"",TEXT); SetFontSize(18); SetViewColor(...); SetAlignment(B_ALIGN_CENTER)`.
- **DEPRECATED — do NOT use:** `BTextControl::SetMaxBytes` (still at afpMsgWindow.cpp:98 — don't copy).
- Server-running detect (optional): `AFPServerIsRunning()` → `AFPGetUsersLoggedOn(&u)==B_OK` via IPC.
- OpenSSL SONAME: libcrypto111v.so→libcrypto.so.1.1, libssl111v.so→libssl.so.1.1 (rename MUST stay).

## Makefile model
Model `installer/makefile` on `afp_config/makefile` (GUI app, no OpenSSL):
`NAME=MacFileInstaller`, `TYPE=APP`, `SRCS=$(wildcard installer_sources/*.cpp)`, `RSRCS=` (empty),
`RDEFS=Resource.rdef`, `LIBS=be textencoding tracker network`, `OPTIMIZE=FULL`,
`LOCAL_INCLUDE_PATHS=installer_sources`, ends `include $(BUILDHOME)/etc/makefile-engine`.
(afpserver/makefile additionally: `DEFINES=$(DBG)`, `SYMBOLS=TRUE`, `DEBUGGER=$(DBGR)`,
`LIBPATHS=../deps/openssl/lib`, `SYSTEM_INCLUDE_PATHS=/boot/system/develop/headers/openssl`, trailing rc-bug rule.)

## Target release layout (what user extracts)
```
MacFile_x86_64_Release/
├── MacFileInstaller       (GUI app — double-click)
├── install-macfile.sh     (backend)
├── install.zip            (payload: afp_server, MacFile, libcrypto111v.so, libssl111v.so)
└── ReadMe!
```
Do NOT add CreateAfpShare (current release doesn't ship it).

## DRAFTED C++ design (NOT yet on disk)
Files to write under `installer/`:
1. `makefile` (as above).
2. `Resource.rdef`: `resource app_signature "application/x-vnd.MacFileInstaller";` + app_version
   (major=2,middle=0,minor=0,variety=B_APPV_DEVELOPMENT,internal=0,short_info="MacFile Installer").
3. `installer_sources/main.cpp`: standard main().
4. `installer_sources/InstallerApp.{h,cpp}`: sig `application/x-vnd.MacFileInstaller`; `ReadyToRun()`
   → `find_directory(B_APP_DIRECTORY,&appDir)` → `new InstallerWindow(appDir.Path())` → Show.
5. `installer_sources/InstallerWindow.{h,cpp}`: product name BStringView, description, status
   BStringView, BProgressBar, Install/Uninstall/Quit buttons, read-only BTextView log (accumulate
   BString, SetText+ScrollTo); detect installed via
   `BEntry("/boot/home/config/non-packaged/apps/afp_server").Exists() && .IsFile()`;
   message codes 'inst','unin','quit','prog','stat','lgnl','done'; MessageReceived handles
   PROGRESS(SetValue+status)/STATUS/LOG(append)/DONE(BAlert success or failure, refresh state, unbusy).
   NO SetMaxBytes.
6. `installer_sources/InstallWorker.{h,cpp}`: `static int32 worker_loop(void* arg)`; arg=heap
   `struct {BString releaseDir; BString subcommand; BMessenger target;}`; script=`<releaseDir>/install-macfile.sh`;
   `pipe`+`fork`; child: dup2 stdout→pipe, `execl("/bin/sh","sh",script,subcommand,NULL)`, `_exit(127)`;
   parent: close write end, `fdopen(read,"r")`, `getline` loop parse PROGRESS/INFO/ERROR/STATUS/DONE
   (fallback: log verbatim), post BMessages; `waitpid`; success=WIFEXITED&&WEXITSTATUS==0; post DONE.
   `InstallWorker::Spawn(releaseDir,subcommand,BMessenger*)` = spawn_thread+resume_thread.

## Build / test env notes
- No Linux cross-compiler. Build on Haiku via `ssh user@haikuos`; export `BUILDHOME=/boot/system/develop`.
- Debug: `dbgbuild.sh` (=`DBG=DEBUG DBGR=TRUE make`); debug binary at
  `afpserver/objects.x86_64-cc13-debug/afp_server`. A lone `mimeset` failure = treat as success.
- Local (Linux) shell is zsh: **always quote globs** (`--include='*.cpp'`). `zip` NOT installed;
  use `python3` (zipfile) or `jar` to make zips. `unzip`, `python3`, `jar` available.
- `/bin/sh` on Haiku = GNU bash 5.3 → `set -euo pipefail` works; `ps` yes, `listapps` no.

## Current release build (to be replaced)
`build_macfile.sh`: prompts "Build for release?"; builds afpserver + afp_config only; makes
`distribution/install.zip` (x64 adds the 2 openssl .so); makes `MacFile_x86_64_Release.zip`
(release zip = install.zip + install-macfile.sh + ReadMe!). No `set -euo pipefail`.

## Build fixes applied this session (all committed to origin/installer)
- `InstallerWindow.cpp`: BWindow 3rd arg is `window_type`, 4th is `flags` — this Haiku build
  has NO `B_TITLED`/`B_CLOSABLE`. Changed to `B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE`
  (matches afpMsgWindow.cpp's `B_MODAL_WINDOW` + flags pattern). Added `#include <Screen.h>`
  (BScreen lives in Screen.h, not Window.h).
- `InstallWorker.cpp`: `worker_loop` is a private static member in the header; it was defined
  as a file-scope free function → `undefined reference` at link. Changed to
  `int32 InstallWorker::worker_loop(void* arg)`.
- `build-release.sh`: release-zip target was relative (`../../distribution/...`) but the staging
  dir is a `mktemp` dir in /tmp, so `zip` could not create the output file. Captured
  `REPO_ROOT="$(pwd)"` after `cd "$(dirname "$0")"` and target the absolute
  `"$REPO_ROOT/$RELEASE_ARCHIVE"`.

## Runtime bug fixes (user ran the installer on a real Haiku system, 2026-09-08)
Two runtime bugs reported after running the built installer; both fixed in
`installer/installer_sources/` and build-verified on the Haiku server.
- **Bug 1 — buttons stuck after uninstall.** After a completed uninstall the
  Uninstall button stayed enabled and Install stayed disabled. Root cause:
  `fInstalled` was only set at construction and on `INSTALL_M_STATUS`, but the
  `install`/`uninstall` backend paths never emit a `STATUS` line (only the
  `status` subcommand does), so `fInstalled` was stale and `RefreshState()`
  kept the wrong button enabled. Fix: in the `INSTALL_M_DONE` handler, re-detect
  real state from disk before refreshing —
  `BEntry e(SERVER_PATH); fInstalled = e.Exists() && e.IsFile();` — so the
  buttons flip correctly after both install and uninstall.
- **Bug 2 — process lingers after quit.** Closing the window hid it but the
  `MacFileInstaller` process kept running and had to be killed. Root cause: a
  `BApplication` does NOT auto-quit when its last window closes, so the app's
  message loop (and process) outlived the window. Fix: added a
  `WindowClosed(bool wasCanceled)` override (declared in `InstallerWindow.h`,
  defined in `InstallerWindow.cpp`) that calls `Quit()`, so the process exits
  with the window. (A lingering worker thread was ruled out: in Haiku, when
  `main()` returns the process terminates and all threads are killed.)
- Committed as `45ac578` ("Fix installer button state and process exit"),
  pushed to `origin/installer`, and rebuilt clean on the Haiku server
  (all 4 sources compile + link + xres + mimeset OK).
- Release re-packaged on the Haiku server (`BUILDHOME=/boot/system/develop
  ./build-release.sh`): `distribution/MacFile_x86_64_Release.zip` now contains
  the fixed `MacFileInstaller` (49677 bytes) + install-macfile.sh + install.zip +
  ReadMe!. Verified with `unzip -l`. Ready for the user to re-test both bugs.

## Runtime bug fixes round 2 (user re-ran installer, 2026-09-08)
Two more runtime issues reported after the round-1 fixes:
- **Bug 2 (still broken in round 1) — process lingers after quit / window close.**
  Round 1's fix used a `virtual void WindowClosed(bool)` override, but that is NOT a real
  BWindow virtual in this build — it was a dead method that never fired. The real window-close
  hook is `virtual bool QuitRequested();`, which `BWindow::Quit()` (called by both the Quit
  button and the X close box) invokes. The default `BWindow::QuitRequested()` only hides the
  window and does not quit the `BApplication`, so the message loop + process outlived the window.
  Fix: `InstallerWindow.h` line 18 is now `virtual bool QuitRequested();` (replaced the bogus
  `WindowClosed` declaration); `InstallerWindow.cpp` defines it as
  `be_app->PostMessage(B_QUIT_REQUESTED); return BWindow::QuitRequested();` — the proven idiom
  from the working `afp_config` app. This covers BOTH the Quit-button and window-close paths.
- **Bug 3 (new) — log scrollbar overflows the window's right edge.**
  The log `BTextView` target frame is `(10, 160, 470, 530)` (460 wide). `BScrollView` has no
  frame constructor — it computes its own outer frame from the target PLUS the 14px vertical
  scrollbar, so its natural right edge landed at 470 + 14 = 484, past the 480-wide window.
  **API gotcha (this build):** `BScrollView`/`BView` have NO `SetFrame` and NO `MoveResize` —
  the first attempt (`logScroll->SetFrame(...)`) failed to compile
  (`'class BScrollView' has no member named 'SetFrame'`). Available frame methods are
  `MoveTo` + `ResizeTo`. Fix in `InstallerWindow.cpp` (lines 153-154):
  `logScroll->MoveTo(10, 160); logScroll->ResizeTo(470 - 10, 530 - 160);` — pins the scrollview
  outer frame to 460 wide at x=10 (right edge x=470); the target shrinks to 446 wide to make
  room for the 14px scrollbar, so the scrollbar's right edge sits at x=470 — the same 10px
  buffer as the text area's left side.
- Both fixes committed as `d5fba26` ("Fix installer scrollbar overflow and process exit"),
  pushed to `origin/installer`, built CLEAN on the Haiku server (all 4 sources compile + link +
  xres + mimeset OK — no mimeset failure this time), and re-packaged:
  `distribution/MacFile_x86_64_Release.zip` now contains the fixed `MacFileInstaller`
  (50261 bytes) + install-macfile.sh + install.zip + ReadMe! (verified with `unzip -l`).
  Ready for the user to re-test both the lingering-process and scrollbar issues.

## Runtime bug fixes round 3 (user re-ran installer, 2026-09-08/09)
User feedback after round 2: "The process still lingers if the quit BUTTON is used...
The process no longer lingers if the window close box is used. The scrollbar is not
visible at all. It looks like the text area goes off the right edge and the bottom
of the window completely."
- **Bug 2 (button path) — round-2 `QuitRequested()` fix covered only the close box.**
  Root cause: the Quit BUTTON path was `case CMD_QUIT: Quit(); break;` → `BWindow::Quit()`,
  which closes the window but does NOT quit the `BApplication`. The close box invokes
  `QuitRequested()` directly, which is why it worked. Fix in `InstallerWindow.cpp`
  `MessageReceived`: the `CMD_QUIT` handler now does `be_app->PostMessage(B_QUIT_REQUESTED);`
  — the same proven idiom as `afpMainWindow.cpp:125` (button built with
  `new BMessage(B_QUIT_REQUESTED)`) and `afpConfigApplication.cpp:93,100,113`.
- **Bug 3 (scrollbar) — round-2 `MoveTo/ResizeTo` was right but the target width was
  wrong.** Root cause: the `BTextView` target's frame is in the scrollview's LOCAL
  coordinate system, not the window's. Round 2 left the target at 460 wide (the outer
  width), so with the 14px vertical scrollbar the text + scrollbar exceeded the
  scrollview interior and the scrollbar was pushed off-window. Fix: the `BTextView`
  target is now `BRect(0, 0, 446, 370)` (anchored at local (0,0), width 446 = outer 460
  minus the 14px scrollbar); the scrollview itself is positioned in the window via
  `logScroll->MoveTo(10, 160); logScroll->ResizeTo(470 - 10, 530 - 160);` so its outer
  frame is (10,160)-(470,530) — the same 10px buffer as every other element. The
  scrollbar now sits inside the right edge (x=456..470) and nothing overflows the
  480x540 window.
- Both fixes committed as `27a933e` ("Fix Quit-button process exit and log scrollbar
  geometry"), pushed to `origin/installer`, built CLEAN on the Haiku server (all 4
  sources compile + link + xres + mimeset OK), and re-packaged:
  `distribution/MacFile_x86_64_Release.zip` now contains the fixed `MacFileInstaller`
  (50277 bytes) + install-macfile.sh + install.zip + ReadMe! (verified with `unzip -l`).
  **PENDING: user re-test of (a) Quit-button process exit and (b) scrollbar/log geometry.**
  Geometry cannot be confirmed by a build alone — needs runtime verification on a real
  Haiku system.

## Runtime bug fixes round 4 (2026-09-09)
Round 3's scrollbar fix (BTextView target `BRect(0,0,446,370)` in LOCAL coords + scrollview
`MoveTo(10,160)`/`ResizeTo(460,370)`) was still reported broken. The root cause of the
recurring overflow was a coordinate-system mismatch: the target's frame and the scrollview's
positioned frame were being set in two different coordinate systems, so the scrollview's
interior did not line up with the window.

**Fix (round 4) — realigned to the PROVEN `afpMsgWindow.cpp` idiom:** the `BTextView` target's
frame is set in the PARENT (window) coordinate system and represents the scrollview's INTERIOR;
the `BScrollView` then computes its own outer frame from that target PLUS the 14px vertical
scrollbar (plus border insets). No `MoveTo`/`ResizeTo` is called on the scrollview. In
`InstallerWindow.cpp`:
```cpp
BRect logRect(10, 160, 470 - 14, 530);   // target interior, window coords, right edge 456
fLogView = new BTextView(logRect, "log", BRect(2, 2, 240, 150), 0, B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE);
...
logScroll = new BScrollView("logScroll", fLogView, B_FOLLOW_LEFT | B_FOLLOW_TOP, 0, false, true);
mainView->AddChild(logScroll);
```
**Geometrically confirmed against the actual Haiku source** (`src/kits/interface/ScrollView.cpp`,
`_ComputeFrame`): the scrollview's outer frame = target frame + vertical scrollbar preferred
width (14px) + border insets, and the target is re-anchored to the scrollview origin and fills
the interior. So target `(10,160)-(456,530)` → scrollview outer ≈ `(8,158)-(472,532)`; the
scrollbar sits inside the right edge (window x≈454–468) and nothing overflows the 480×540 window.
This matches the proven-working `afpMsgWindow.cpp` exactly.

- Committed as `0ca4907` ("Fix log scrollbar geometry (round 4)"; also carried a one-line
  `CLAUDE.md` SDK-header-path doc fix: `haiku-os-headers/` → `~/haiku-sdk/boot/system/develop/headers/`),
  pushed to `origin/installer`, built CLEAN on the Haiku server (all 4 sources compile + link +
  xres + mimeset OK), and re-packaged:
  `distribution/MacFile_x86_64_Release.zip` now contains the round-4 `MacFileInstaller`
  (49925 bytes) + install-macfile.sh + install.zip + ReadMe! (verified with `unzip -l`).
- **RESOLVED: user runtime-confirmed both (a) Quit-button process exit (Bug 2, fixed round 3)
  and (b) scrollbar/log-area geometry (Bug 3, fixed round 4).** User reported: "the scrollbar and
  quit button have been fixed." Both are now closed.

## ROUND 5 — Bug 4: log text wraps at ~half width (2026-09-09)

**Reported (user, verbatim):** "the text in the scroll area wraps too soon. all text wraps at
approximately halfway across the scroll area."

**Root cause — CONFIRMED against the actual Haiku source** (`src/kits/interface/TextView.cpp`):
`BTextView` wraps to the width of its **content rect** (the 3rd constructor argument), NOT its
frame. `_InitObject` stores `fTextRect` and calls `_UpdateInsets(textRect)`; `_UpdateInsets`
derives the left/right insets from the difference between the FRAME and the content rect
(`rightInset = bounds.right >= rect.right ? bounds.right - rect.right : 0`), and the wrap width
is `_ViewWidth() = Bounds().Width() - leftInset - rightInset`.

Our log frame is `logRect(10, 160, 456, 530)` → width **446px**. The old content rect was the
fixed narrow `BRect(2, 2, 240, 150)` (width 238). Because `240 < 456`, this left a
**rightInset of 456 − 240 = 216px**, so the wrap width was `446 − 0 − 216 = 230px ≈ half` the
frame — exactly the reported symptom. (The `afpMsgWindow.cpp` reference uses the same narrow
content rect but with a *narrower* frame, where `textRect.right > frame.right` clamps the right
inset to 0 — so it wraps fine there and is NOT a fix to copy.)

**Fix (round 5) — span the frame with a small symmetric 2px inset** so the wrap width is the full
interior. In `InstallerWindow.cpp`, the content rect is now computed from the frame:
```cpp
BRect logRect(10, 160, 470 - 14, 530);
BRect textRect(logRect.left + 2, logRect.top + 2,
    logRect.right - 2, logRect.bottom - 2);   // (12,162)-(454,528) → 2px symmetric insets
fLogView = new BTextView(logRect, "log",
    textRect, 0, B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE);
```
→ leftInset = 2, rightInset = 2, wrap width = `446 − 2 − 2 = 442px` (full interior). No other
lines touched; the frame, scrollview wiring, and all other code are unchanged.

- Committed as `05404eb` ("Fix log text wrapping at half width (round 5)"), pushed to
  `origin/installer`, built CLEAN on the Haiku server (all 4 sources compile + link + xres +
  mimeset OK), and re-packaged: `distribution/MacFile_x86_64_Release.zip` (Sep 9 06:30) contains
  the round-5 `MacFileInstaller` (49925 bytes) + install-macfile.sh + install.zip + ReadMe!
  (verified with `unzip -l`).
- **PENDING: user runtime re-test of the log text wrapping (Bug 4, fixed round 5).** Wrap width
  cannot be confirmed by a build alone — needs verification on a real Haiku system. NOT to be
  claimed fixed until then.

## VERIFIED release build (Haiku R1 beta6, x86_64, 2026-09-08)
`BUILDHOME=/boot/system/develop ./build-release.sh` → all 3 components built clean
(afp_server, MacFile, MacFileInstaller: compile + link + xres + mimeset OK), then:
- `distribution/MacFile_x86_64_Release.zip` = MacFileInstaller + ReadMe! + install-macfile.sh + install.zip
- `distribution/install.zip` (payload) = afp_server + MacFile + libcrypto111v.so + libssl111v.so
Both zips verified with `unzip -l`. Release build is complete and correct.

## CURRENT STATE (2026-09-09)
All phases (1–6) + docs + Haiku build-server test are complete. The installer is functional;
we are in a runtime bug-fix cycle driven by the user running the built installer on a real Haiku
system.

**Bug status:**
- Bug 1 — button state after uninstall: **FIXED (round 2), runtime-confirmed.**
- Bug 2 — Quit-button process lingering: **FIXED (round 3), runtime-confirmed** ("quit button … fixed").
- Bug 3 — scrollbar/log-area geometry: **FIXED (round 4), runtime-confirmed** ("scrollbar … fixed").
- Bug 4 — log text wraps at ~half width: **FIXED (round 5), built clean + re-packaged, PENDING
  runtime verification.**

**Next action:** user re-tests log text wrapping on a real Haiku system (fresh
`distribution/MacFile_x86_64_Release.zip`, Sep 9 06:30). Do NOT claim Bug 4 fixed until
runtime-confirmed.
