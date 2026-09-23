# AutoPickPocket — universal 12340 x86 loader integration

Development branch: `feature/autopickpocket-12340`. Canonical universal updater and external game-thread loader are inherited from current `work`. Do NOT install the retired `src/Loader12340/Wow335Loader.dll` or a second updater.

Native `AutoPickPocket335.dll` exports `W335_MessageId`, `W335_HookProc` and `W335_CallWndProc` and is loaded by the same `src/Loader/loader_win32.c` manifest chain as AutoLoot. It has independent stealth, known/usable spell 921, cooldown, native undead/humanoid type (6/7), GUID cast, world token and correlated result policies. No target switching, movement hooks, extra auto-loot or chat messages. Version 1.0.7 conditionally clears only the PP-cast GUID when it is still selected. Logs go to `.wow335_debug/AutoPickPocket.jsonl`.

The one-time `activate_autopickpocket.yml` gate on the feature branch builds a genuine native PE32 x86 PP DLL with the pinned MSVC toolchain, audits exports and exact client ABI, registers the exact binary and module ownership, and validates the full active AutoLoot+PP rebuild before publishing a new branch commit. Only the normal candidate workflow on that registered commit may publish the game package with `FINAL_PACKAGE: PASS`. The universal updater itself is still built only from `work`; select the AutoPickPocket feature branch in its game-branch selector to install the two-DLL candidate.

Do not present compilation as in-game proof. The first verified game test must confirm that startup, both hooks, actual stealth Pick Pocket on nearby undead/humanoid NPCs, retry/results and AutoLoot coexist without freezing or crashes. No game package is ready if activation, rebuild or final package gate fails.

## Crash #5: Lua callback removed (1.0.1 TEST)

The exact Wow.exe is pinned and unchanged. WoW Error #134 names `PP335_LootOpened` and an invalid function pointer inside `AutoPickPocket335.dll`. The old policy registered a DLL C function in the client's Lua VM. The new policy never registers a DLL callback from Lua: a normal Lua frame records nonce-scoped `LOOT_OPENED`, failure and spell-921 server cast events. A native loot-source GUID, if still available before the game's automatic looting closes the window, corroborates the exact target. An independently GUID-correlated server cast success is sufficient to mark the *cast* complete after a short grace period, not proof that loot reached inventory. Without matching evidence the existing bounded timeout/retry logic applies.

The registered DLL must be refreshed with a genuine MSVC PE32 x86 rebuild and the manifest SHA256 updated together on this feature branch. A source-only commit will fail the strict game-package gate; install only the subsequent exact-HEAD `FINAL_PACKAGE: PASS` candidate through the existing universal updater. Do not reuse the previously crashed 1.0.0 DLL; keep `work` (AutoLoot-only) as the rollback. The user must test actual Pick Pocket, stable startup, no Lua callback crash, native loot and coexistence with AutoLoot in-game.

## Target-independent GUID queue (1.0.4 TEST)

Historical 1.0.4 behavior: no target selection or clearing. This changed
in 1.0.7: the GUID cast remains target-independent, but the game-thread
post-cast callback conditionally clears a still-selected matching GUID.
A different manually selected GUID is preserved. The Lua event observer
still records nonce-scoped spell/loot results and may also clear an exact
matching target on the corresponding spell-success event.

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

On an exact-GUID server `SPELL_CAST_SUCCESS` event the Lua observer records
the result and conditionally clears only the PP GUID if still selected.
The core's independent GUID/nonce confirmation retains an 80 ms error-grace
interval; a cast ACK does not guarantee that gold or items reached inventory.
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
selection, mouseover emulation, additional launcher, AutoLoot
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
force target selection/movement, or modify AutoLoot; the server still controls
real spell availability. Validation requires a new exact x86 binary,
finalized TEST candidate and a user's in-game report.

## 1.0.7 TEST: immediate exact-GUID target clear

