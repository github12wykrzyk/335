# AutoLoot 12340 — source-only isolated experiment

Target: the **exact** pinned WoW 3.3.5a 12340 Windows x86 client from
`runtime/client_exe_target.json`. This is a portable decision engine,
**not a working DLL or runnable package**.

## Behavior being ported from WoW112

Nearest eligible lootable corpse; no mouseover or target requirement;
one owned interaction at a time; bounded wait for the loot window;
repeat loot-all while the server reports remaining loot; defer failed,
unreachable and full-bag corpses per GUID; no manual loot-window stealing.
An explicit OFF toggle stops all future actions. The core does not implement
PickPocket and must not spoof coordinates, acquire movement ownership or
reuse any WoW 5875 addresses, hook sites, opcodes or structures.

The adapter MUST independently validate the exact pinned executable and
identify for build 12340: object enumeration, live lootability/ownership,
real interaction/range/LOS, loot-window ownership, server-confirmed empty
state, native loot-all and safe game-thread scheduling. An "open" UI by itself
is not proof that all items were collected. Scans and native calls must run
on a verified game thread, never from DllMain or a timer worker. Use one
shared arbiter when another active DLL controls targeting, movement, or
loot UI; do not overwrite a user's target or close manually opened loot.

The range is intentionally supplied by the verified adapter instead of
hard-coding WoW112's 300-yard patch. If the exact-client interfaces cannot
be established, the adapter must fail closed and not activate this engine.

## Activation gate

Do not register this core as an active runtime DLL, claim `FINAL_PACKAGE:
PASS`, or expose it as installed in the updater until the native 12340
adapter exists, a real PE32 x86 DLL is built and checked against its
registered bytes, integration resources/dependencies are declared in
`runtime/module_registry.json`, and exact-SHA candidate CI passes.
The user must then test corpse detection, loot completion, full bags,
manual loot windows, distance/LOS, combat, and interaction with other DLLs.
