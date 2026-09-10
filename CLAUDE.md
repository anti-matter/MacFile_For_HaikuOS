# MacFile AFP Server

An Apple Filing Protocol (AFP) file server for the Haiku operating system. Serves vintage Macintosh clients (MacOS 8.0 through MacOS X 10.5) over TCP/IP port 548. Currently at version **2.0**, licensed under the MIT License by Michael J. Conrad.

## SDK Headers

The HaikuOS SDK headers are located at `~/haiku-sdk/boot/system/develop/headers/` (relative to repo root). When writing or reviewing code that includes Haiku system headers, use this path as the base — e.g., `#include <~/haiku-sdk/boot/system/develop/headers/kernel/OS.h>` instead of `<kernel/OS.h>`.

## Project Structure

```
afpserver/          Core AFP daemon (the main server)
  afp_sources/      All AFP protocol implementation files (~50 source files)
  dbgbuild.sh       Debug build script (enables DBGWRITE logging)
  makefile          Haiku BeOS Generic Makefile v2.2

afp_config/         GUI configuration application ("MacFile")
  afpconfig_sources/ Configuration UI and settings management
  afpconfig.rsrc    Mac resource fork for the config app
  makefile          Build configuration

afp_createshare/    Tracker add-on to share a directory over AFP
  afp_sources/      Share creation implementation
  afpcreate.rsrc    Mac resource fork

ShareVolume/        Volume sharing utility with UAM (User Authentication Method) support
  ShareUAM.cpp      User authentication handler

installer/          GUI installer/uninstaller application ("MacFileInstaller")
  installer_sources/ Installer UI + worker (spawns install-macfile.sh, parses its output)
  Resource.rdef     Resource definition
  makefile          Build configuration

deps/openssl/       Vendored OpenSSL 1.1.1
  lib/              Prebuilt .so libraries for x86_64 (libcrypto111v, libssl111v)
  headers/          OpenSSL C headers

distribution/       Release artifacts and install scripts
  install-macfile.sh  Installer/uninstaller script (authoritative install backend)
  ReadMe!           Release notes

build-release.sh    Single-command release build (server + config + tracker add-on + installer)
build_macfile.sh    Legacy release build (server + config only, no installer)

ref/                Reference documents
  afp3XX.pdf        AFP 3.x protocol specification

LICENSE             MIT License
README.md           User-facing documentation
```

## Build System

Each component builds independently using Haiku's **BeOS Generic Makefile v2.2** engine (`$(BUILDHOME)/etc/makefile-engine`). All makefiles share the same structure:

- `NAME` — output binary name
- `TYPE` — `APP` for the BApplication components; `SHARED` for the `afp_createshare` tracker add-on
- `SRCS` — wildcard over source subdirectory
- `RSRCS` / `RDEFS` — Mac resource fork and resource definition files
- `LIBS` — linked libraries (be, network, textencoding, etc.)
- `DBG` / `DBGR` — makefile variables controlling debug builds

### Build commands

```bash
# AFP daemon (release)
cd afpserver && make

# AFP daemon (debug — enables DBGWRITE logging)
cd afpserver && ./dbgbuild.sh

# GUI config app
cd afp_config && make

# Tracker add-on (shared library)
cd afp_createshare && make

# Volume sharing tool
cd ShareVolume && make

# GUI installer/uninstaller
cd installer && make
```

### Full release build

```bash
./build-release.sh    # Builds server + config + tracker add-on + installer, creates install.zip + release archive
```

`build-release.sh` is the current single-command release build. It builds all four
components (`afpserver`, `afp_config`, `afp_createshare`, `installer`),
packages
`distribution/install.zip` (the server + config + tracker add-on binaries, plus the
OpenSSL libs on x86_64), and stages a `MacFile_<arch>_Release/` directory containing the
`MacFileInstaller` binary, `install-macfile.sh`, `install.zip`, and `ReadMe!` — zipped
to `distribution/MacFile_<arch>_Release.zip`.

> `build_macfile.sh` is the older, interactive script. It builds only `afpserver` and
> `afp_config` (not the installer) and does not produce the release archive; use
> `build-release.sh` for a full release.

### Remote build server (HaikuOS)

The project is compiled on a real Haiku machine (there is no cross-compiler for Linux). A build server is available over SSH and should be used to verify that changes build.

- **Access**: `ssh user@haikuos` — key-based auth, no password prompt.
- **Server OS**: Haiku R1 beta6, x86_64.
- **Repo on server**: `~/dev/MacFile_For_HaikuOS` (same GitHub `origin` as this clone).

**Gotcha — `BUILDHOME`:** it is *not* set in a non-interactive SSH session, but every makefile does `include $(BUILDHOME)/etc/makefile-engine`. Without it, the include resolves to `/etc/makefile-engine` and the build fails immediately with `No rule to make target '/etc/makefile-engine'`. Always export `BUILDHOME=/boot/system/develop` when building over SSH.

