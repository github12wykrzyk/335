# WoW335GUI — shared in-process GUI v1 (TEST)

One future-proof native Win32 x86 DLL loaded by the existing universal loader. A single
Insert owner opens a modeless window over the actual game in **windowed/borderless**
mode; exclusive fullscreen is **not** supported by this Windows popup backend.
No second D3D9 Present/EndScene detour is installed; the existing PlayerESP
renderer stays the only D3D owner. This prevents a regression in ESP frame pacing.

## ABI / runtime

See w335_gui_api.h. Modules export no UI window of their own: after their
W335 enable on the game thread, they call GUI's exported W335GUI_Register with
a stable module ID, DLL basename, a copyable field schema, and callback. GUI
deep-copies metadata and calls every setting callback once with persisted
defaults. Only GUI's game-window thread may access the ABI and UI.
W335GUI_Unregister must be called on stop before the module is freed.

GUI reads the already updater-verified `dlls.txt` from the WoW executable
directory as an allowlist, and uses `GetModuleHandleA` to show **only actual
in-process DLLs named by the managed active package**. No Windows/system or
third-party DLL is listed. If the allowlist is missing, invalid or unreadable,
the list remains empty (fail closed). Registered UI modules show READY; other
loaded managed DLLs show LOADED (legacy AutoLoot does not expose GUI status).
Loaded alone never means initialized or confirmed working in-game.

The managed DLL list retains manifest order, selected row and scroll position.
It is rebuilt only when actual membership or registration status changes; the
2-second status poll does not reset/repaint an unchanged list. New compatible
modules appear automatically through `dlls.txt`, with no GUI source edits.
Field controls are derived only from registered schemas. GUI stores values in
WoW335GUI.ini beside Wow.exe; the updater must preserve this user settings file.

The initial ESP adapter includes ESP visibility, Player/NPC toggles, Horde
and Alliance toggles, Unknown players, numeric range, and diagnostic crosses.
It **does not present hostility/mixed-BG classification or names as working
filters**, because the current exact-client scanner provides no reliable
hostility/BG team/name information. Do not invent these values. The normal
ESP renderer and fallback run unchanged. On mixed BG, faction-only filtering
does not substitute for hostile-to-me classification.

Shutdown: loader calls ESP disable (unregister), GUI disable destroys windows.
A game focus check accepts GUI-owned windows so ESP remains visible when
controls are clicked. ESP never intercepts Insert or creates its own settings window. SharedGUI is
the only Insert owner; its PlayerESP page also controls diagnostic foot markers.
No fallback ESP hotkey is installed when SharedGUI is unavailable.

Runtime current.json and module_registry.json require the exact PE32 x86
native build and registered SHA on the isolated feature branch before any
TEST package can be claimed. Static/CI success never proves game usability.
