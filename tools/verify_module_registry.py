"""Validate cross-module ownership contracts for the exact WoW 12340 runtime.

The registry is an auditable *declaration*, not a substitute for binary analysis
or in-game verification. No hooks from WoW 5875 may be reused implicitly.
"""
from __future__ import annotations
import json
import sys
from pathlib import Path
from manifest_common import ROOT

MODES = {"exclusive", "chain", "observe"}
PREFIXES = ("wow12340:", "win32:", "logical:")


def validate(runtime, registry):
    errors = []
    if registry.get("schema_version") != 1 or registry.get("target_build") != 12340:
        errors.append("module registry must target 12340 schema 1")
    files = runtime.get("files", [])
    if not isinstance(files, list):
        return ["runtime files must be a list"]
    active = {row.get("component"): (index, row) for index, row in enumerate(files)
              if isinstance(row, dict) and row.get("kind") == "dll"}
    modules = registry.get("modules")
    if not isinstance(modules, list):
        return errors + ["registry modules must be a list"]
    declared = {}
    owners = {}
    for m in modules:
        if not isinstance(m, dict):
            errors.append("module entry must be an object")
            continue
        name = m.get("component")
        if not isinstance(name, str) or not name:
            errors.append("module missing component")
            continue
        if name in declared:
            errors.append("duplicate registry component: " + name)
            continue
        declared[name] = m
        if name not in active:
            errors.append("inactive/unknown component in registry: " + name)
        sources = m.get("sources")
        if not isinstance(sources, list) or not sources:
            errors.append(name + ": sources must list canonical src/ files")
        else:
            for source in sources:
                if not isinstance(source, str) or not source.startswith("src/"):
                    errors.append(name + ": source must be under src/")
                    continue
                try:
                    full = (ROOT / source).resolve()
                    full.relative_to((ROOT / "src").resolve())
                    if not full.is_file():
                        errors.append(name + ": missing source " + source)
                except (OSError, ValueError):
                    errors.append(name + ": unsafe source " + source)
        deps = m.get("requires")
        if not isinstance(deps, list) or len(deps) != len(set(deps)) if isinstance(deps, list) else True:
            errors.append(name + ": requires must be a list without duplicates")
            deps = []
        for dep in deps:
            if dep not in active:
                errors.append(name + ": missing active dependency " + str(dep))
            elif name in active and active[dep][0] >= active[name][0]:
                errors.append(name + ": dependency must load first: " + dep)
        if name in active and set(deps) != set(active[name][1].get("depends_on", [])):
            errors.append(name + ": requires differs from runtime depends_on")
        resources = m.get("resources")
        if not isinstance(resources, list):
            errors.append(name + ": resources must be an explicit list (possibly empty)")
            resources = []
        seen = set()
        for res in resources:
            if not isinstance(res, dict):
                errors.append(name + ": invalid resource declaration")
                continue
            key, mode, arb = res.get("id"), res.get("mode"), res.get("arbitrator")
            if not isinstance(key, str) or not key.startswith(PREFIXES):
                errors.append(name + ": resource must use wow12340:/win32:/logical: identifier")
                continue
            if key in seen:
                errors.append(name + ": duplicate resource " + key)
            seen.add(key)
            if mode not in MODES:
                errors.append(name + ": invalid mode for " + key)
            if mode == "chain" and (not isinstance(arb, str) or not arb):
                errors.append(name + ": chain must name an arbitrator for " + key)
            owners.setdefault(key, []).append((name, mode, arb))
        build = m.get("build")
        if not isinstance(build, dict) or build.get("toolchain") != "msvc_x86":
            errors.append(name + ": an explicit Windows x86 build contract is required")
        else:
            srcs = build.get("sources")
            if not isinstance(srcs, list) or not srcs or not all(s in (sources or []) for s in srcs):
                errors.append(name + ": build sources must be declared canonical sources")
            for key in ("include_dirs", "libraries", "cflags", "ldflags"):
                if not isinstance(build.get(key), list) or not all(isinstance(v, str) for v in build.get(key, [])):
                    errors.append(name + ": invalid build " + key)
    if set(active) != set(declared):
        errors.append("all and only active DLL components must have ownership contracts: missing " +
                      ", ".join(sorted(set(active) - set(declared))))
    for key, entries in owners.items():
        writers = [(n, mode, arb) for n, mode, arb in entries if mode != "observe"]
        if len(writers) > 1:
            if any(mode != "chain" for _, mode, _ in writers):
                errors.append(key + ": conflicting writers (exclusive resource)")
            elif len({arb for _, _, arb in writers}) != 1 or (writers[0][2] not in declared and not (writers[0][2] == "Loader" and (ROOT / "src/Loader/loader_win32.c").is_file())):
                errors.append(key + ": chain requires one active declared arbitrator")
    return errors


def main():
    try:
        runtime = json.loads((ROOT / "runtime/current.json").read_text(encoding="utf-8"))
        registry = json.loads((ROOT / "runtime/module_registry.json").read_text(encoding="utf-8"))
        errors = validate(runtime, registry)
        if errors:
            for error in errors:
                print("MODULE_REGISTRY: FAIL", error)
            return 1
        print("MODULE_REGISTRY: PASS; active DLLs", len(registry["modules"]))
        return 0
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print("MODULE_REGISTRY: FAIL", exc)
        return 1


if __name__ == "__main__":
    sys.exit(main())