**Build workflow** (from this repo):

```bash
# 1. Commit and push your branch from here.
git push origin <branch>

# 2. On the server, fetch and check it out. A throwaway branch keeps the
#    server's working checkout (usually work3) untouched:
ssh user@haikuos 'cd ~/dev/MacFile_For_HaikuOS \
  && git fetch origin && git checkout -B build-test origin/<branch>'

# 3. Build the AFP daemon (debug). Note the BUILDHOME export.
ssh user@haikuos 'cd ~/dev/MacFile_For_HaikuOS/afpserver \
  && BUILDHOME=/boot/system/develop ./dbgbuild.sh'
```

Notes:
- **Debug build**: use `dbgbuild.sh` (which is `DBG="DEBUG" DBGR="TRUE" make`), not plain `make`.
- **Output binary**: `afpserver/objects.x86_64-cc13-debug/afp_server` (debug) — *not* `afpserver/afp_server`.
- **Clean build**: pass the same debug vars to `make clean`, or it cleans the *release* objects dir and the debug build then does nothing:
  ```bash
  ssh user@haikuos 'cd ~/dev/MacFile_For_HaikuOS/afpserver \
    && BUILDHOME=/boot/system/develop DBG=DEBUG DBGR=TRUE make clean \
    && BUILDHOME=/boot/system/develop ./dbgbuild.sh'
  ```

**Known build-server issue (as of 2026-09-01):** the final `mimeset` step can fail with `mimeset: ".../afp_server": No such file or directory` even though the file exists. This is a filesystem/environment problem on the server, **not** a code or compile error — by that point every source has compiled and the binary is fully linked and resource-merged (`xres`). If only `mimeset` fails, treat the build as successful and use the binary at the path above. (The drive is being diagnosed.)

## Components

| Component | Binary | Directory | Purpose |
|---|---|---|---|
| **afp_server** | `afp_server` | `afpserver/` | Core AFP daemon — runs as a Haiku BApplication background process |
| **MacFile** | `MacFile` | `afp_config/` | GUI for configuring shares, users, and server settings |
| **CreateAfpShare** | `Share with Macs (AppleShare)` | `afp_createshare/` | Tracker add-on (shared library) — share a directory over AFP from the tracker's Add-ons menu. The makefile builds it as `CreateAfpShare` (the makefile engine can't handle a spaced NAME in its xres step); `build-release.sh` renames the output to `Share with Macs (AppleShare)`, and the tracker labels add-ons by file name |
| **share_volume** | — | `ShareVolume/` | Volume sharing utility with UAM support (linked into afp_server) |
| **MacFileInstaller** | `MacFileInstaller` | `installer/` | GUI to install/uninstall MacFile — thin frontend that runs `distribution/install-macfile.sh` |

## Architecture

```
afp_server (BApplication)
├── TCP listener (port 548) → ServerConnection thread per client
│   └── dsi_connection (DSI framing, packet parsing)
│       └── afp_session (AFP state: volumes, files, desktop refs, auth)
│           ├── fp_volume (shared directory, open file tracking)
│           ├── OPEN_FORK_ITEM (BFile*, fork type, range locks)
│           └── OPEN_DESK_ITEM (icons, comments, APPL mappings)
├── dsi_scavenger (background thread: tickles, dead session cleanup)
├── volume_blist (shared list of all fp_volume instances)
└── dsi_stats (network statistics collector)
```

### Key classes and their roles

| Class | File | Role |
|---|---|---|
| `afpServerApplication` | `afpServerApplication.{cpp,h}` | BApplication entry point, message routing, Pulse() loop |
| `dsi_connection` | `dsi_connection.{cpp,h}` | TCP socket I/O, DSI protocol framing, request/reply dispatch |
| `afp_session` | `afp_session.{cpp,h}` | Per-client AFP state: volumes, files, desktop refs, auth info |
| `fp_volume` | `fp_volume.{cpp,h}` | Shared volume representation: path, open files, dirty tracking |
| `dsi_scavenger` | `dsi_scavenger.{cpp,h}` | Background thread: session tickles, dead session cleanup |
| `dsi_stats` | `dsi_stats.{cpp,h}` | Network statistics (bytes sent/received, packet counts) |
| `afp_buffer` | `afp_buffer.{cpp,h}` | Growing reply buffer for AFP responses |
| `finder_info` | `finder_info.{cpp,h}` | Mac file type/creator lookup by extension |
| `ClassicMacIcon` | `ClassicMacIcon.{cpp,h}` | 1-bit 32×32 volume icon (bitmap + mask) served in `GetSrvrInfo` via the `VolumeIconAndMask` field |

### AFP command handler files (formerly `afp.cpp`)

