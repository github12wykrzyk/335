"""End-to-end AutoPickPocket TEST acceptance: never certify an isolated DLL as playable.

This source gate verifies delivery architecture. The *separate* native
Windows PE32/SHA256 build and FINAL_PACKAGE gate remain mandatory, and
neither proves in-game operation.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PP = "AutoPickPocket"
PP_DLL = "AutoPickPocket335.dll"
EXPECTED_BRANCH = "feature/autopickpocket-12340"


def inspect(root: Path) -> list[str]:
    blockers: list[str] = []
    def get(path: str) -> str:
        p = root / path
        if not p.is_file():
            blockers.append(f"MISSING_SOURCE: {path}")
            return ""
        return p.read_text(encoding="utf-8")
    try:
        manifest = json.loads(get("runtime/current.json"))
        registry = json.loads(get("runtime/module_registry.json"))
    except (ValueError, TypeError) as exc:
        return blockers + [f"INVALID_RUNTIME: {exc}"]

    files = manifest.get("files", [])
    modules = registry.get("modules", [])
    active = [r for r in files if isinstance(r, dict) and r.get("component") == PP]
    if len(active) != 1 or active[0].get("kind") != "dll" or (
        active[0].get("path") != "runtime/" + PP_DLL
    ):
        blockers.append("ACTIVE_RUNTIME: register exactly one genuine runtime/" + PP_DLL)
    else:
        p = root / "runtime" / PP_DLL
        if not p.is_file():
            blockers.append("EXACT_BINARY: register and commit the built PP32 x86 DLL")
        if not active[0].get("sha256"):
            blockers.append("EXACT_SHA: add the actual runtime DLL SHA256")
    owners = [m for m in modules if isinstance(m, dict) and m.get("component") == PP]
    if len(owners) != 1 or not owners[0].get("build") or not owners[0].get("resources"):
        blockers.append("MODULE_REGISTRY: declare AutoPickPocket x86 build/dependencies/resources")

    # An adapter with no concrete policy implementations will bind to nothing.
    policy = get("src/AutoPickPocket/autopickpocket_game_policies.c")
    for key in ("eligible_npc", "spell_usable", "begin_attempt",
                "cast_result", "world_token"):
        if policy and key not in policy:
            blockers.append("GAME_POLICY: implement " + key + " on pinned 12340 client")
    loader = get("src/Loader12340/Wow335Loader.c")
    for symbol in ("PP335_BindOnGameThread", "PP335_TickOnGameThread",
                   "PP335_CommandOnGameThread"):
        if loader and symbol not in loader:
            blockers.append("GAME_THREAD_LOADER: integrate " + symbol)
    if loader and "AutoPickPocket335.dll" not in loader:
        blockers.append("GAME_THREAD_LOADER: load PP by manifest after preflight")
    host = get("src/AutoPickPocket/autopickpocket_win32_host.c")
    if host and "PP335_BindOnGameThread" not in host:
        blockers.append("NATIVE_ADAPTER: missing verified on-game-thread bind")

    updater = get("tools/updater/UpdaterAutoLootRuntimeFeature.cs")
    # The existing AutoLoot-only external launcher must NOT be selected
    # for a two-DLL package; it explicitly requires a one-element list.
    if updater and "listed.Length != 1" in updater:
        bridge = get("tools/updater/UpdaterAutoPickPocketRuntimeFeature.cs")
        if not bridge or PP_DLL not in bridge:
            blockers.append("UPDATER: replace AutoLoot-only launcher with manifest-verified multi-DLL launch")
    app = get("tools/updater/WoW335Updater.cs")
    if app and EXPECTED_BRANCH not in app and "registered_test_branches" not in app:
        blockers.append("UPDATER_CHANNEL: route exact PP TEST branch/SHA through normal updater")
    if len(active) == 1 and active[0].get("kind") == "dll":
        names = {r.get("component") for r in files if isinstance(r, dict)}
        if "AutoLoot" not in names:
            blockers.append("DEPENDENCY: include an approved AutoLoot in the same test compatibility set")
    return blockers


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--report", default="dist/autopickpocket_delivery.json")
    args = parser.parse_args()
    blockers = inspect(ROOT)
    report = {
        "schema_version": 1,
        "branch": EXPECTED_BRANCH,
        "integration_ready_for_native_package_gates": not blockers,
        "game_test_passed": False,
        "blockers": blockers,
    }
    path = ROOT / args.report
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if blockers:
        for blocker in blockers:
            print("AUTOPICKPOCKET_DELIVERY: BLOCKED", blocker)
        print("AUTOPICKPOCKET_DELIVERY: NO_GAME_PACKAGE; implement remaining integration")
        return 1
    print("AUTOPICKPOCKET_DELIVERY: integration sources/manifest declared; native build and package still REQUIRED")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
