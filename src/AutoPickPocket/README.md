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
