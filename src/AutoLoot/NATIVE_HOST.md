# Native AutoLoot 12340 — implementation status

## Actual data used, not another manual diagnostic

The pinned `Wow.exe` is PE32 x86 and has SHA256
`2236646eca33960431eb1c5331c0b8cce516f2f82e2885c17241b54e92c18c3d`.
Its exact function bytes at unit right-click, Lua execution and object
position are extracted in CI. The exact client's base\n`CGObject_C__GetPosition` at `0x004D5EA0` returns zero for generic\nobjects; the native host instead calls the unit override at\n`0x006E6F10`, separately audited in the exact EXE. Independent public 3.3.5a (12340) references
indicate object-manager pointer `0x00C79CE0 -> +0x2ED0`, first object
`+0xAC`, local GUID `+0xC0`, unit type `+0x14`, linked-list next
`+0x3C`, GUID `+0x30`, descriptor `+0x08`, unit health field
`0x18`, dynamic flags `0x4F`, lootable bit `1`.

The new native `autoloot_win32_host.c` supplies actual Windows and stock
12340 callbacks to the existing C decision engine: SHA verification of the
on-disk game executable in the target process, pinned executable function
prefixes, safely bounded memory reads, game-object positions, thiscall
right-click with autoLoot flag, owned loot GUID at `0x00BFA8D8`, and
FrameScript_Execute. It also exports a WH_GETMESSAGE hook to let an
explicitly selected x86 launcher request ticks on the game's own window
thread. The corresponding `autoloot_win32_launcher.c` starts the selected
Wow.exe, selects that process's window thread and posts bounded 100ms ticks.
There is **no** patch to Wow.exe, no cross-build fallback, no 1.12 ABI copy,
no stealth or antivirus/anti-cheat bypass, and no injected worker-thread
client calls.

**This is an unregistered, untested-in-game preview**, not a validated
complete runtime. Build or successful static address verification alone
does not prove that Windows' GUI message thread is the only permitted Lua
thread, that object position and OnRightClick ABIs work under all gameplay
conditions, or that the server permits the interaction. The WH_GETMESSAGE
hook is used to establish an execution path, but may be blocked by Windows
privilege boundaries or the server. The updater must own exact-byte DLL
installation, hook lifetime, rollback, GUI control, whole-stack ownership
contracts and game test before promotion.

The supplied GitHub report
https://github.com/github12wykrzyk/335/issues/2 confirms only that manual
corpse interaction opened loot windows and existing 3.3.5a Lua LootSlot
commands cleared slots. The player had `autoLootDefault=1`; that is not
evidence of automatic corpse discovery or native right-click success.

Independent research references (design/offset research only, no source
copied):
- https://github.com/johnmoore/WoW-Object-Manager/blob/master/WoWObjMgr/PlayerScan.cs
- https://github.com/Likon69/CopilotBuddy/blob/master/Offsets335.txt
- https://github.com/suprepupre/wow-optimize/blob/main/src/allocators/loading_defrag.cpp
- https://github.com/wowgaming/3.3.5-interface-files/blob/main/LootFrame.lua

**Do not mark the runtime manifest active or emit FINAL_PACKAGE: PASS
until the game DLL is incorporated in a complete exact-SHA game package
and the updater/loader installation path is proven and tested in game.**
