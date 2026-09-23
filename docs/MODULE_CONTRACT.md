# Native module integration — WoW 3.3.5a build 12340

The active stack is defined by `runtime/current.json`. Its DLL entries MUST have
exactly matching `component` declarations in `runtime/module_registry.json`.
The registry is a reviewed declaration, not a binary hook scanner or evidence of
correct in-game operation. Never import offsets from build 5875.

For each added DLL:
1. Place canonical C/C++ implementation and headers under `src/<owner>/`.
2. Register the exact PE32 x86 DLL path and SHA256, version, canonical_source,
   dependency ordering and component in `runtime/current.json`.
3. Register the component in `runtime/module_registry.json`, with:
   - `sources`: all source files relevant to ownership analysis.
   - `requires`: active components which must load earlier; exactly mirrors
     the runtime manifest `depends_on` list.
   - `resources`: declared `wow12340:`, `win32:` or `logical:` resources
     for hook addresses, WndProc, movement, target, cast, input, etc.
     Every resource has `mode` = `exclusive`, `observe` or `chain`.
     Multiple writers require a single named, active chain arbitrator; do not
     declare two independent writers as observers.
   - `build`: `toolchain: msvc_x86`, `sources`, `include_dirs`,
     `libraries`, `cflags`, `ldflags`. Provide explicit empty arrays as
     applicable. A multi-DLL mechanism belongs to one experiment, but each
     DLL still gets its own ownership and build declaration.
4. Update `AI_INDEX.json` active module entries, retaining exact source lineage.
5. Run `python tools/verify_repo.py`, `python tools/verify_current.py`,
   `python tools/verify_module_registry.py` and unit tests.
6. TEST builds the affected modules and their reverse dependencies using
   `tools/build_active.py`. The Windows x86 PE32 rebuilt DLL SHA256 must equal
   the registered runtime DLL bytes; updating only source without updating the
   recorded binary is a hard failure. A TEST package always includes the whole
   active compatible stack in manifest order and exact-SHA source/package
   metadata. The full native compilation is not attempted in empty bootstrap.
7. Record the exact test SHA/package and actual in-game results. A successful CI
   run verifies compilation and static contracts, never gameplay.

STABLE: curate only accepted changes onto a branch based on current `main`,
set stable metadata, and make every accepted DLL exactly recoverable as an XZ
cache under `artifacts/runtime_cache/<sha256>.dll.xz`, with matching
`binary_artifact.kind/path/sha256/size` in the runtime manifest. The gate
verifies cached bytes against active bytes and packages those EXACT bytes.
If any cache is missing, promotion fails; do not replace it by a new build.
`main` must not move before the exact promotion SHA has passed all gates.

The external updater consumes successful artifacts at the current branch HEAD,
rejects stale/mismatching commit identity, verifies package SHA and backs up
managed files; new unrelated DLL/dlls.txt name collisions block installation.
Single-DLL installs are NOT permitted without a compatible complete manifest.

Incremental CI saves compilation time by selecting changed sources and reverse
dependencies. Every active DLL still has registered exact bytes and order in the
whole candidate package. Static ownership checks do not prove transient hook
arbitration or behavior in a live build 12340 process.