The native GUID cast is unchanged. Immediately **after** the verified
12340 native spell-921 call returns on the game/UI thread, the policy
executes one Lua conditional: it clears the current target only when
both the active attempt nonce and the full currently selected target GUID
still match the PP attempt. If another target is selected, the callback
leaves it alone. On a later exact-GUID `SPELL_CAST_SUCCESS` event, the
existing Lua observer rechecks the selected GUID and applies the same
conditional clear, covering clients that update the visible target after
the native call returns. No unconditional ClearTarget, forced target
selection, spoof movement, chat output, extra loot or new native Lua
callback registrations. Clearing after *submission* does not imply
confirmed PP success; the established GUID/nonce result and timeout
handling remains unchanged. This is a TEST-only user-requested UI
behavior change; verify that loot and GUID cast correctness remain intact
in the installed exact-SHA package.

## 1.0.8 TEST: 400 ms attempt budget and positive-money fast path

A 400 ms engine timeout replaces the previous 900 ms bound. The game-thread
policy uses the **same** shared constant and an explicit GUID+nonce release
before the next cast. This is an experimental timeout: a late server result
may now be classified as unconfirmed more frequently. The per-GUID 3000 ms
backoff on timeout and existing attempt budget remain unchanged.

Immediately before a native cast, the Lua observer stores the player's
`GetMoney()` baseline and arms a new nonce. Its normal `PLAYER_MONEY`
event records a strictly positive copper delta only within 400 ms and
only while the matching attempt is armed; decreases and unchanged money
are ignored. The core can advance on an **experimental wallet+loot signal**
only when both this money event and `LOOT_OPENED` belong to the same active
nonce and the native loot-source GUID, if readable and nonzero, does not
contradict the attempted GUID. Explicit no-pockets and spell/range failures
have priority. Money alone never completes PP. Item-only PP continues through
its independent cast/loot confirmation or bounded timeout. If native loot
source is already cleared, simultaneous money+loot is an indicative signal,
**not proof of which NPC paid**; a different money source could be mistaken
for PP. Game tests should avoid other simultaneous gold income.

Structured loader reports use `reason=wallet_loot_signal` to distinguish
this accelerated heuristic from exact-GUID `reason=verified_result`.
Compare elapsed `result_wait_ms`, overall `cast_gap_ms`, timeout counts,
and actual received gold/items with the 1.0.7 report. No movement spoofing,
chat spam, mouseover substitution, extra loot calls or changes to AutoLoot.

## 1.0.9 TEST — bounded cluster burst (target: ~100 ms between submissions)

When at least three **unblocked** NPCs are concurrently within the real
4-yard cast radius, the engine switches to a four-slot GUID+nonce queue.
One native PP submission per game-thread loader pulse and a minimum 100 ms
interval between cast submissions are enforced; a cast is not counted as a
success simply because the native call returned. Existing exact-GUID
eligibility, player/target position, health and spell-readiness checks still
run before every cast. If the client or server applies a cooldown, the next
cast waits until spell 921 is usable. The 400 ms response timeout, bounded
retry budget and GUID history remain, now per outstanding attempt. The
usual single-target engine is retained outside a cluster.

The native observer maintains a fixed four-slot pending table and a Lua
spell-event record keyed by full target GUID and attempt nonce. Delayed
spell success/failure cannot confirm another NPC. During overlapping
casts, unscoped money and LOOT_OPENED are **not** used to mark a particular
NPC successful; the heuristic continues in ordinary nonoverlapping mode.
A missing GUID-tagged server acknowledgement is classified as unconfirmed
and yields a timeout even when money reached the wallet. Native Lua event
callbacks are **never** registered; the existing in-game Lua frame is used.
The observer's old GUID records are released after the matching attempt;
loading a different world clears the fixed pending table and Lua table.

The 60 ms spatial snapshot now caches GUID-to-object pointer pairs. Each
cast validates manager and exact GUID before using the pointer, and always
checks actual type, health, spell usability and both live positions again.
Cache misses fall back to the earlier full GUID scan. This avoids repeated
object-manager walks for an already detected four-mob cluster without
accepting stale range or identity.

The log's new `pending_count` tracks submitted but unresolved attempts;
`result_wait_ms` uses the exact nonce timestamp even if results arrive out
of order. TEST with four closely grouped stationary mobs, then sprinting,
and compare real gold/items, no-pockets errors, actual server-confirmed
spell-921 events, cast gaps, client FPS and stability. A 100 ms interval
between native submissions is an experimental goal, **not** evidence the
server accepted four thefts or awarded their loot. The canonical loader,
AutoLoot, client binary and main/work branches are not changed.