The original monolithic `afp.cpp` was split into four domain files. When locating an AFP command handler, check the file that matches its concern:

| File | Contains |
|---|---|
| `afp_dispatch.cpp` | Includes, globals, the dispatch table, time helpers, and `FPDispatchCommand()` |
| `afp_volcmds.cpp` | Server/session/volume commands — `GetSrvrInfo`, `FPLogin`, `FPGetSrvrParms`, etc. |
| `afp_catalog.cpp` | File/directory catalog commands — `FPEnumerate`, `FPCreate`, `FPDelete`, `FPMoveAndRename`, etc. |
| `afp_fork.cpp` | Fork I/O commands — `FPOpenFork`, `FPRead`, `FPWrite`, `FPFlush`, byte-range locks |

## GUI Installer (`MacFileInstaller`)

A native Haiku C++ BApplication that installs/uninstalls MacFile from a window. It is a **thin frontend**: it contains no install logic of its own and delegates all filesystem work to `distribution/install-macfile.sh`, which remains the single authoritative install/uninstall backend.

```
MacFileInstaller (BApplication)
└── InstallerWindow (window, buttons, status/progress, log)
    └── InstallWorker (worker thread)
        └── fork() + execl("/bin/sh", install-macfile.sh, <subcommand>)
            └── reads the script's stdout line-by-line, posts BMessages to the window
```

- **`InstallerWindow.{cpp,h}`** — the UI: Install / Uninstall / Quit buttons, a status line, a percentage progress line (no `BProgressBar` in this Haiku build), and a read-only log in a `BScrollView`. Detects installed state from `/boot/home/config/non-packaged/apps/afp_server`.
- **`InstallWorker.{cpp,h}`** — spawns the script in a worker thread over a pipe and parses its output, posting `INSTALL_M_PROGRESS` / `INSTALL_M_STATUS` / `INSTALL_M_LOG` / `INSTALL_M_DONE` messages back to the window.
- **Line protocol** (emitted by `install-macfile.sh`, parsed by the worker): `PROGRESS <0-100> <label>`, `INFO <msg>`, `ERROR <msg>`, `STATUS installed|not_installed`, `DONE success|failure`. Exit status 0 = success. The `install`/`uninstall` paths do not emit a `STATUS` line (only `status` does), so the window re-detects installed state from disk on `DONE`.
- **Subcommands** (run by the worker, dispatched in `install-macfile.sh`): `install`, `uninstall`, `status`, `help`; no argument = interactive mode.

## AFP Protocol Support

Implements **AFP 2.2 through 3.3** dynamically per session. Each client negotiates its own version during the `FPLogin` exchange. Feature selection is driven by the AFP version the client reports.

### Key capabilities

- Unicode filenames (Long names / Extended / Full UTF-8 depending on AFP version)
- File IDs / node_ref mapping for reliable tracking across renames/moves
- Resource fork emulation via extended attributes (`Afp_Resource`)
- Byte-range locking with steal support for disconnected sessions
- Session reconnect with 32-entry replay cache (AFP 3.3+)
- 64-bit volume sizes (32-bit clamped to 4 GB for AFP 2.x compatibility)
- Finder Info blocks (32 B) with type/creator assignment by extension
- Extended attributes (5 custom AFP xattrs: Finder Info, resource fork, file attributes, long names, comments)
- Blank access privileges — per-file Owner/User/Guest search/read/write ACLs (AFP 3.2+)
- Default privileges from parent directory on new file creation
- No exchange files — prevents AFP rename-from-volume conflicts
- TMLock steal support — clients can steal byte-range locks held by disconnected sessions
- Block size: 1024 bytes — matches Classic Mac OS convention
- Sleep notification (`FPZzzz`)
- Sync commands (`FPSyncDir` / `FPSyncFork`)

### AFP command codes

The server implements all standard AFP commands (codes 1–79) plus extended variants. See `afpserver/afp_sources/afp.h` for the full enum (`afpVolClose` through `afpSyncFork`). Commands are dispatched via `FPDispatchCommand()`.

## Authentication Methods (UAMs)

| UAM | Constant | Description |
|---|---|---|
| **Guest** | `afpUAMGuest` | Unauthenticated access — Guest account auto-created on first startup |
| **Cleartxt passwrd** | `afpUAMClearText` | Plaintext password exchange over TCP |
| **DHCAST128** | `afpUAMDHCAST128` | Diffie-Hellman key exchange with CAST encryption |

User database is maintained by the server (not integrated with Haiku system accounts), stored in `~/.settings/` with schema versioning (`AFP_USERDB_VERSION 0x02000000`) and migration support. User flags: enabled, admin, must-change-password, don't-display, can-change-password.

## Special .res File Handling

Files with the `.res` extension receive special treatment optimized for **source control workflows**:

