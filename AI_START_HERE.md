# START — WoW 335
1. Read AGENTS.md, this file, AI_INDEX.json, CURRENT.json, runtime/current.json in this order.
2. Resolve actual branch HEAD and only affected source/dependencies. src/ is editable; old repos are reference only.
3. New repo bootstrap is on work; main remains the old README until an accepted promotion.
4. Run python tools/verify_repo.py, python tools/verify_current.py and python -m unittest discover -s tests -v.
5. A real game candidate requires an exact 12340 x86 EXE + active DLLs, a verified root ZIP and FINAL_PACKAGE: PASS. No runtime exists at bootstrap.
6. Updater is a separate .NET Framework x86 external application. Its build does not certify a game package.
7. Before adding any DLL, register its owner, canonical sources, dependencies, hook arbitration, x86 build contract in runtime/module_registry.json. Run tools/verify_module_registry.py; incomplete contracts fail closed. CI is not an in-game test.
8. TEST source changes must also update the exact matching built DLL bytes and manifest; build_active.py refuses stale/mismatched runtime DLLs. STABLE requires exact SHA-addressed xz recovery for each accepted DLL and package_exact_current.py, not a fresh compile.
