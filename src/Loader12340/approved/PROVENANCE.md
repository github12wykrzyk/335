# Exact work AutoLoot binary — external source provenance

This file is NOT an independently authored Loader12340 AutoLoot implementation.
It is an exact-byte copy (same Git blob, no recompilation) of
`github12wykrzyk/335` work SHA
`3cbc14b66aa2ed5687b0132190c4381494c0bdb6`,
`runtime/AutoLoot335.dll`; git blob
`9169f0874e470ce030ef4727f6120a04059eab30`;
SHA256 `6551b34fde100edaf0b9597b4e267d1844acf92da379fae344c41efeaa8c31c3`.
The exact 12340 x86 DLL was rebuilt and package-verified in work Actions
run 35872640642 (FINAL_PACKAGE: PASS). This does not prove in-game behavior.

The isolated feature/loader-12340 runtime manifest remains empty: this
binary is a pinned external dependency embedded in the TEST updater and is
installed only for a proven existing managed work AutoLoot with transactional
migration of the DLL and both updater states. No unknown DLLs are scanned,
created or accepted by name only. STABLE/main is unchanged.
