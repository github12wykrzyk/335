# AutoLoot 12340 native adapter — isolated implementation

The source under `src/AutoLoot/autoloot_12340_adapter.{c,h}` now implements
object-manager enumeration, health+lootable checks, nearest eligible corpse
selection (within a **maximum 5-yard** verified host range), GUID-based
interaction handoff, bounded loot-slot draining through a Lua execution
callback, game-thread gating, optional ON/OFF and per-GUID retries.

**Do not install this diagnostic DLL into WoW or publish a candidate ZIP.**
The existing adapter is intentionally unbound by default. The Windows x86
adapter DLL compiles for the requested target and exposes a host API; it does
NOT independently hook into the game or register as an active runtime DLL.
A separate, exact-client-verified host must supply and audit:

- Genuine process SHA256 against the pinned Wow.exe, not just an arbitrary
  "verified" return value. Recheck the file before every new session.
- The actual game-thread entry point/scheduler and safe UI/interaction
  arbitration. Never call client internals from a DLL worker-thread timer.
- Actual positions, safe memory reads and stable object lifetime for
  object-manager fields and the descriptor layout on this exact binary.
- `0x00731260` unit-right-click and `0x00819210` FrameScript_Execute
  full call signatures and thread conditions (static prolog matches alone
  do not prove these semantics).
- Owned loot window with exact corpse GUID, server-confirmed empty, blocked
  and full-bag states. Never drain or close a manually opened window.
- Collision/line of sight, alive/lootable/dead semantics, transition handling
  and coexistence with any future game DLL owning these logical resources.

Public reference addresses for WoW 3.3.5a **are candidates only**. The two
relevant sources for these independently written routines are:
- 12340 offset listings: https://github.com/Likon69/CopilotBuddy/blob/901af3e7db2bb8ce28f5b15f21e6e526e1b19732/Offsets335.txt
- WotLK Lua interaction example: https://github.com/Jnnshschl/AmeisenBotX/blob/e23d5844e56b57e2e3c2b647f92f9acb29892508/AmeisenBotX.WowWotlk/WowInterface335a.cs

No third-party source is copied into this repo. Existing static provenance:
`tools/audit_autoloot_abi.py` and the matching Actions report for the exact
selected EXE; this does not certify the missing binding callbacks.

`tests/test_autoloot_12340_adapter.py` compiles both real C units with
simulated memory and verifies fail-closed ABI bind, thread ownership, loot
request/drain, empty confirmation, per-GUID backoff and OFF. Windows CI
also compiles the native PE32 x86 DLL separately but MUST NOT claim
`FINAL_PACKAGE: PASS` before a real host and actual active runtime exist.

## Held keyboard / mouse capture — test of native preview

The x86 launcher now posts on/off/tick control messages to WoW's **window
HWND**, not `PostThreadMessageW` (which uses a null HWND and can be
filtered out in nested input message loops). The existing `WH_GETMESSAGE`
callback also performs a time-gated AutoLoot tick on any retrieved game-thread
message while the module is enabled, including ordinary keyboard and mouse
traffic. A reentrancy guard permits at most one tick in flight and no more
than one every 80 ms. There is no gameplay key-up gate.

The active runtime still remains empty: this is an independently built
experimental native preview retrieved by the TEST updater from an exact SHA.
The in-game report that the earlier preview looted normally is evidence for
that earlier test only; **held-input behavior for this new SHA is not yet
verified**. These changes cannot execute while the client's game thread is
completely frozen or pumps no messages at all. Never advertise absolute
guarantees of background operation when the game is paused.


## 2026-09-23 — held-input responsiveness experiment
The previous WH_GETMESSAGE-only scheduler depended on normal posted messages
being retrieved. An external bounded SendMessageTimeout pulse now invokes a
second WH_CALLWNDPROC hook directly on the verified WoW window thread, while
WH_GETMESSAGE remains a fallback. Neither callback calls WoW from the launcher
thread. One reentrancy/time guard limits ticks to at most one per 40 ms and
the launcher keeps at most one pulse in flight, preventing posted-message
queue growth during long mouse capture. The portable engine retains a 600 ms
loot-window timeout but reduces the subsequent GUID-specific retry delay
from 1200 to 200 ms. Held-input behavior and actual game/server response
are NOT verified until this exact-SHA native preview is tested in game.
If WoW's window thread stops processing both sent and posted messages, a
client-verified frame callback is the next isolated architecture experiment;
do not invoke client memory calls from an arbitrary worker thread.
