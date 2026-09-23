# WoW 335 — AI operating contract
Target only github12wykrzyk/335, WoW 3.3.5a build 12340, Windows x86.
Read in order AGENTS.md, AI_START_HERE.md, AI_INDEX.json, CURRENT.json, runtime/current.json, then affected modules and workflows. PROJECT_INSTRUCTIONS.md contains the full Polish contract.
main = accepted stable, work = development, parallel only if it exists, feature/* isolated experiments, promote/* verified promotion. Never copy WoW 1.12 offsets/binaries.
Current manifest and exact source lineage determine active runtime. Empty manifest means NO GAME PACKAGE. Fail closed; never claim x86 smoke = game DLL test.
One logical multi-file commit; no force push. Routine: python tools/verify_repo.py; python tools/verify_current.py; python -m unittest discover -s tests -v.
TEST: compile actual affected Windows x86 modules, package only verified exact runtime, run tools/verify_candidate_package.py --finalize; publish only FINAL_PACKAGE: PASS. User tests exact SHA. STABLE: curate accepted exact bytes on current main via promote/*, validate exact promotion SHA, then promote selectively.
No automatic cross-branch promotion. Check HEAD/Actions after interrupted streams. AI handles files, code, GitHub, builds, rollback. Report truthful branch/SHA/tests/packaging/required in-game test.
