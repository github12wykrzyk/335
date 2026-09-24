# AutoPickPocket packet experiment — WoW 3.3.5a exact build 12340

Branch: `feature/autopickpocket-packets-12340`. The original native
`feature/autopickpocket-12340` and `work` / `main` are unchanged.

This feature uses the existing registered `AutoPickPocket335.dll`, game-thread
loader, NPC scan/eligibility/GUID engine and Lua result observer but **replaces**
the five-argument native spell-cast submission with a direct, game-thread
`ClientServices::SendPacket(CDataStore*)` call. It NEVER loads both modes.
`AutoLoot` still owns loot windows; PP does no separate looting, targets or chat.

Exact pinned client SHA256:
`2236646eca33960431eb1c5331c0b8cce516f2f82e2885c17241b54e92c18c3d`.

Audited native call chain in this exact PE32 x86 EXE:
`0x0080B2F5` builds `CMSG_CAST_SPELL=0x12E` in a DataStore;
`0x0080B4E3` prepares `CDataStore*` argument and calls
`0x006B0B50` (cdecl, one argument); wrapper reads connection singleton
`[0x00C79CF4]` and calls `0x00632B50` (thiscall, CDataStore*).
CDataStore layout is 24 bytes on x86:
`[vtable,buffer,base,alloc,size,read]`.
Original vtable `0x009E0E24`. Client state 5 and exact ABI bytes
are gated before submission; the full EXE SHA is verified at bind.
The stack-owned datastore and small 23-byte packet are consumed by the
client's synchronous send invocation, not retained by this module.

Protocol payload after client DataStore's four-byte opcode:
8-bit cast count, uint32 spell ID 921, uint8 cast flags zero,
uint32 target flags 2, packed GUID mask and its nonzero bytes.
A 1.12 sender, offsets, header or vtable are NEVER used.

Packet submission is not proof of a server-accepted cast or loot.
The existing game-thread policy correlates each attempt's target GUID and
spell 921 result, no-pockets failure and bounded timeout. In-game verification
of speed while sprinting, result attribution, target preservation, connection
and rapid transitions remains mandatory before a stable promotion.

Run `python tools/verify_repo.py`,
`python tools/verify_current.py`,
`python -m unittest discover -s tests -v`, then real MSVC PE32 x86
module registration and exact SHA rebuild in the feature-only workflow.
Only a successful `FINAL_PACKAGE: PASS` for its exact feature HEAD is a
downloadable game TEST package; a codec-only x86 harness is not.

## 1.1.1 TEST — verified native bind blocker (user report: no PP)

A direct byte comparison of all the packet-mode DLL's native runtime signature
checks against the pinned `Wow.exe` found the deterministic blocker:
`0x00632B50` begins with `83 BE 34 05 00 00 05` (CMP imm8), not the
previously expected `81 BE 34 05 00 00 05 00 00 00` (CMP imm32).
The previous `packet_sender_abi()` always returned false, therefore
`PP335_BindOnGameThread` refused activation without emitting any cast.
Corrected to the exact verified bytes; packet sender ABI audit is now triggered
by changes to the host as well. The byte check alone cannot prove in-game
reception or success; the next exact-HEAD TEST requires a new PE32 x86 build,
package gate and the user's actual in-game confirmation.

## 1.1.2 TEST — packet no-result adaptation, native cast path

A user-supplied game report contained consecutive packet submission events followed by ~900ms timeouts without server/loot evidence. Packet dispatch is not success. On TWO timed-out exact-GUID and nonce packet submissions, this experimental module changes transport once per process to the verified five-argument native GUID-cast mechanism; it never transmits both paths for one attempt. Structured events contain `packet_no_ack_native_fallback`, `transport` and `no_ack_count`. Native fallback is NOT a confirmation of raw-packet functionality. Native path also observes nonce-scoped `PLAYER_MONEY` together with `LOOT_OPENED` within 400ms; report labels this as indicative `wallet_loot_signal`, not GUID-proven success when loot-source GUID is unavailable. The previously selected user target is not cleared after native casting. User must verify real theft and absence of target side effects in game.

## Target ownership (CI regression)

Never call ClearTarget(), TargetUnit() or UnitGUID('target') from the packet or fallback policy: a previous experimental native target-release callback violated the branch's existing no-target-mutation invariant and was removed. Native cast target side effects remain subject to actual in-game verification.

Mandatory plain-Python unit test `tests/test_autopickpocket_exact_packet_gate.py` checks every runtime byte gate against the exact pinned PE32 x86 EXE and verifies the original client send-call displacement, preventing the earlier silent bind failure from passing ordinary verify CI.

## Separate 112 architecture adaptation: cast acknowledgment gate

The wow112 parallel manifest lists active AutoLootPP v0.14, LongPickPocket v1.0 and PickPocketSelectiveRange v10. Editable sources for the first two are explicitly marked functionally equivalent reconstructions, not original sources. This branch carries no 5875 address, opcode framing, detour or binary.

Live result policy now reports GUID-scoped `SPELL_CAST_SUCCESS` as `PP_RESULT_CAST_ACK` only. Burst keeps that GUID pending without stalling other NPCs; confirmed theft requires a matching live loot-source GUID plus same-attempt loot and wallet observations. Failed correlation remains UNKNOWN. The client-owned AutoLoot window behavior is unchanged and no native spell fallback is introduced. These observations are NOT yet a verified 12340 incoming packet / LOOT_MONEY / LOOT_RELEASE interceptor. The full user-requested adaptation and in-game validation remain open.
