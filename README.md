# WoW 3.3.5a (build 12340) — Windows x86
AI-led development for github12wykrzyk/335. Start with AGENTS.md and PROJECT_INSTRUCTIONS.md.
This work branch is INFRASTRUCTURE ONLY at bootstrap: the existing reference Wow.exe must be replaced by the exact user-selected binary pinned in runtime/client_exe_target.json. No active game DLL, playable candidate or accepted stable runtime is present.
GitHub Actions independently verify repository contracts and build a WoW335Updater x86 Windows GUI. Candidate packaging fails closed until verified 12340 runtime artifacts exist.

## Client EXE audit / candidate gate
`Wow.exe` is an uploaded reference client, not a verified runnable game package. `Build 335 work candidate` audits PE32 x86, SHA256, Win32 file-version resources and readiness. With an empty `runtime/current.json`, it publishes an **EXE-AUDIT** JSON artifact only; the updater intentionally cannot install that artifact. A runnable game package requires the exact registered EXE, real active x86 DLLs, their build recipe and `FINAL_PACKAGE: PASS`. A matching file-version resource helps identify 3.3.5a build 12340 but cannot prove gameplay compatibility.

## Exact user-selected client upload
Replace the existing root Wow.exe on branch work via https://github.com/github12wykrzyk/335/upload/work . Required SHA256: 2236646eca33960431eb1c5331c0b8cce516f2f82e2885c17241b54e92c18c3d; length: 7717528 bytes. Candidate audit will fail closed until the new file is uploaded. A matching Win32 3.3.5.12340 version resource is necessary but not proof of gameplay compatibility. The updater rejects packages with another EXE and no runnable package is issued until real active x86 game DLLs pass their build and final package verification.

## Server environment (user-confirmed)
The game server uses **TrinityCore (TCCore / TC) 3.3.5 base**. The exact upstream commit/revision, fork and server-side customizations have not been established; do not assume stock TrinityCore behavior without checking the actual server or in-game evidence.

Client-side DLLs target the exact selected WoW 3.3.5a build 12340 Windows x86 binary. TrinityCore 3.3.5 server code is a reference for server-side mechanics and diagnostic interpretation (for example, Pick Pocket eligibility and rejection), **not** evidence of client memory layouts, addresses, ABI or working in-game integration. Do not vendor the server source or change client/runtime manifests solely because of this environment note.
