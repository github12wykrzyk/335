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
