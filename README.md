# WoW 3.3.5a (build 12340), Windows x86 — main

`main` contains a STABLE **infrastructure-only** baseline. `runtime/current.json` is EMPTY: no accepted game DLL, patched EpochConnection DLL or runnable STABLE game package is active.

Isolated Epoch loader and AutoLoot source remain as inactive test/reference code for audit. A source file, TEST binary or successful updater build does not certify in-game compatibility. The updater built from main selects STABLE by default and does not enable the isolated Epoch TEST install flow. Development stays on `work` and `feature/*`.

To activate STABLE, curate a complete user-accepted exact-SHA in-game-tested module set on `promote/*` based on current main, including dependencies and SHA-addressed recoverable DLL artifacts. The exact promotion commit must pass the strict gate before main can publish `FINAL_PACKAGE: PASS` game artifacts.

Read AGENTS.md, AI_START_HERE.md and CURRENT.json before editing.
