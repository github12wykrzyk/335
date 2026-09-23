# AutoPickPocket — universal 12340 x86 loader integration

Development branch: `feature/autopickpocket-12340`. Canonical universal updater and external game-thread loader are inherited from current `work`. Do NOT install the retired `src/Loader12340/Wow335Loader.dll` or a second updater.

Native `AutoPickPocket335.dll` exports `W335_MessageId`, `W335_HookProc` and `W335_CallWndProc` and is loaded by the same `src/Loader/loader_win32.c` manifest chain as AutoLoot. It has independent stealth, known/usable spell 921, cooldown, native undead/humanoid type (6/7), GUID cast, world token and correlated result policies. No target switching, movement hooks, extra auto-loot or chat messages. Logs go to `.wow335_debug/AutoPickPocket.jsonl`.

The one-time `activate_autopickpocket.yml` gate on the feature branch builds a genuine native PE32 x86 PP DLL with the pinned MSVC toolchain, audits exports and exact client ABI, registers the exact binary and module ownership, and validates the full active AutoLoot+PP rebuild before publishing a new branch commit. Only the normal candidate workflow on that registered commit may publish the game package with `FINAL_PACKAGE: PASS`. The universal updater itself is still built only from `work`; select the AutoPickPocket feature branch in its game-branch selector to install the two-DLL candidate.

Do not present compilation as in-game proof. The first verified game test must confirm that startup, both hooks, actual stealth Pick Pocket on nearby undead/humanoid NPCs, retry/results and AutoLoot coexist without freezing or crashes. No game package is ready if activation, rebuild or final package gate fails.

## Crash #5: Lua callback removed (1.0.1 TEST)

The exact Wow.exe is pinned and unchanged. WoW Error #134 names `PP335_LootOpened` and an invalid function pointer inside `AutoPickPocket335.dll`. The old policy registered a DLL C function in the client's Lua VM. The new policy never registers a DLL callback from Lua: a normal Lua frame records nonce-scoped `LOOT_OPENED`, failure and spell-921 server cast events. A native loot-source GUID, if still available before the game's automatic looting closes the window, corroborates the exact target. An independently GUID-correlated server cast success is sufficient to mark the *cast* complete after a short grace period, not proof that loot reached inventory. Without matching evidence the existing bounded timeout/retry logic applies.

The registered DLL must be refreshed with a genuine MSVC PE32 x86 rebuild and the manifest SHA256 updated together on this feature branch. A source-only commit will fail the strict game-package gate; install only the subsequent exact-HEAD `FINAL_PACKAGE: PASS` candidate through the existing universal updater. Do not reuse the previously crashed 1.0.0 DLL; keep `work` (AutoLoot-only) as the rollback. The user must test actual Pick Pocket, stable startup, no Lua callback crash, native loot and coexistence with AutoLoot in-game.

## Target release and faster next-NPC scan (1.0.2 TEST)

Only after a GUID-correlated `PP_RESULT_SUCCESS` does the game-thread
policy conditionally call `ClearTarget()` if `UnitGUID('target')` still equals
the just-pickpocketed GUID. Other manually selected targets are untouched;
no target is cleared for an unsuccessful cast, timeout or rejected NPC.

After success or confirmed empty pockets, the core immediately scans for a
*different* eligible GUID in the same loader pulse, instead of waiting for
the next 100 ms scan cycle. Routine scans are bounded to 50 ms. The native
adapter first rejects distant units by range and only invokes the expensive
native creature-type/eligibility policy for nearby NPCs. The actual cadence
remains limited by game-thread pulses, stealth, skill cooldown and server
result delivery; no changes to AutoLoot or the loader are required.
