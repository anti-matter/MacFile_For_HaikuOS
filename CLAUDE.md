# MacFile for Haiku OS

## Project Overview

MacFile is an **Apple Filing Protocol (AFP) file server** that runs on the **Haiku operating system**, enabling vintage Macintosh clients (MacOS 8.0 through MacOS X 10.5, i.e., AFP 2.2 and later) to connect over TCP/IP for file sharing. The server has been in development since at least 2004, is currently at version **1.8.6**, and is licensed under the **MIT License** by Michael J. Conrad.

A sibling project exists for Windows: [Windows_AFP_Server](https://github.com/anti-matter/Windows_AFP_Server).

## Repository Structure

```
MacFile_For_HaikuOS/
├── afpserver/              # Core AFP server daemon (afp_server) -- PRIMARY PROJECT
│   ├── afp_sources/        # 30+ C++ source files implementing the AFP protocol
│   ├── makefile            # Haiku Generic Makefile v2.2
│   ├── afp_server.rsrc     # Haiku resource file
│   ├── Resource.rdef       # Resource definitions
│   ├── dbgbuild.sh         # Debug build helper
│   └── .genio              # GenIO IDE project file
├── afp_config/             # GUI configuration application (MacFile)
│   ├── afpconfig_sources/  # 18 source files for the UI and config logic
│   └── makefile
├── afp_createshare/        # Utility to create AFP shares from CLI
├── ShareVolume/            # Small utility (share_volume) with ShareUAM.cpp
├── deps/openssl/           # Bundled OpenSSL 1.1.x (precompiled .so + headers)
├── distribution/           # Packaging: install-macfile.sh, ReadMe!
├── ref/                    # Reference: afp3XX.pdf (AFP 3.x protocol spec, 248 pages)
└── build_macfile.sh        # Top-level build/packaging script
```

## Build System

- **Haiku Generic Makefile v2.2** -- each component directory has its own `makefile` that includes `$(BUILDHOME)/etc/makefile-engine`.
- **No CMake, Meson, or cross-platform build system.** This is Haiku-only.
- To build the server: `cd afpserver && make`
- Debug build: `cd afpserver && ./dbgbuild.sh` (sets `DEBUG` and `DBGR=TRUE`)
- Top-level `build_macfile.sh` orchestrates building all components and packaging into a release ZIP for x86 or x86_64.

### afp_server Binary Dependencies

Links against: `be`, `network`, `textencoding`, `ssl111v`, `crypto111v`, and standard C++ libs. OpenSSL libraries come from `../deps/openssl/lib`.

---

## AFP Server Architecture (`afpserver/`)

The server is a Haiku `BApplication` that listens on **TCP port 548** for AFP connections. All source lives under `afp_sources/`.

### Entry Point & Application Lifecycle

- **`afpServerApplication.h/cpp`** -- The main `BApplication` subclass (`afpServerApplication`).
  - Constructor calls `afpInitializeServerNetworking()` to start the TCP listener on port 548.
  - `ReadyToRun()` reads the logon message from `~/.settings/afpLogonMessage`, verifies/creates the user database, ensures a Guest account exists, and calls `ShareAllVolumes()` to restore configured shares.
  - `MessageReceived(BMessage*)` handles all inter-process commands from the config app (`MacFile`) and other utilities. Commands are identified by four-character codes defined in `commands.h`:
    - **Versioning:** `vers`, `cver`
    - **Messages:** `send`, `updt`
    - **Volume management:** `adds` (add share), `rems` (remove share), `gvol`/`gvnm` (get name/path), `sflg`/`gflg` (set/get flags)
    - **User management:** `addu`, `delu`, `getu`, `geti`, `updu`
    - **Statistics:** `gbps`, `gdsi`, `gusr`, `grcv`, `gsnt`
    - **Hostname:** `ghst`, `shst`
    - **System:** `B_NODE_MONITOR` (watch for share point moves/deletes), `B_QUIT_REQUESTED`

### Global Objects

Three global objects coordinate the server:

1. **`gAFPSessionMgr`** (`dsi_scavenger*`) -- Tracks all active client sessions, manages attention messages, and provides session lookup by ID or token.
2. **`gAFPStats`** (`dsi_stats`) -- Collects network statistics (bytes sent/received, packets processed, throughput).
3. **`volume_blist`** (`std::unique_ptr<BList>`) -- The list of all currently shared `fp_volume` objects.

---

### Layer 1: DSI (Data Stream Interface) Network Layer

The DSI layer handles the raw TCP transport and DSI protocol framing that wraps every AFP exchange.

- **`dsi_network.cpp/h`** -- Creates the listening socket on port 548, accepts incoming connections, and spawns a `ServerConnection` thread per client. Exports `afpInitializeServerNetworking()` and `afpSrvrConnectThread()`.
- **`dsi_connection.cpp/h`** -- One `dsi_connection` instance per TCP client socket. Responsibilities:
  - Owns a `std::unique_ptr<afp_session>` for the AFP-level session state.
  - `Receive()` reads raw bytes from the socket into `mReceiveBuffer`.
  - `ProcessReceivedBytes()` parses DSI headers (16-byte header: flags, command, request ID, error code, data length) and dispatches to the AFP command layer.
  - DSI commands: `DSI_CMD_CloseSession` (1), `DSI_CMD_Command` (2), `DSI_CMD_GetStatus` (3), `DSI_CMD_OpenSession` (4), `DSI_CMD_Tickle` (5), `DSI_CMD_Write` (6), `DSI_CMD_Attention` (8).
  - `dsi_OpenSession()` negotiates session parameters including request quantum size and replay cache size.
  - `SendAttention()` / `SendTickle()` for keepalive and server-to-client notifications.
  - Buffer sizes: `RECV_BUFFER_SIZE` = 65535, `SEND_BUFFER_SIZE` = 65535, overflow = 4096.
- **`dsi_scavenger.cpp/h`** -- Background thread that monitors all tracked connections:
  - Sends tickles every 15 seconds (`SEND_TICKLE_INTERVAL`) if the server hasn't recently communicated.
  - Kills dead sessions after 120 seconds (`SESSION_DEAD_INTERVAL`).
  - Cleans up sleeping clients after 24 hours (`SESSION_SLEEPING_INTERVAL`).
  - `TrackConnection()` / `StopTracking()` manage the connection list.
  - `SendGlobalAttention()` broadcasts attention messages to all clients.
- **`dsi_stats.cpp/h`** -- Collects and reports network statistics (bytes sent/received, packets processed, bytes/second throughput).

### Layer 2: AFP Session Management

- **`afp_session.cpp/h`** -- One `afp_session` per authenticated (or unauthenticated) client connection.
  - Tracks open volumes (`mOpenVolumes`, max 16), open files/forks (`mOpenFiles`, max 24), and open desktop refs (`mOpenDesks`, max 24).
  - Manages file fork items (`OPEN_FORK_ITEM`) which hold `BFile*`, `BEntry*`, fork type (data vs resource), reference numbers, and range-lock lists.
  - Desktop items (`OPEN_DESK_ITEM`) for AFP desktop operations (icons, comments, APPL mappings).
  - Authentication state: `mIsAuthenticated`, `mUAMLoginType`, `mAFPVersion`.
  - Reconnect support: client ID (`mID`), session token (`mToken`), AFP timestamp (`mAFPTimeStamp`).
  - Extended login blob for DHCAST128 UAM state between `FPLogin` and `FPContLogin`.
  - Thread-safe via `BLocker mLock`.

### Layer 3: AFP Command Dispatch

- **`afp.cpp`** -- The command dispatcher. Contains the `afpTable[]` array that maps each AFP command code (1-79, plus special codes 122 and 192) to its handler function.
  - `FPDispatchCommand()` checks authentication state:
    - **Unauthenticated:** only `FPGetSrvrInfo`, `FPLogin`, `FPContLogin`, and `FPChangePswd` (if temporarily authenticated with expired password).
    - **Authenticated:** full command set via `afpTable[]` lookup.
  - `FPGetSrvrInfo()` -- The only AFP API callable without authentication (alongside login). Returns server capabilities, hostname, supported AFP versions (2.2, 3.0, 3.1, 3.2, 3.3), and supported UAMs.
  - `FPGetSessionToken()` / `FPDisconnectOldSession()` -- Reconnect support for crash recovery.
  - **Volume commands:** `FPOpenVol`, `FPCloseVol`, `FPGetVolParms`, `FPGetSrvrParms`.
  - **File operations:** `FPCreateFile`, `FPDelete`, `FPOpenFork`, `FPCloseFork`, `FPRead`, `FPWrite`, `FPFlush`, `FPFlushFork`, `FPGetForkParms`, `FPSetForkParms`.
  - **Directory operations:** `FPCreateDir`, `FPEnumerate` (handles `afpEnumerate`, `afpEnumerateExt`, `afpEnumerateExt2`).
  - **Metadata:** `FPGetFileDirParms`, `FPSetFileDirParms`, `FPMapID`, `FPMapName`, `FPResolveID`.
  - **Move/Rename:** `FPMoveAndRename`, `FPRename`.
  - **Copy:** `FPCopyFile` (with `CopyAttrs` and `CopyFile` helpers in `fp_objects`).
  - **Range locks:** `FPByteRangeLock` via `fp_rangelock`.
  - **Desktop ops:** `FPOpenDT`, `FPCloseDT`, `FPAddIcon`, `FPGetIcon`, `FPGetIconInfo`, `FPAddComment`, `FPGetComment`, `FPRemoveComment`, `FPAddAPPL`, `FPGetAPPL`, `FPRemoveAPPL`.
  - **Extended attributes (AFP 3.2+):** `FPGetExtAttribute`, `FPSetExtAttribute`, `FPRemoveExtAttribute`, `FPListExtAttribute`.
  - **Sync (AFP 3.2+):** `FPSyncDir`, `FPSyncFork`.
  - **Sleep (AFP 3.1):** `FPZzzz` -- client sleep notification.
  - Many commands are marked `FPUnimplemented` and return `afpCallNotSupported` (-5024).

### Layer 4: AFP Protocol Constants & Types (`afp.h`)

This is the master header defining the AFP protocol:

- **AFP Error codes:** `-5000` to `-5063` (e.g., `afpAccessDenied` = -5000, `afpParmErr` = -5019, `afpObjectNotFound` = -5018).
- **AFP versions supported:** 2.2, 3.0, 3.1, 3.2, 3.3 (enum: `afpVersion22` through `afpVersion33`).
- **UAMs (User Authentication Methods):** Guest (`UAM_NONE_STR` = "No User Authent"), Clear Text (`UAM_CLEAR_TEXT` = "Cleartxt passwrd"), DHCAST128 (`UAM_DHCAST128` = "DHCAST128").
- **Command codes:** Enumerated 1-79 with `afpLastFunc = afpSyncFork` (79).
- **Volume attribute bits:** Read-only, Unicode names, extended attributes, ACLs, case sensitivity, etc.
- **File/Directory bitmap flags:** For requesting specific parameter fields in get/set operations.
- **Access rights bits:** Owner/User/Guest with Search/Read/Write permissions (12 permission bits total).
- **FINDER_INFO struct:** 32-byte Mac metadata (4-byte type, 4-byte creator, 2-byte flags, 4-byte location point, 2-byte folder reserved, 16-byte finder data).
- **FILEPARMS / DIRPARMS structs:** File and directory parameter structures for AFP responses.
- **Constants:** `MAX_AFP_FILES_OPEN` (24), `MAX_AFP_OPEN_DIRS` (24), `MAX_AFP_OPEN_VOLUMES` (16), time delta (`AFP_TIME_DELTA` = 946684800, Jan 1 2000 epoch offset).

### Layer 5: Volumes

- **`fp_volume.cpp/h`** -- Represents a shared directory as an AFP volume.
  - Created from a `BPath*`; stores volume name, ID, flags, root dir ID, and parent-of-root ID.
  - Tracks open files on the volume for fork-open bit management.
  - `fp_GetVolParms()` populates volume parameter responses based on bitmap requests.
  - Dirty/clean tracking (`mIsDirty`) for change notification to clients.
- **`afpvolume.cpp/h`** -- Volume persistence and lifecycle.
  - `VolumeStorageData` struct (version, path, flags) stored in preferences.
  - `SaveVolumeData()`, `GetVolumeData()`, `RemoveVolumeData()` for preference I/O.
  - `StartSharingVolume()`, `StopSharingVolume()`, `ShareAllVolumes()` for volume lifecycle.
  - `FindVolume()` by ID, name, or node_ref.
  - `WatchVolume()` / `StopWatchingVolume()` -- B_NODE_MONITOR integration so that if a share point is moved/renamed/deleted, the server automatically stops sharing it.

### Layer 6: File System Objects

- **`fp_objects.cpp/h`** -- Bridges AFP concepts to Haiku's BFilesystem (BEntry, BDirectory, BNode).
  - **AFP metadata stored as extended attributes:**
    - `Afp_FinderInfo` -- 32-byte Finder Info block
    - `Afp_Resource` -- Resource fork data (Haiku doesn't have native resource forks)
    - `Afp_Attributes` -- AFP file attribute bits
    - `Afp_Longname` -- Unicode name for files whose name exceeds AFP 2.x 31-character limit
    - `Afp_Comment` -- File/folder comments
  - `SetAFPEntry()` -- Resolve a dirID + pathname to a `BEntry`.
  - `GetEntryFromFileId()` -- Look up entry by numeric file ID.
  - `CreateLongName()` -- Store Unicode long name in extended attribute.
  - `fp_GetDirParms()` / `fp_GetFileParms()` -- Populate AFP parameter responses from filesystem metadata.
  - `fp_SetFileDirParms()` -- Apply AFP parameter changes to filesystem.
  - **Permission mapping:** AFP permissions (Owner/User/Guest x Search/Read/Write) are mapped to POSIX mode_t bits (`S_IRUSR`, `S_IWGRP`, `S_IXOTH`, etc.).
  - `CopyAttrs()` / `CopyFile()` -- Used by `FPCopyFile` to duplicate extended attributes and file content.

- **`fp_rangelock.cpp/h`** -- Byte-range locking for concurrent file access.
  - Each lock is associated with an `OPEN_FORK_ITEM`.
  - `RangeLocked()` checks if any existing lock overlaps a given range.
  - Used by `FPByteRangeLock`, and checked before read/write operations.

### Layer 7: Authentication & Users

- **`afpuser.cpp/h`** -- Login/logout flow.
  - `FPLogin()` -- Parse login request, validate UAM type, start authentication.
  - `FPContLogin()` -- Continue login with password.
  - `FPLogout()` -- Clean up session state.
  - `FPChangePswd()` -- Password change.
  - `afpGetUAMType()` / `afpGetAFPVersion()` -- Parse UAM/version strings from client.

- **`afplogon.cpp/h`** -- User database operations.
  - `AFP_USER_DATA` struct: username (65 bytes), password (65 bytes), group ID, flags, user ID.
  - User flags: `kUserEnabled` (0x01), `kDontDisplay` (0x02), `kMustChngPswd` (0x04), `kIsAdmin` (0x08), `kTempAuthenticated` (0x10), `kCanChngPswd` (0x20).
  - User database stored in `~/.settings/` with schema versioning (`AFP_USERDB_VERSION`).
  - `afpImpLogonUser()` -- Validate credentials.
  - `afpSaveNewUser()`, `afpDeleteUser()`, `afpUpdateUserInfo()` -- CRUD operations.
  - `afpVerifyUserDatabase()` -- Schema migration check on startup.

- **`afpdhxlogin.cpp/h`** -- DHCAST128 authentication implementation.
  - Uses OpenSSL: `DH` (Diffie-Hellman) for key exchange, `CAST` for password encryption.
  - `DHXLogin()` -- Initiate DH exchange, generate random buffer.
  - `DHXLoginContinue()` -- Complete exchange, decrypt and verify password.
  - `DHXChangePassword()` -- Encrypted password change.
  - `DHXInfo` blob stored in session between login and continue calls.

### Layer 8: Access Control

- **`afpaccess.cpp/h`** -- Per-operation access checks.
  - `afpAccessCheck()` -- Generic check with access type enum (`afpAccessRead`, `afpAccessWrite`, `afpAccessSearch`).
  - `afpCheckReadAccess()`, `afpCheckWriteAccess()`, `afpCheckSearchAccess()` -- Convenience wrappers.
  - Checks user role (Guest/User/Owner) against AFP permission bits stored on the entry.

### Layer 9: Desktop Operations

- **`afpdesk.cpp/h`** -- AFP desktop database for icons, comments, and application associations.
  - Stored in `.afpdesktop.db` file within each shared volume.
  - `DESKTOP_ENTRY` struct holds entry type (icon/APPL/comment), Mac type/creator codes, path, and icon data (up to 1024 bytes).
  - Entry types: `ENTRY_TYPE_EMPTY` (0), `ENTRY_TYPE_ICON` (1), `ENTRY_TYPE_APPL` (2), `ENTRY_TYPE_CMNT` (3), `ENTRY_TYPE_RESERVED` (4).
  - Cache of 64 desktop entries (`NUM_DESK_ENTRIES_TO_CACHE`).
  - Operations: `FPOpenDT`, `FPCloseDT`, `FPAddIcon`, `FPGetIcon`, `FPGetIconInfo`, `FPAddComment`, `FPGetComment`, `FPRemoveComment`, `FPAddAPPL`, `FPGetAPPL`, `FPRemoveAPPL`.
  - Internal helpers: `afp_FindDTEntry()`, `afp_AddEntry()`, `afp_RemoveEntry()`.

### Layer 10: Extended Attributes (AFP 3.2+)

- **`afpextattr.cpp/h`** -- Get/Set/List/Remove extended attributes on files and directories.
  - Flags: `kXAttrNoFollow` (0x1), `kXAttrCreate` (0x2), `kXAttrReplace` (0x4).

### Support Modules

- **`afp_buffer.cpp/h`** -- Binary buffer class for parsing AFP request packets and building response packets.
  - Template-based `push_num<T>()` / `pull_num<T>()` with automatic big-endian byte swapping for multi-byte types.
  - Pascal string handling (`AddCStringAsPascal`, `GetPascalString`).
  - Unicode string encoding/decoding (`AddUniString`, `GetUnicodeString`, `GetUniString`).
  - Path character conversion: `/` is replaced with `π` (0xb6) since AFP uses `:` as path delimiter.
  - `GetDataLength()` tracks how many bytes have been written/read.

- **`byte_swap.cpp/h`** -- Byte swap utilities for big-endian network byte order.
  - `_byteswap_ulong()`, `_byteswap_ushort()`, `_byteswap_uint64()`.
  - Template `byte_swap<T>()` dispatches by size (1, 2, 4, 8 bytes).

- **`afpreplay.cpp/h`** -- Replay cache for AFP 3.3+.
  - `AFPReplayCacheItem` stores request ID, reply size, and reply data.
  - Cache size: 32 entries (`AFP_REPLAY_CACHE_SIZE`).
  - `AFPReplayAddReply()`, `AFPReplaySearchForReply()`, `AFPReplayEmptyCache()`.

- **`afphostname.cpp/h`** -- AFP server hostname management (the name Mac clients see, NOT the system hostname).
  - Stored in `~/.settings/afpHostname`.
  - Falls back to network settings if not set.
  - Max length: 128 characters (`MAX_HOSTNAME_LEN`).

- **`afpmsg.cpp/h`** -- Server messages (logon message and attention messages).
  - `afp_SetLogonMessage()` / `afp_SetServerMessage()` -- Set message text.
  - `FPGetServerMessage()` -- Handle AFP get server message command.
  - Max message length: 199 characters (`AFP_MSG_MAXLEN`).

- **`debug.cpp/h`** -- Conditional debug logging infrastructure.
  - Levels: `dbg_level_error` (1), `dbg_level_warning` (2), `dbg_level_info` (3), `dbg_level_trace` (4), `dbg_level_dump_in` (5), `dbg_level_dump_out` (6).
  - Current default level: `dbg_level_trace`.
  - `DBGWRITE(level, format, ...)` -- Conditional log macro (compiled out when `DEBUG` is not defined).
  - `dump_bitmap()` / `hex_dump()` -- Debug helpers for bitmaps and binary data.
  - `afpGlobals.h` also defines `DPRINT()` (raw printf in debug mode), `BEGIN_PERF_MEASURE()`/`END_PERF_MEASURE()`, and `GET_BERR_STR()` for Haiku error string conversion.

---

## Config Application (`afp_config/`)

The **MacFile** GUI application for managing the AFP server. 18 source files in `afpconfig_sources/`:

- **`main.cpp`** -- Entry point, creates `afpConfigApplication`.
- **`afpConfigApplication.cpp/h`** -- `BApplication` subclass for the config UI.
- **`afpMainWindow.cpp/h`** -- Main configuration window with tabs/panels for shares, users, and settings. Communicates with `afp_server` via `BMessage` IPC using the command codes from `commands.h`.
- **`afpAboutWindow.cpp/h`** -- About dialog.
- **`afpMsgWindow.cpp/h`** -- Server message sending window.
- **`afpSetHostWin.cpp/h`** -- Hostname configuration window.
- **`afpUserConfig.cpp/h`** -- User account management UI.
- **`afpLaunch.cpp/h`** -- Server launch/stop control.
- **`afpConfigUtils.cpp/h`** -- Shared utility functions for the config app.
- **`SG_URLView.cpp/h`** -- Custom URL-view widget.

---

## Utilities

- **`afp_createshare/`** -- `CreateAfpShare` CLI tool to create AFP shares programmatically.
- **`ShareVolume/`** -- `share_volume` utility with `ShareUAM.cpp` for volume sharing operations.

---

## Key Design Decisions & Patterns

1. **Resource fork emulation:** Haiku has no native resource fork support. Resource fork data is stored in the `Afp_Resource` extended attribute on each file. The entire resource fork is read into memory (`BMallocIO* rsrcIO`) when a file is opened, and written back on close/flush. This does not scale well for large resource forks.

2. **Big-endian wire format:** AFP uses network byte order (big-endian). All multi-byte integers in packets are byte-swapped via `afp_buffer` template methods and `byte_swap.h`.

3. **Thread-per-connection model:** Each TCP connection spawns a `ServerConnection` thread that owns a `dsi_connection` object. The scavenger thread monitors all connections independently.

4. **Message-based IPC:** The config app communicates with the running server via Haiku `BMessage` IPC. All commands use four-character codes from `commands.h`.

5. **Path encoding:** AFP uses `:` as path delimiter (Mac convention). The `/` character is illegal in AFP names and is replaced with `π` (0xb6) during transmission.

6. **Debug build:** Compile with `DEBUG` defined to enable `DBGWRITE()` logging, `DPRINT()`, hex dumps, and error string conversion. Release builds compile all debug code to no-ops.

7. **User database:** Self-managed user database in `~/.settings/` with schema versioning for migration. Not integrated with Haiku's system authentication.

8. **Reconnect support:** Session tokens and client IDs allow Mac clients to reconnect after a crash or network interruption, transferring open resources from the old session to the new one.
