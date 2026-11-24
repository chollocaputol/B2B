# F24Q — Named F23D SD entrypoints and cross-writer handle lease MODEL (BUILD ONLY)

**Exact required local parent:** F24P `71a8ef0ad58509956668d7c35c6fc93ba8fac85b`, branch `feature/firmware-multihop`, clean tree. **Status:** candidate; local Windows validation/commit pending. The profile is **NOT a deployable image**: inherited normal LoRa transmissions remain present if flashed. DO NOT FLASH.

## Source scope

1. `f23d_sd_file_writer.h` now routes its four **volume entrypoints** (`exists`, three `open` operations) through `F23D_SD_ACCESS`, defaulting to the same `SD` object for standalone legacy tests. In `ThesisSDLogger`, this name is `interposedSD`, which aliases `SD` in ordinary profiles. No existing F23D file format, path, append/verification sequence or journal state machine is changed. This is **not interception of the returned File handle's methods**.
2. `dtn_f24q_cross_writer_arbiter.h` is a pure C++17 **software contract** for one active SOS/legacy append/export/history/audit/F23D lane at a time. It models two simultaneous F23D handles during append/readback, rejects conflicting lanes, checks independent media/mount/owner generations on operations, denies uncoordinated paths, and quarantines handles on lease loss rather than pretending close is safe. Its positive witness is only synthetic in host tests.
3. `ThesisSDLogger::poll` compiles an `Arbiter<DenyWitness>` preview for F24Q; it always refuses to acquire physical owner rights. The F24Q profile inherits F24P/F24O/F24N/F24M/F24K permanent SD denial, disallows F23D evidence, live fleet storage, physical GPIO and legacy relay TX. It does **not** schedule a real SD operation or install a lease into the existing logger's `File` objects.
4. Host tests include an adversarial arbiter state model and the **actual** `F23DSDFileWriter` compiled twice with disposable in-memory File/SD objects: legacy `SD` route and newly named `interposedSD` route.

## Important limitations

- The existing `exportFile`, `historyFile`, startup/audit temporary Files and `F23D::file_` / `verification_` lifetimes still use raw Arduino `File` methods outside a true global owner. F24Q does **not** prove `allPathsInterposed`, exclusive physical media ownership, radio/SOS preemption during File.close, reliable SD media IDs, or power-loss durability.
- The model intentionally refuses File close after owner loss; actual existing firmware File destructors are **not** thereby made safe. A future physical adapter must explicitly account for cleanup under lost media/lease.
- The existing F24O port independently closes an acquired handle on revocation; its hardware-safe behavior is not proven by the F24Q model. Owner-integrated cleanup needs reconciliation before any grant.
- No actual physical F23D writer runs in the F24Q build profile. No SD cards (especially preserved V002 media), board USB/COM, flashing, RF relay, backend, HTTP upload, credentials, Supabase, RLS, or shore acknowledgment changes are authorized.
- Host positives do not establish physical permission. F24Q does not produce valid `CloseFacts`, an immutable source, a registered identity, or final SOS delivery credit. Ordinary LoRa neighbor ACK / HTTP 200 cannot be promoted to shore receipt.

## Acceptance before local checkpoint

- Require Windows g++ strict C++17 **F24Q cross-writer model (307 checks)**, actual F23D writer legacy-default and named-facade (20 each), **F24P (133 checks)** and **F24O seven-phase regression (266 checks)** = **746 host checks**.
- Require `C:\PlatformIO\penv\Scripts\platformio.exe run -d C:\Users\Public\Thesis\SYSTEM_multihop\firmware\universal -e tbeam-supreme-f24q-cross-writer-deny-build-only` with **no upload target**.
- Verify exact seven-file scoped diff (3 modified, 4 added), whitespace/working tree, and unchanged F24P parent before any separately reviewed guarded *local* commit. No automatic push.

**Next engineering gate (not delivered by F24Q):** actual legacy `File` adapter and F23D retained handles under a single trustworthy owner, exhaustive bypass inventory, reliable owner lease and media continuity, non-preemptive close behavior, durable session seal and fault-tested sacrificial-media qualification only with explicit physical authorization.
