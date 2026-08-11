# MacFile AFP Server

An Apple Filing Protocol (AFP) file server for the Haiku operating system, designed to serve vintage Macintosh clients (MacOS 8.0 through MacOS X 10.5) over TCP/IP port 548. The server is currently at version **1.8.6** and licensed under the MIT License by Michael J. Conrad.

## Overview

`afp_server` is a Haiku `BApplication` daemon that provides AFP file sharing with full support for the AFP 2.2 through AFP 3.3 protocol specifications. It manages shared volumes, user authentication, session tracking, and all AFP commands expected by Classic Mac OS clients. The server runs as a background process and communicates with the `MacFile` GUI configuration application via Haiku `BMessage` IPC.

## AFP Protocol Support (2.2 – 3.3)

The server implements **AFP versions 2.2, 3.0, 3.1, 3.2, and 3.3** dynamically per session — each client negotiates its own version during the `FPLogin` exchange. Feature selection is driven by the AFP version the client reports:

| Feature | AFP 2.2 | AFP 3.0+ | AFP 3.1 | AFP 3.2+ | AFP 3.3+ |
|---|---|---|---|---|---|
| Unicode filenames | -- | Long names (AFPName) | Extended | Full UTF-8 | Full UTF-8 |
| Long filenames (255 chars) | Yes | Yes | Yes | Yes | Yes |
| File IDs / node_ref mapping | Yes | Yes | Yes | Yes | Yes |
| Resource fork emulation | Yes | Yes | Yes | Yes | Yes |
| Finder Info blocks (32 B) | Yes | Yes | Yes | Yes | Yes |
| Extended attributes | Yes | Yes | Yes | Yes | Yes |
| Byte-range locking | Yes | Yes | Yes | Yes | Yes |
| Session reconnect / crash recovery | -- | -- | -- | -- | Replay cache (32 entries) |
| 64-bit free/total bytes | Yes | Yes | Yes | Yes | Yes |
| Block size (1024 B) | Yes | Yes | Yes | Yes | Yes |
| Sleep notification (`FPZzzz`) | -- | -- | Yes | Yes | Yes |
| Sync commands (`FPSyncDir/FPSyncFork`) | -- | -- | -- | Yes | Yes |

### Volume Parameters

Each shared volume reports the following capabilities to clients:

- **File IDs** — maps AFP file references to Haiku `node_ref` for reliable tracking across renames/moves
- **Unicode names** — full UTF-8 filename support on the wire
- **Extended attributes** — 5 custom AFP xattrs store Finder Info, resource fork data, file attributes, long names, and comments
- **Blank access privileges** — per-file Owner/User/Guest search/read/write ACLs (AFP 3.2+)
- **Default privileges from parent directory** — new files inherit the parent's permissions
- **No exchange files** — prevents AFP rename-from-volume conflicts
- **TMLock steal support** — clients can steal byte-range locks held by disconnected sessions
- **Block size: 1024 bytes** — matches Classic Mac OS convention

Volume sizes are reported as both 32-bit (AFP 2.x compatibility, clamped to 4 GB) and 64-bit (AFP 3.2+ clients see true values).

### Authentication Methods (UAMs)

Three user authentication methods are advertised and supported:

| UAM | Description |
|---|---|
| **Guest** (`No User Authent`) | Unauthenticated access — a Guest account is auto-created on first startup |
| **Cleartxt passwrd** | Plaintext password exchange over the TCP connection |
| **DHCAST128** | Diffie-Hellman key exchange with CAST encryption for secure password transmission |

The server maintains its own user database (not integrated with Haiku system accounts) stored in `~/.settings/` with schema versioning and migration support. User flags include enabled, admin, must-change-password, don't-display, and can-change-password.

## CodeWarrior Development Support

MacFile includes **built-in features specifically designed for developers using Metrowerks CodeWarrior** on Classic Mac OS. When a file is created or enumerated, the server automatically assigns its Mac **file type** and **creator code** based on the file extension, using a lookup table in `finder_info.cpp`. This ensures files appear correctly in the Finder with proper icons and double-click behavior.

### CodeWarrior File Extension Mapping

| Extension | File Type | Creator | Purpose |
|---|---|---|---|
| `.p`, `.cp` | `TEXT` | `CWIE` | CodeWarrior IDE source file |
| `.cpp`, `.c` | `TEXT` | `CWIE` | C++ / C source file (CodeWarrior) |
| `.h`, `.hpp` | `TEXT` | `CWIE` | Header file |
| `.pch`, `.pch++` | `TEXT` | `CWIE` | Precompiled header |
| `.prj` | `MMPr` | `CWIE` | CodeWarrior Professional 1 project |
| `.mcp` | `MMPr` | `CWIE` | CodeWarrior Professional 4+ project |
| `.cwlib` | `MPLF` | `CWIE` | CodeWarrior library file |
| `.ppob` | `rsrc` | `MWC2` | PowerPlant object file |
| `.err` | `MMCH` | `CWIE` | Error list window |