1. **Data-fork-as-resource-fork** — resource fork content stored in the file's data fork instead of an extended attribute (`Afp_Resource`). Appears as a regular binary blob on non-Mac systems (Git, SVN).
2. **Standard disk I/O** — normal `BFile` I/O on the data fork, no in-memory caching or dirty-flagged writes.
3. **No metadata leakage** — zero Mac-specific metadata beyond Finder Info block (type/creator) stored separately.

Implementation: `IsResFile()` checks extension; AFP resource fork operations are redirected to the file's data fork.

## CodeWarrior File Extension Mapping

Files created via the server automatically get Mac file type and creator code based on extension (lookup table in `finder_info.cpp`). Ensures files appear correctly in the Classic Mac OS Finder with proper icons and double-click behavior.

| Extension | File Type | Creator | Purpose |
|---|---|---|---|
| `.p`, `.cp` | `TEXT` | `CWIE` | CodeWarrior IDE source file |
| `.cpp`, `.c` | `TEXT` | `CWIE` | C++ / C source file |
| `.h`, `.hpp` | `TEXT` | `CWIE` | Header file |
| `.pch`, `.pch++` | `TEXT` | `CWIE` | Precompiled header |
| `.prj` | `MMPr` | `CWIE` | CodeWarrior Professional 1 project |
| `.mcp` | `MMPr` | `CWIE` | CodeWarrior Professional 4+ project |
| `.cwlib` | `MPLF` | `CWIE` | CodeWarrior library file |
| `.ppob` | `rsrc` | `MWC2` | PowerPlant object file |
| `.err` | `MMCH` | `CWIE` | Error list window |

Unrecognized extensions receive default type/creator of `"???? "` / `"????"`.

## Coding Conventions

- **Class naming**: `camelCaseWithLeadingCapital` for classes (`afp_session`, `dsi_connection`, `fp_volume`)
- **Function naming**: `FP` prefix for AFP command handlers (`FPOpenFork`, `FPRead`, etc.); camelCase for methods (`GetVolumeName`, `AddOpenFile`)
- **Error type**: `AFPERROR` typedef'd as `int32`; use `AFP_OK` (0), `AFP_SUCCESS(e)`, `AFP_FAILURE(e)` macros
- **Debug logging**: `DBGWRITE(level, format, ...)` when built with DEBUG; no-op otherwise. Levels: `dbg_level_error` through `dbg_level_dump_out`
- **Performance tracing**: `BEGIN_PERF_MEASURE()` / `END_PERF_MEASURE(s)` in DEBUG builds
- **IN/OUT params**: `IN` / `OUT` macros mark parameter direction
- **Preprocessor guards**: `#ifndef __name__` / `#define __name__` / `#endif //__name__` pattern
- **AFP error codes**: negative enum values starting at -5000 (`afpAccessDenied`, etc.)
- **BeOS/Haiku error strings**: `GET_BERR_STR(e)` macro in DEBUG builds maps `B_*` errors to string names
- **Modern C++**: prefer `std::vector` over hand-managed arrays/lists, and use current Haiku APIs — the v2.0 pass removed deprecated calls such as `BTextControl::SetMaxBytes` and `BString::Format`, so don't reintroduce them

## Dependencies

- **OpenSSL 1.1.1** — vendored under `deps/openssl/` (libcrypto + libssl). Provides DHCAST128 authentication support via `bn.h`, `dh.h`, `cast.h`. Linked as `libcrypto111v.so` / `libssl111v.so`.
- **Haiku SDK** — system headers in `haiku-os-headers/`. Core APIs: `libbe.so`, `libnetwork.so`, `libtextencoding.so`.

## Installation

Two entry points, both backed by the same `distribution/install-macfile.sh` script:

- **GUI installer** — run the `MacFileInstaller` binary (from the release archive). It offers Install / Uninstall buttons and shows progress and a log. This is the recommended path for end users.
- **Command line** — run `distribution/install-macfile.sh` on a Haiku system. Subcommands: `install`, `uninstall`, `status`, `help`; no argument runs interactive mode.

The `install` subcommand:

1. Extracts binaries to `~/config/non-packaged/apps/`
2. Installs OpenSSL libs to `~/config/non-packaged/lib/`
3. Installs the "Share with Macs (AppleShare)" tracker add-on to `~/config/non-packaged/add-ons/Tracker/`
4. Creates deskbar menu links (Preferences → MacFile, Applications → afp_server)
5. Links afp_server into `~/config/boot/launch/` for auto-start
6. Starts the server and optionally opens the config tool

Neither install nor uninstall touches the user's configuration in `~/.settings/`.

## Reference

- AFP 3.x protocol spec: `ref/afp3XX.pdf`
- GitHub Wiki: https://github.com/anti-matter/MacFile_For_HaikuOS/wiki
