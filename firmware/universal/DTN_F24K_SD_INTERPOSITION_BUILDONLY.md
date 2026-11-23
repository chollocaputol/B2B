# F24K — Direct logger SD-call-site interposition (BUILD-ONLY / DENY)

**Required clean local parent:** `6524bf0ab031ea573e2f178b54b9a37cdcf4232a` on `feature/firmware-multihop`.

## Implemented

This phase *edits actual `ThesisSDLogger` source call sites*, not only a host-only model. The guarded staging script replaces all 27 observed direct `SD.begin/open/exists/mkdir/cardSize` entrypoint invocations within `firmware/shared/storage/thesis_sd_logger.h` with calls on `interposedSD`, with a pinned inventory of method counts (1 begin, 12 open, 8 exists, 5 mkdir, 1 cardSize). It refuses any added, missing or unreviewed direct SD method and verifies that none remain. The real logger's append, mount, read-only audit, history, export, startup verification and directory creation entrypoints therefore share a single named SD facade.

- **Ordinary builds (F24K=0):** `interposedSD` aliases the already existing Arduino `SD` object; previous behavior is preserved (this is not yet single-owner arbitration).
- **F24K build-only preview (F24K=1):** `interposedSD` is a `DTNF24K::DenySdFacade<File>` instance with no reference to Arduino's SD object and no path to a physical implementation; `begin/exists/mkdir/remove/rename` deny; `open` always returns an invalid `File{}`; `cardSize` returns zero. Calls count as RAM-only diagnostics. The profile inherits F24J but **MUST NOT BE FLASHED**: normal radio transmission may still be inherited.
- Previous F24B/F24F/F24G/F24H/F24I/F24J contracts and synthetic tests remain unchanged. F24K does not modify normal LoRa packet flow, relay permission, weather algorithm, SOS cloud mapping, database, or credentials.

## Not yet solved — material safety holds

1. This facade intercepts entrypoints of **this logger header only**. The existing `File` handle method calls (`read`, `write`, `print`, `close`, etc.) still directly use those handles; in deny-preview no handle can be obtained from this facade, but complete independent ownership has not been proven.
2. Other repository modules, most importantly the separately compiled F23D SD file writer and potential future direct `SD` users, have not been routed through the facade. F24K preview explicitly forbids `THESIS_F23D_SD_EVIDENCE=1`; it **cannot** claim repository-wide `allWritesInterposed=true`.
3. Ordinary builds are intentionally unchanged and retain historical SD behavior. There is NO real, exclusive physical SD-owner lock, verified card identity, FAT durability guarantee, crash-consistent retired journal rotation, immutable source, or trusted media witness. F24H–F24J modeled these concepts only with injected synthetic ports.
4. No F24F SOS replay permission, Wi-Fi scan/provisioning, HTTPS POST, backend ACK, or cloud ingestion is enabled. Original Vessel-002 historical SD journals must not be read or written.

## Acceptance process

Verify source ZIP SHA-256 and installer SHA-256, exact clean parent, fixed inventory and three new-file hashes; rerun F24B–F24J host suites plus F24K denial suite on the actual Windows repository. Stage the five-file patch without Git add/commit. Compile only `tbeam-supreme-f24k-sd-interpose-deny-build-only` with ESP32-S3 PlatformIO. Review evidence and make a separate guarded local commit only after successful native/PIO checks. No flash, physical SD, push or cloud changes.
