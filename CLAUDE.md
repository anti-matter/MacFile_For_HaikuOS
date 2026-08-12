# MacFile AFP Server

An Apple Filing Protocol (AFP) file server for the Haiku operating system. Serves vintage Macintosh clients (MacOS 8.0 through MacOS X 10.5) over TCP/IP port 548. Currently at version **1.8.6**, licensed under the MIT License by Michael J. Conrad.

## SDK Headers

The HaikuOS SDK headers are located at `haiku-os-headers/` (relative to repo root). When writing or reviewing code that includes Haiku system headers, use this path as the base — e.g., `#include <haiku-os-headers/kernel/OS.h>` instead of `<kernel/OS.h>`.

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

afp_createshare/    CLI utility to create shared AFP volumes
  afp_sources/      Share creation implementation
  afpcreate.rsrc    Mac resource fork

ShareVolume/        Volume sharing utility with UAM (User Authentication Method) support
  ShareUAM.cpp      User authentication handler

deps/openssl/       Vendored OpenSSL 1.1.1
  lib/              Prebuilt .so libraries for x86_64 (libcrypto111v, libssl111v)
  headers/          OpenSSL C headers

distribution/       Release artifacts and install scripts
  install-macfile.sh  Installer/uninstaller script
  ReadMe!           Release notes

ref/                Reference documents
  afp3XX.pdf        AFP 3.x protocol specification

LICENSE             MIT License
README.md           User-facing documentation
```

## Build System

Each component builds independently using Haiku's **BeOS Generic Makefile v2.2** engine (`$(BUILDHOME)/etc/makefile-engine`). All makefiles share the same structure:

- `NAME` — output binary name
- `TYPE= APP` — all components are BApplication apps
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

# CLI share utility
cd afp_createshare && make

# Volume sharing tool
cd ShareVolume && make
```

### Full release build

```bash
./build_macfile.sh    # Builds all components, creates install.zip archives
```

Output: `MacFile_x86_Release.zip` or `MacFile_x86_64_Release.zip` in `distribution/`.

## Components

| Component | Binary | Directory | Purpose |
|---|---|---|---|
| **afp_server** | `afp_server` | `afpserver/` | Core AFP daemon — runs as a Haiku BApplication background process |
| **MacFile** | `MacFile` | `afp_config/` | GUI for configuring shares, users, and server settings |
| **CreateAfpShare** | `CreateAfpShare` | `afp_createshare/` | CLI tool to create a new shared volume from the terminal |
| **share_volume** | — | `ShareVolume/` | Volume sharing utility with UAM support (linked into afp_server) |

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

## Dependencies

- **OpenSSL 1.1.1** — vendored under `deps/openssl/` (libcrypto + libssl). Provides DHCAST128 authentication support via `bn.h`, `dh.h`, `cast.h`. Linked as `libcrypto111v.so` / `libssl111v.so`.
- **Haiku SDK** — system headers in `haiku-os-headers/`. Core APIs: `libbe.so`, `libnetwork.so`, `libtextencoding.so`.

## Installation

Run `distribution/install-macfile.sh` on a Haiku system. The script:

1. Extracts binaries to `~/config/non-packaged/apps/`
2. Installs OpenSSL libs to `~/config/non-packaged/lib/`
3. Creates deskbar menu links (Preferences → MacFile, Applications → afp_server)
4. Links afp_server into `~/config/boot/launch/` for auto-start
5. Starts the server and optionally opens the config tool

## Reference

- AFP 3.x protocol spec: `ref/afp3XX.pdf`
- GitHub Wiki: https://github.com/anti-matter/MacFile_For_HaikuOS/wiki
