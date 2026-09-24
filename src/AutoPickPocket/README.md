# AutoPickPocket — universal 12340 x86 loader integration

Development branch: `feature/autopickpocket-12340`. Canonical universal updater and external game-thread loader are inherited from current `work`. Do NOT install the retired `src/Loader12340/Wow335Loader.dll` or a second updater.

Native `AutoPickPocket335.dll` exports `W335_MessageId`, `W335_HookProc` and `W335_CallWndProc` and is loaded by the same `src/Loader/loader_win32.c` manifest chain as AutoLoot. It has independent stealth, known/usable spell 921, cooldown, native undead/humanoid type (6/7), GUID cast, world token and correlated result policies. No target switching or clearing, movement hooks, extra auto-loot or chat messages. Logs go to `.wow335_debug/AutoPickPocket.jsonl`.

The one-time `activate_autopickpocket.yml` gate on the feature branch builds a genuine native PE32 x86 PP DLL with the pinned MSVC toolchain, audits exports and exact client ABI, registers the exact binary and module ownership, and validates the full active AutoLoot+PP rebuild before publishing a new branch commit. Only the normal candidate workflow on that registered commit may publish the game package with `FINAL_PACKAGE: PASS`. The universal updater itself is still built only from `work`; select the AutoPickPocket feature branch in its game-branch selector to install the two-DLL candidate.

Do not present compilation as in-game proof. The first verified game test must confirm that startup, both hooks, actual stealth Pick Pocket on nearby undead/humanoid NPCs, retry/results and AutoLoot coexist without freezing or crashes. No game package is ready if activation, rebuild or final package gate fails.

## Crash #5: Lua callback removed (1.0.1 TEST)

The exact Wow.exe is pinned and unchanged. WoW Error #134 names `PP335_LootOpened` and an invalid function pointer inside `AutoPickPocket335.dll`. The old policy registered a DLL C function in the client's Lua VM. The new policy never registers a DLL callback from Lua: a normal Lua frame records nonce-scoped `LOOT_OPENED`, failure and spell-921 server cast events. A native loot-source GUID, if still available before the game's automatic looting closes the window, corroborates the exact target. An independently GUID-correlated server cast success is sufficient to mark the *cast* complete after a short grace period, not proof that loot reached inventory. Without matching evidence the existing bounded timeout/retry logic applies.

The registered DLL must be refreshed with a genuine MSVC PE32 x86 rebuild and the manifest SHA256 updated together on this feature branch. A source-only commit will fail the strict game-package gate; install only the subsequent exact-HEAD `FINAL_PACKAGE: PASS` candidate through the existing universal updater. Do not reuse the previously crashed 1.0.0 DLL; keep `work` (AutoLoot-only) as the rollback. The user must test actual Pick Pocket, stable startup, no Lua callback crash, native loot and coexistence with AutoLoot in-game.

## Target-independent GUID queue (1.0.4 TEST)

AutoPickPocket never calls `ClearTarget`, `TargetUnit`, or queries the
player's currently selected target. No target is selected or released.
The user's target is independent of automatic GUID-based Pick Pocket.
The Lua event observer records only nonce-scoped spell and loot results;
it does not act upon targets, movement, loot or chat.

The core stores a bounded 64-NPC snapshot of nearby GUIDs. Once a GUID
is submitted it is consumed from the snapshot and subsequent eligible
NPCs can be tried without repeating a full object-manager walk. The
snapshot expires after 1200 ms, on exhaustion, disable or world reset.
**Every native cast** still re-reads the selected GUID, NPC type,
health, spell usability, and player/NPC positions. One native cast
submission at most per loader pulse; failed or timed-out GUIDs are
temporarily blocked. No movement, position spoofing or AutoLoot changes.

The structured loader report includes `scan_ms` (last full scan time),
`scan_candidates`, `queue_depth`, `queue_age_ms`, `pulse_gap_ms`,
`cast_gap_ms` (previous cast to current cast), `result_wait_ms`
(submission to matched result) and `next_wait_ms` (last matched result
to next cast). Zeros can mean same-millisecond completion or missing
previous measurement; no chat messages. A successful cast ACK is not
proof of receiving gold or items.

## Rapid scan and server-range resilience (next TEST candidate)

The currently shared `work` loader pulses feature DLLs about every 40 ms; lowering a
module-only timer cannot exceed this external scheduling bound. The PP core now
accepts a new scan every 20 ms (thus on each delivered loader pulse) and
immediately examines a different GUID on a result, range rejection or timeout.
A guarded, manager+GUID-validated player pointer cache avoids rescanning the
entire object list merely to locate the player on every pulse. NPCs are still
classified and range-checked again before casting, and the cache is discarded
when the world changes, is unavailable or the module is explicitly reset.

