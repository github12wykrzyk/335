# AutoLoot 3.3.5a (12340)

Canonical C sources for the TEST branch AutoLoot335.dll. The engine/adapter
are the tested isolated AutoLoot12340 experiment, packaged only with the
exact user-selected Wow.exe and a generated dlls.txt once the strict native
build and candidate gate pass. The GUI updater embeds its own x86 game-thread
launcher; WoW does not automatically parse dlls.txt.

The WoW335AutoLootDiag Lua addon under diagnostics/ is an optional standalone
window test, not an active game DLL. See NATIVE_ADAPTER.md for loading,
resource ownership and first in-game TEST acceptance requirements.
