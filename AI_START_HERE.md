# START — WoW 335
1. Read AGENTS.md, this file, AI_INDEX.json, CURRENT.json, runtime/current.json in this order.
2. Resolve actual branch HEAD and only affected source/dependencies. src/ is editable; old repos are reference only.
3. work is the development branch; main changes only after an explicitly accepted selective promotion. Confirm live HEAD and runtime/current.json before relying on historical bootstrap notes.
4. Run python tools/verify_repo.py, python tools/verify_current.py and python -m unittest discover -s tests -v.
5. Exact user-selected 12340 x86 Wow.exe is pinned in runtime/client_exe_target.json. Runtime files, module ownership and readiness must be read from the current branch, not inferred from bootstrap status. A runnable candidate requires exact EXE, all registered DLLs, matching native x86 rebuilds and FINAL_PACKAGE: PASS. CI cannot prove in-game functionality.
6. Updater is a separate .NET Framework x86 external application. Its build does not certify a game package.
7. Before adding any DLL, register its owner, canonical sources, dependencies, hook arbitration, x86 build contract in runtime/module_registry.json. Run tools/verify_module_registry.py; incomplete contracts fail closed. CI is not an in-game test.
8. TEST source changes must also update the exact matching built DLL bytes and manifest; build_active.py refuses stale/mismatched runtime DLLs. STABLE requires exact SHA-addressed xz recovery for each accepted DLL and package_exact_current.py, not a fresh compile.
9. Check runtime/ai_experiments.json after the five entrypoints; tools/ai_experiments.py route --module NAME provides non-mutating advice. Check live GitHub HEAD/branch existence before a write. Record only proven exact-SHA user game tests.