The candidate uses a conservative 4.0-yard **geometric** reach instead of 5.0
and recognizes the game's `SPELL_FAILED_OUT_OF_RANGE` /`ERR_OUT_OF_RANGE`
result for the active attempt. This reduces borderline attempts without moving
the character; server range/latency can still reject a moving or obstructed
NPC. Failures back off the rejected GUID while other eligible NPCs continue.

On an exact-GUID server `SPELL_CAST_SUCCESS` event the Lua observer only
records the result. The core's independent GUID/nonce confirmation retains
an 80 ms error-grace interval; a cast ACK does not guarantee that gold or
items reached inventory. The user-selected target is never changed.
No movement, position spoofing, forced NPC target selection or AutoLoot changes.
Confirm actual cast cadence, target release, false positives and CPU stability
in-game before considering any wider deployment.

## Moving fast: continuous look-ahead scan (1.0.5 TEST)

The previous 1.0.4 implementation **stopped discovering new NPCs while a
Pick Pocket result was pending** and could keep a nearby-GUID snapshot for
1200 ms. This specifically misses NPCs passed during Sprint/Stealth.
The updated core discovers NPCs even while the prior result is pending or
while the server reports a spell cooldown. A bounded object-manager scan
refreshes every 60 ms of in-game pulse time (approximately every other
40 ms external loader pulse, subject to frame scheduling and scan cost).
NPCs are detected up to 9 yards in advance, but only marked ready at a
verified geometric range of at most 4 yards; the native GUID cast bridge
**rechecks** player and target positions, type, health and spell usability
immediately before submission. Prefetched NPCs are never cast on outside
validated range. A scan snapshot expires within 160 ms, and all active
histories remain GUID-specific. There is at most one native cast submitted
per pulse and one outstanding spell result; the bounded unanswered-result
window is 900 ms rather than 1500 ms, with nonce-correlation and per-GUID
backoff unchanged. A delayed server result must never count as success of
another GUID. Do not speed up by assuming cast submission means success.

This removes local scanning/queue stalls; it cannot remove server GCD,
spell cooldown, packet latency or the requirement to be physically within
Pick Pocket range while Sprinting. No movement spoofing, target
selection/clearing, mouseover emulation, additional launcher, AutoLoot
modifications, or in-game chat messages. The first in-game test should
compare `scan_ms`, `pulse_gap_ms`, `cast_gap_ms`,
`result_wait_ms`, `next_wait_ms`, and `not_castable` events during
Stealth+Sprint. In particular a repeating ~3 s gap with no casts while
`not_castable` is emitted indicates spell/server readiness rather than
unvisited NPCs; the updater report contains the structured evidence.

## 1.0.6 TEST: coherent attempt release across the two timeout layers

A user report on 1.0.5 showed repeated ~900 ms result timeouts and
~1-3 s cast gaps. The native policy previously retained its own live
attempt for 1500 ms, even after the engine had timed out at 900 ms.
Worse, its timer started *after* the engine clock sample, so setting
both guards to 900 ms alone would retain an occasional rejected next
cast. The engine now explicitly signals `end_attempt(GUID,nonce)` on
its result timeout, failed submission, enable/disable and world reset.
The native policy clears only the matching in-flight GUID and nonce,
never a newer attempt or other NPC. Both guards also share the same
`PP_RESULT_TIMEOUT_MS` bound. Before immediately casting a different
GUID in the same pulse, the previous local observer arm is released.
This does not treat an unconfirmed spell as a success, force cooldown,
change target/movement, or modify AutoLoot; the server still controls
real spell availability. Validation requires a new exact x86 binary,
finalized TEST candidate and a user's in-game report.

## 1.0.7 TEST: same-pulse local range failover (Sprint phase 1)

A pre-submission geometric range miss is distinct from a server-rejected
packet. The adapter returns PP_CAST_LOCAL_RANGE without sending or treating
that GUID as a server retry. The decision engine releases the exact attempt,
temporarily excludes that GUID for 250 ms, and searches up to four different
nearby candidates during the *same* loader pulse. It submits at most one
actual spell cast per pulse and retains the 900 ms result timeout for casts
that were actually submitted. A server-correlated OUT_OF_RANGE still releases
the pending GUID and selects another candidate on its result pulse.

The bypass is bounded to protect the WoW game thread from an unbounded
object-manager search. It never forces server range, cooldown, stealth or
movement and never guesses an unscoped UI error's GUID. Structured logs
distinguish local_range_precheck_rejected from server out_of_range and
from an actual cast_submitted. In-game Sprint testing must compare missed
mobs, packet submission-to-result latency, pulse gap, and live transport
(packet versus native fallback) on the exact installed commit SHA.

## Sprint phase 3: bounded unknown-result handoff (isolated TEST)

