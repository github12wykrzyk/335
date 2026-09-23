# Isolated native-frame AutoLoot experiment — exact 12340 only

Canonical experiment source is copied with its original Git blob IDs from the
`work` branch at `3cbc14b66aa2ed5687b0132190c4381494c0bdb6`.
The host file is a new derivative, not the previously accepted work DLL.
No third-party code is copied.

Exact client's static PE audit on GitHub Actions:
`feature/loader-12340` commit `ce83fe40b59b3e73721941d9e2bd1a5b255ed391`,
workflow run `35879764556`. Original `OnLayerUpdate` at `0x004FA040`,
prolog `55 8B EC 83 EC 3C D9 45 08`, ONE read-only data-table
reference `0x009F9A50`. `OnWorldUpdate` at `0x004FA5F0` had no
pointer in the scanned nonexecutable PE sections: do not hook that
function based solely on public names.

On receipt of a one-time (or retry while world is not initialized) control
message on the verified game-window thread, the native host checks executable
SHA256 + known client internal prologs, `s_currentWorldFrame` at
`0x00EEEA8C`, its object's vtable range, entry contents and byte-for-byte
OnLayerUpdate prolog. It swaps a single aligned vtable slot using atomic
compare-exchange, preserving the original method. The x86 naked thunk
preserves GPRs, EFLAGS, ECX, stack and tail-jumps to the real frame update.
The engine runs only when callback thread == verified window game thread,
at most once per 40 ms, guarded against reentrancy.

This is a candidate, not an accepted 12340 game module. The source's correct
static bytes do NOT prove OnLayerUpdate's call frequency, execution-thread
identity, or that the server permits loot while moving. Until the matching
DLL's MSVC PE32 x86 build, pair compatibility, manifest and updater delivery
are verified, the old work/TEST runtime and Epoch loader remain unchanged.
Do not mark FINAL_PACKAGE PASS for a standalone DLL.
