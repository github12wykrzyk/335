# Packet-based AutoPickPocket 12340 (isolated experiment)

Branch: `feature/autopickpocket-packets-12340`. This is NOT the working
`feature/autopickpocket-12340` native spell-cast implementation; no edits are
promoted to `work` or `main`.

The WoW 1.12.1 packet transport/address/layout cannot be reused. The audited
exact client has SHA256 `2236646eca33960431eb1c5331c0b8cce516f2f82e2885c17241b54e92c18c3d`.
The portable 3.3.5 serializer constructs a 32-bit client DataStore opcode
`CMSG_CAST_SPELL=0x12E` followed by 8-bit cast count, spell 921 (LE32),
8-bit zero cast flags, 32-bit unit target flags (LE32 2), and packed GUID
(mask + nonzero bytes). The opcode prefix is NOT the socket framing.

Status: protocol codec and read-only exact-client send-path audit, NOT a
validated live packet injector. Native transport ABI and collision with
normal client casts must be established before linking the serializer to the
same singleton AutoPickPocket game DLL. Until then the legacy registered DLL
must not be labelled a packet implementation and no packet-mode TEST package
may be published. Sender must run on game thread, use validated GUID/range/
stealth checks, hold one correlated spell result, use bounded retries, and
coexist with AutoLoot without target selection, extra loot or UI.

Audit workflow: `.github/workflows/audit_pp_packets.yml`.
Codec unit test: `python -m unittest tests/test_autopickpocket_packet.py -v`.
