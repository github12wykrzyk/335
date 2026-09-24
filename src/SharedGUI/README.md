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

Native enumeration uses Toolhelp32 snapshot of the **game process** and displays
every mapped DLL as LOADED. Registered DLLs are READY; LOADED alone never
implies initialized, working or verified gameplay. Legacy AutoLoot remains
visible without changing its native runtime. Field controls are derived only
from registered schemas; no loader rebuild is needed for a new UI-compatible
module. The GUI stores values in WoW335GUI.ini beside Wow.exe. The updater
must not remove this untracked user settings file.

The initial ESP adapter includes ESP visibility, Player/NPC toggles, Horde
and Alliance toggles, Unknown players, numeric range, and diagnostic crosses.
It **does not present hostility/mixed-BG classification or names as working
filters**, because the current exact-client scanner provides no reliable
hostility/BG team/name information. Do not invent these values. The normal
ESP renderer and fallback run unchanged. On mixed BG, faction-only filtering
does not substitute for hostile-to-me classification.

Shutdown: loader calls ESP disable (unregister), GUI disable destroys windows.
A game focus check accepts GUI-owned windows so ESP remains visible when
controls are clicked. Insert is suppressed in ESP when shared GUI is loaded;
Ctrl+Shift+Insert remains ESP's separate diagnostic shortcut.

Runtime current.json and module_registry.json require the exact PE32 x86
native build and registered SHA on the isolated feature branch before any
TEST package can be claimed. Static/CI success never proves game usability.
