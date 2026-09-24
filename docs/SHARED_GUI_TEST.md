# Shared GUI — game TEST acceptance checklist

This file deliberately belongs to the isolated feature/shared-gui-12340
runtime. The GUI is a Win32 game-owned popup and supports windowed or
borderless WoW 3.3.5a build 12340, not exclusive fullscreen. The only D3D9
Present owner remains PlayerESP. Loader installs the full exact-SHA stack
AutoLoot335.dll -> WoW335GUI.dll -> PlayerESP335.dll.

After FINAL_PACKAGE: PASS on the **exact current branch HEAD**, check in game:

1. Run the pinned Wow.exe in windowed/borderless mode using the updater's
   complete verified feature candidate; do not install a single DLL manually.
2. Insert opens/closes the single GUI without toggling ESP labels. Verify mouse
   interaction; focus on the GUI must not hide ESP markers. Ctrl+Shift+Insert
   retains the separate ESP foot-marker diagnostic.
3. The DLL list includes all DLLs mapped into Wow.exe, including system DLLs;
   AutoLoot335.dll should be LOADED (legacy), PlayerESP335.dll READY once it
   registers, and WoW335GUI.dll LOADED. LOADED does not prove module activity.
4. ESP settings visibly affect Players, NPC, Horde, Alliance, Unknown players,
   and the configurable 5–100 yd distance; enable/disable the ESP labels.
   Hostility/mixed-BG teams and real unit names are not implemented; do not
   interpret faction filtering as hostility.
5. Change ESP settings, close GUI, exit WoW normally and reopen. Validate
   WoW335GUI.ini persistence beside Wow.exe.
6. Move camera and NPCs while toggling GUI. Confirm existing 12340 ESP visual
   cadence/rendering did not regress and AutoLoot continues working.
7. Press Insert repeatedly, change game focus, relog/enter instance, then
   exit: no stale HWND, duplicated GUI, hanging loader or crash.
8. Send exact-SHA in-game report with outcome, game mode and any specific
   failed step; static Windows x86 compilation is not gameplay proof.

The canonical candidate workflow must run on the current remote branch HEAD
and publish a TEST game ZIP only after FINAL_PACKAGE: PASS.
