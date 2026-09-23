# AutoLoot335.dll — native TEST module, WoW 3.3.5a build 12340 x86

This is the isolated AutoLoot12340 core/adapter/host promoted to the **work
TEST channel only**. Do not infer stable acceptance from an earlier preview
test. src/ is the canonical source; runtime/AutoLoot335.dll must match the
exact registered SHA256 and rebuild byte-for-byte with tools/build_active.py.

## Behavior and thread ownership
- Read exact 12340 client Object Manager, only lootable dead units within
  the host's configured 5-yard reach. No movement or target spoofing.
- Interact and handle only the loot window owned by this AutoLoot GUID.
- Use FrameScript on the game thread; never execute client internals from
  the external updater, a launcher worker thread or DllMain.
- The external x86 updater embeds a deterministic game-thread launcher and
  starts it automatically with **Uruchom grę** if the managed runtime manifest
  includes AutoLoot335.dll. The launcher loads that registered DLL from the
  same selected game directory. **dlls.txt alone does not load a DLL**.
- The game process and launcher must remain alive together. The
  built-in loader posts to the WoW window HWND and also advances the
  controller on ordinary keyboard/mouse messages (80 ms gate).
- The prior experimental native preview must not be launched in a session
  containing registered normal AutoLoot, to prevent conflicting double hooks.

## Provenance
The user reported the earlier exact-SHA native preview looted corpses in
WoW 3.3.5a build 12340; this does not verify the *new* regular runtime,
its self-updater/game-launch integration or held-key behavior. The new
runtime is **TEST**, never automatically promoted to main/STABLE.
The selected unmodified Wow.exe SHA256:
2236646eca33960431eb1c5331c0b8cce516f2f82e2885c17241b54e92c18c3d.
No WoW 5875 offsets are used. The client's function prefixes/ABI are
checked separately from actual in-game acceptance.

## Build and release gate
A Windows PE32 x86 runner compiles the canonical implementation with
MSVC /Brepro, records the exact DLL bytes and SHA256, then runs all
unchanged repository, runtime, module-registry and repeat-build checks
before registering the DLL on work. Only a candidate with a successful
FINAL_PACKAGE: PASS for that exact work SHA is installed by the
updater. On failed/unfinished CI the updater must not fall back to old
artifacts. No partial DLL install, arbitrary DLL load or manual ZIP copy.

For game testing, select TEST/work in the updater, update the updater,
click Aktualizuj to install the whole compatible stack and choose
Uruchom grę; do not use the older Natywny AutoLoot TEST preview button.
Verify auto-loot with and without held WASD / left and right mouse buttons,
manual loot windows, full bags, combat and the no-corpse case. The first
native package must be treated as unaccepted until the player reports
this exact-SHA test result.
