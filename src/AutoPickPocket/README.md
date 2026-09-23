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
