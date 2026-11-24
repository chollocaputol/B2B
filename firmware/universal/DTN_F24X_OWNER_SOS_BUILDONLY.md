# F24X — owner-held SOS persistence integration (build only)

Accepted local parent F24W: `c75b1862f1dd385cc119d51dc02ec09ede2a6c15`.
Branch `feature/firmware-multihop`; V001 unavailable, multihop paused.

## Actual changes
- F24W owner now supports checked exists (status distinct from absence), size, read seek, flush and matching owner/media/epoch identity. Tokens contain no raw file. Backend failures and post-operation identity mismatches quarantine the owner.
- F24X narrow SOS port uses F24O evidence/path policy and the same F24W owner. Current session append only; old session reads require verified closure and no pending urgent work. A dropped/moved token never closes a raw resource implicitly; abandonment latches fault. Owner and witness must outlive port and handles. Single caller only.
- Existing F24M SessionWriter and F24N Replay/ReceiptWriter use an opt-in storageHealthy status hook to check existence and final close, including readback. Legacy ports without the hook keep prior behavior; their close/error semantics are NOT retroactively qualified. No second operational queue or alternate journal.
- The F24X firmware profile selects the same denied mount owner's token port for the existing writer and replay. No positive backend on target; deny witness and read/receipt denial inherited. Positive directory discovery deliberately returns failure; host tests supply a known closed-session list. Normal operational RF inherited if flashed; NOT globally RF silent. Do not upload.

## Evidence and limits
- Host stdio backend (test source only) checks exact fwrite/fread, fflush, fclose, seek and stat results. Independent process init/offline/mock-shore receipt/restart tests preserve 62-byte SOS wire records, 88-byte F24M frames and 40-byte F24N receipts. Fault injection covers missing-vs-stat failure, short write, flush after write, close/readback failures, media change, abandoned tokens, urgent read revocation, old-session write rejection and torn-frame recovery. Pending state is not acknowledged by a failed append. Fake shore evidence is not authenticated delivery.
- The packet codec and capture path are SOS-only. CANCEL is rejected; this is a separate functional gap, not an implemented or validated feature. Do not change packet identity or introduce CANCEL persistence without checking actual local protocol semantics.
- Host source reconstructed from F24P capture and accepted P-W payloads; Linux packet_protocol.h was previously obtained from GitHub main, SHA-256 5c8b5cbba14ed060035dd4457b1386fe612995d0913b40bf2a6cefea5c01525d, not independently confirmed to be the exact current local branch header. Windows validates actual repository.
- No physical backend, true media identity, global mount ownership, RTOS serialization, FAT/power-fail durability, physical teardown, discovery or end-to-end authenticated shore delivery is proven. Host stdio success is not card durability; quarantined host handles can remain open until process exit. No SD/COM/flash/relay/cloud/GitHub push authorized. Preserve original V002 card.
- Next: qualify a checked target backend and directory inventory on the exact installed core, migrate remaining logger clients or exclude them with explicit ownership, establish actual operator-bound disposable-media permission. Do not keep adding standalone synthetic owner layers.