### Additional Extension Mappings

| Extension | File Type | Creator | Purpose |
|---|---|---|---|
| `.txt` | `TEXT` | `ttxt` | Plain text file |
| `.zip` | `ZIP ` | `SITx` | ZIP archive (StuffIt) |
| `.sit` | `SIT5` | `SIT!` | StuffIt archive |
| `.hqx` | `TEXT` | `SITx` | BinHex archive |
| `.bin` | `BINA` | `SITx` | Binary archive (StuffIt) |
| `.img`, `.image` | `dimg` | `ddsk` | Disk image |
| `.o` | `OBJ ` | `MPS ` | MPW library object |

Files with unrecognized extensions receive a default type/creator of `"???? "` / `"????"`.

### Resource Fork Emulation

Haiku has no native resource fork support. MacFile emulates AFP resource forks by storing the entire fork payload in an **extended attribute** (`Afp_Resource`) on each file. When a file is opened, the full resource fork is read into memory (`BMallocIO*`), and written back on close or flush. This provides:

- Full read/write access to resource fork data for CodeWarrior project builds
- Automatic Finder Info block assignment based on extension (see table above)
- Resource fork open-bit tracking per volume so Mac clients see correct file attributes

## Special `.res` File Handling

Files with the **`.res` extension** receive special treatment optimized for **source control workflows**:

1. **Data-fork-as-resource-fork**: The entire resource fork content is stored in the file's **data fork** instead of an extended attribute (`Afp_Resource`). This makes `.res` files appear as regular binary blobs on non-Mac systems (Git, SVN, etc.) — no hidden xattrs to confuse version control.

2. **Standard disk I/O**: Resource fork reads and writes go through normal `BFile` I/O on the data fork, with no in-memory caching or dirty-flagged writes. The server simply redirects AFP resource fork operations to the file's data fork (e.g., `FPRead` on the rsrc fork reads from the data fork).

3. **No metadata leakage**: Since the resource data is just the file's content and there are no special extended attributes, `.res` files contain zero Mac-specific metadata beyond the Finder Info block (type/creator) stored separately.

## Key Features

### Session Management
- **Thread-per-connection** model: each TCP client spawns a dedicated server thread
- **Session reconnect**: AFP 3.3+ clients can reconnect after a crash; the server transfers open volumes, files, and desktop references to the new session via session tokens and client IDs
- **Replay cache**: 32-entry reply cache for AFP 3.3+ enables automatic replay of unacknowledged responses after reconnection
- **Dead session detection**: 120-second timeout with periodic tickle-based liveness checks

### File Operations
- All standard AFP file/directory commands: create, delete, open, close, read, write, flush, rename, move, copy (with attribute preservation)
- **Byte-range locking** (`FPByteRangeLock`): per-session range locks on open file forks with steal support for disconnected sessions
- **Directory enumeration**: supports `afpEnumerate`, `afpEnumerateExt`, and `afpEnumerateExt2` formats
- **Desktop database**: icons, comments, and application associations stored per-volume in `.afpdesktop.db`

### Access Control
- Per-file ACLs mapped to POSIX mode bits: Owner/User/Guest with Search/Read/Write (12 permission bits)
- Default privileges inherited from parent directory on new file creation
- Read-only volume flag support

### Volume Monitoring
- **B_NODE_MONITOR** integration: if a share point is moved, renamed, or deleted on disk, the server automatically stops sharing it and logs the event
- **Dirty/clean tracking**: volumes report changes to connected clients for efficient update notifications

### Desktop Operations
- Icon storage (up to 1024 bytes per entry)
- File/folder comments
- Application type associations (`FPAddAPPL` / `FPGetAPPL`)
- 64-entry in-memory cache with on-disk persistence

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

### Build

```bash
cd afpserver && make          # Release build
cd afpserver && ./dbgbuild.sh # Debug build (enables DBGWRITE logging)
```

## Distribution

- **afp_server** — Core AFP daemon (`afpserver/`)
- **MacFile** — GUI configuration app (`afp_config/`)
- **CreateAfpShare** — CLI share creation utility (`afp_createshare/`)
- **share_volume** — Volume sharing utility with UAM support (`ShareVolume/`)

## License

MIT License — see [LICENSE](../LICENSE)
