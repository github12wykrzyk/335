# AutoPickPocket / 12340 x86 — isolated source experiment

An isolated PE32 x86 adapter exists, but it is NOT an activated game DLL and is NOT in runtime/current.json or dlls.txt.
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
The native silent argument dispatcher accepts on/off/reset on the verified game thread, but /appp is NOT registered in WoW by the current loader; these commands are NOT yet available in-game.


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


## Native adapter implementation (isolated experiment)

Canonical source: autopickpocket_12340_adapter.{c,h} and
autopickpocket_win32_host.{c,h}; native Windows x86 adapter is separately
compiled and mock-tested. NOT in runtime/current.json or dlls.txt.
The exact pinned EXE static audit on 2026-09-23 confirmed:
- address 0x0080DA40 is executable and has 18 direct E8 call sites;
- native callers push five 32-bit arguments and clean 0x14 bytes in caller;
- at inspected call sites the argument order is spell ID, zero,
  target GUID low32, target GUID high32, zero.
- the first 18 bytes at 0x0080DA40 and the caller cleanup at 0x00510423
  are checked at runtime IN ADDITION TO the full pinned Wow.exe SHA256.

These are static ABI checks, NOT an in-game successful cast proof.
The isolated native adapter is a real PE32 x86 DLL with an internal cast
entrypoint but will NOT activate on its own. PP335_BindOnGameThread requires
a separately authorized shared loader and non-null, game-thread-safe policy
callbacks for hostile pickpocketable NPCs, learned/usable spell and Stealth,
and a verified result for the exact GUID and monotonically increasing attempt nonce. The result observer must arm that GUID and nonce before submission; a late response from an older attempt never counts as success. All unknown cases
must fail closed. No fallback to Lua targeting or to a different GUID.
The loader on work currently supports ONLY the registered AutoLoot,
and the experimental loader on feature/loader-12340 is a separate
unaccepted branch. No automatic cross-branch merging is allowed.

The isolated build report is NOT FINAL_PACKAGE: PASS and the DLL is NOT
a playable test package. Do not put it manually into the client or dlls.txt.
Standalone PP action never calls AutoLoot, LootSlot, or closes loot windows.
Diagnostic events are bounded JSONL under .wow335_debug/AutoPickPocket.jsonl
and sent by the existing reporter once installed in a future verified stack.

## Cast-time position guard

Immediately before the cast, the adapter re-resolves the player and target
GUIDs from the object manager and rechecks target health, type, native position,
policy eligibility and the 5-yard 3D distance. A moving player or despawned
NPC can invalidate a preceding scan. Failure uses existing bounded retry and
backoff. This safety guard does not supply the still-missing authoritative
NPC/spell/result policies, game-thread loader or in-game validation.

## Delivery gate (2026-09-23)

`.github/workflows/autopickpocket_delivery.yml` is an independent mandatory
full-TEST gate for this feature branch. A green isolated adapter job does NOT
mean the requested gameplay feature or installable package exists. The gate
uploads an exact-SHA blocker report even when integration is incomplete.
It requires native hostile-NPC/spell/attempt policies, one game-thread loader
handling AutoLoot and AutoPickPocket, an updater that installs and launches the
verified two-DLL manifest, active module registration, a strict reproducible
x86 build and FINAL_PACKAGE: PASS. Only then may an exact-SHA player game test
be requested. Failed readiness is work to implement, not a reason to relabel
an isolated x86 artifact as an AutoPickPocket game package.

## NPC policy native ABI audit

The exact pinned Wow.exe CI audit now prints executable section bytes and direct
call sites for candidate creature type/reaction functions at 0x00605570,
0x006061E0 and 0x0071F6E0 in addition to the existing cast audit. These
addresses are unverified leads from public 3.3.5a code notes:
https://drewkestell.us/Article/6/Chapter/17 and
https://www.elitepvpers.com/forum/wow-bots/678970-offsets-3-3-5a.html .
A function sharing a known name or executable address is NOT proof of its ABI,
semantic hostility check, argument order or safe use on the selected client.
Only the pinned binary and in-game evidence can close this part of the policy.

## Compatibility with current work loader / narrow creature filter

The native host now exports the existing work loader's `W335_MessageId`,
`W335_HookProc` and `W335_CallWndProc` and uses its game-window thread pulses.
This is ABI integration only: no extra loader, no new AutoLoot hook, and no
claim of a playable package. Creature eligibility is restricted at the host
boundary to creature type ID 6 (Undead) and 7 (Humanoid), rechecked before
both scan and cast. An unknown type is never accepted.

The manifest-driven work launcher fails closed if this DLL is registered
without a concrete, independently audited `PP335_VerifiedPolicyV1` export.
That provider is NOT yet implemented; its NPC hostility, stealth/spell and
GUID-attributed server-result callbacks must be verified on the exact client.
The DLL remains inactive and must NOT be manually added to dlls.txt. The
current work runtime and its accepted AutoLoot byte hash remain unchanged.