The old 900ms blocking wait is replaced. At >=420ms, if a fresh
(<=60ms) native scan sees the exact pending GUID beyond cast reach and
another eligible, unblocked GUID in cast range, the engine cancels only
the old nonce and can submit to the next NPC on the same pulse. Otherwise
it releases any still-unconfirmed attempt at 560ms, blocking the old GUID
as *unknown* for 3s; neither release is reported as successful theft.
For 1.5s after an unresolved nonce, generic wallet+loot signals lacking
an exact native loot-source GUID cannot confirm a subsequent attempt.
Exact-GUID spell results remain usable. Distinct log reasons:
unconfirmed_early_release_old_guid_outside_range and
unconfirmed_result_560ms. Packet/server cooldown and actual game cadence
still require a new report from this exact TEST candidate. If missed loot or
native fallback increases, revert to the preceding in-game-tested SHA.

## Sprint phase 2 (isolated TEST, not accepted stable)

A GUID-correlated spell-921 `OUT_OF_RANGE` now blocks only that GUID for 200 ms and the engine tries another available, freshly validated GUID on the same pulse. A global UI range message **never** directly sets the GUID's `W335PP_FAIL`/`W335PP_E` or proves the server rejected that GUID. It can produce a separate `ui_range_local_distance_confirmed_not_server_guid` event only if the native 12340 scan also shows the outstanding exact GUID outside the 4-yard cast radius (yet within the 9-yard detection radius); otherwise the cast stays pending until a real result or the unchanged 900 ms timeout. On this guarded release the exact nonce is cancelled before the next target can be armed.

The scanner samples the player's XY displacement between recent pulses (20-250 ms, plausible speed; teleport/unknown/stationary ignored). Targets **ahead** in the validated movement direction are preferred over nearer targets behind; all casts still require native same-GUID <=4 yd validation. The cast log includes `selection_forward` and `candidates_ready`. Tests cover the new short GUID-specific retry, stationary fallback, moving-target selection and UI/non-GUID attribution guard. A Sprint in-game report must check fresh sessions only and identify `packet` versus `native_fallback`; the mere UI hint is not a successful theft.

## Packet-only 200ms experimental TEST (after report #19)

This user-requested A/B candidate restores the earlier Sprint phase 2
decision, result and queue logic from exact source commit 11b6927f,
replacing only the 900ms unknown-result deadline with 200ms and removing
the native cast path. The host submits Pick Pocket ONLY via the audited
SendPacket ABI: missing acknowledgement never selects an alternate
transport. A 200ms result deadline is an unverified and risky experiment:
prior in-game sessions routinely observed wallet/loot signals 250-360ms
after submission. A timed-out cast may have succeeded on the server;
the engine must never classify an unknown result as confirmed failure
or successful theft. On the next exact-SHA report compare packet-only
casts, result_timeout counts, repeated GUIDs and actual gold changes.
Do not promote this candidate to stable without in-game confirmation.

The `no_ack_count` log field is always zero in this packet-only TEST:
previous native-switch bookkeeping was removed, not evidence that the
server acknowledged all casts. Only authoritative spell/loot observations
can establish a result; a 200ms timeout is an unknown outcome.

## Packet-only nonblocking GUID burst (isolated TEST)

The old single-active-attempt scheduler blocked all new GUIDs while
waiting for the last spell result or its 200ms timeout. Normal play now
uses a nonblocking queue: one packet per loader pulse, at least 80ms
between distinct GUID submissions, up to 32 bounded outstanding GUID/nonce
result observations. A different eligible GUID can be sent without waiting
for the preceding loot, cast animation or result. The scanner remains live,
and the adapter rechecks exact GUID, native NPC type, health, position and
spell usability immediately before every packet. The native 5-argument
cast path remains removed. One explicit /appp probe uses the older isolated
single-attempt path; the registered ordinary game module defaults to burst.

Combat-log success/failure is looked up using the original GUID and nonce
in the Lua observer table for up to 1600ms after submission. Result
observation cannot stop new GUID sends. An already pending GUID never gets
a duplicate packet. If the exact GUID leaves local reach, a single
pending_guid_left_range_nonblocking diagnostic records that fact, but
does not infer server failure or terminate the independently tracked result.
A confirmed server GUID-scoped out_of_range has a 200ms local backoff;
a purely local pre-submit range miss sends NO packet and tries another
candidate in the same pulse. An unconfirmed result after 1600ms stays
UNKNOWN and is excluded from immediate resend for 2500ms; it is never
counted as successfully looted or definitively failed. Full pending slots
and server spell readiness are still natural send gates.

A global UI range message or unattributed wallet/loot notification cannot
be assigned to a particular pending GUID. Independent
wallet_delta_unattributed diagnostics record money-increase event counts
and total copper increase but make no claim as to which NPC was robbed.
Only a GUID-scoped server spell result or an exact native loot-source match
verifies a specific NPC. 0 wallet_loot_signal is therefore possible despite
real successful robberies in burst mode. Compare actual in-game currency,
server-correlated results, new pending/unknown counters, the lack of native
fallback, and the observed inter-packet cadence for one exact candidate SHA.
Do not infer in-game success from Windows x86 build or CI tests alone.
