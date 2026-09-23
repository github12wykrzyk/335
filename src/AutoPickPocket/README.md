# AutoPickPocket / 12340 x86 — isolated source experiment

NOT YET a game DLL; it is NOT in runtime/current.json or dlls.txt.
The portable C decision engine is implemented and unit-tested, but cannot be
called a runnable module until an exact-client native adapter is audited and
built on MSVC x86. No AutoLoot files, loot interaction, movement hooks or chat
output are involved. This module must never call LootSlot or close loot windows.

## Native adapter acceptance contract — not implemented

- Verify the selected Wow.exe exact SHA256 and PE32 x86 ABI, with functions
  and structures independently checked for this exact client build 12340.
- Load on the authorized game thread through a loader that supports multiple
  registered DLLs and arbitrates hooks. Current work AutoLoot launcher loads
  one registered module; it cannot safely load this experiment.
- Scan living pickpocketable hostile NPCs in usable spell range, not merely
  nearby units. Check LOS and reset GUID cache on map/character changes.
- Check Rogue class, learned and usable Pick Pocket, stealth and transient
  conditions. Never assume the spell is castable from the spell name alone.
- Cast on the exact GUID without taking over the user's target. Using Lua
  CastSpellByName on target is not casting on an arbitrary GUID.
- Correlate the exact attempt with a verified result. Cast submission,
  timeout or a closed loot window NEVER proves a successful Pick Pocket.
- Write bounded structured events to .wow335_debug/AutoPickPocket.jsonl
  inside the game directory, with no chat or account paths/tokens. The
  existing WYSLIJ RAPORT collects it independently of the latest 3 logs.
- Before game activation register complete ownership/resource contracts,
  build a genuine Windows PE32 x86 DLL, verify exact bytes and require a
  FINAL_PACKAGE: PASS for the exact SHA; in-game behavior must be tested.

pp_tick scans at most once per 100 ms while enabled, selects nearest
eligible unblocked GUID, and does not count submission as success.
Success, no pockets, and terminal errors are cached per GUID for the session;
temporary failures are retried. pp_reset clears cache after a world change.
Proposed /appp commands are NOT available until a native adapter exists.


## Online research leads (2026-09-23; NOT verified on pinned Wow.exe)

- WotLK Pick Pocket spell ID 921; published 3.3.5a data says melee/combat
  reach 5 yards and Stealth required: https://wotlkdb.com/?spell=921 .
  Server reach, mob eligibility and conditions still need in-game validation.
- Standard Lua CastSpellByName accepts a unit token, not an arbitrary scanned
  NPC GUID. It is protected for ordinary addon code:
  https://warcraft.wiki.gg/wiki/API:CastSpellByName .
- A historical 3.3.5a community discussion mentions internal
  CastSpell(SpellID, Guid) at VA 0x0080DA40:
  https://wrobot.eu/forums/topic/5621-castspell-improvement/ .
  This is an **UNVERIFIED LEAD**, not an audited address, function
  signature, calling convention or permission to inject on build 12340.
  Never call it before validating the exact pinned PE bytes, disassembly,
  ABI, thread, argument memory ownership, result correlation and crash safety.

## Isolated x86 experiment build

Before native activation, feature/autopickpocket-12340 runs a dedicated
Windows MSVC x86 portable-core test executable. No in-game adapter exists,
so the isolated branch does NOT publish the unchanged AutoLoot-only candidate
as a purported AutoPickPocket game package. The regular full-game candidate
rebuild and all manifest/SHA256 gates remain mandatory if the feature
registers a native module, changes active game sources, or changes build
infrastructure. The existing work release remains untouched.

The prior feature build failure was unrelated to the PP core: the strict
full candidate recompiled AutoLoot on hosted VS 17.14.41 but obtained
SHA256 ffb27461272b191595520d2f4fb3ad095d429a4c818564e78b0e2f44b835b98a
instead of registered
6551b34fde100edaf0b9597b4e267d1844acf92da379fae344c41efeaa8c31c3.
The previous passing work run used hosted VS 17.14.40. Toolchain drift
is a plausible cause, NOT yet a proven byte-level diagnosis. Do not
replace the accepted AutoLoot DLL or bypass its rebuild gate.
