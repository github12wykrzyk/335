"""Validate reusable AI experiment routing/evidence; live GitHub HEAD remains authoritative."""
from __future__ import annotations
import argparse
import json
import os
import re
import sys
from manifest_common import ROOT, load_json

SHA = re.compile(r"^[0-9a-f]{40}$")
BRANCH = re.compile(r"^(work|parallel|feature/[a-z0-9][a-z0-9._/-]*)$")
STATES = {"in_progress", "testing", "accepted", "rejected", "blocked", "archived"}

def validate(ledger):
    errors = []
    if ledger.get("schema_version") != 1 or ledger.get("repository") != "github12wykrzyk/335":
        errors.append("experiment ledger must target github12wykrzyk/335 schema 1")
    experiments = ledger.get("experiments")
    if not isinstance(experiments, list):
        return errors + ["experiments must be a list"]
    ids = set()
    for entry in experiments:
        if not isinstance(entry, dict):
            errors.append("invalid experiment entry")
            continue
        eid, branch = entry.get("id"), entry.get("branch")
        if not isinstance(eid, str) or not re.fullmatch(r"[a-z0-9][a-z0-9._-]*", eid):
            errors.append("invalid experiment id")
            continue
        if eid in ids:
            errors.append(eid + ": duplicate experiment")
        ids.add(eid)
        if not isinstance(branch, str) or not BRANCH.fullmatch(branch):
            errors.append(eid + ": invalid development branch")
        if entry.get("status") not in STATES:
            errors.append(eid + ": invalid lifecycle status")
        for key in ("goal", "modules", "dependencies", "shared_resources"):
            value = entry.get(key)
            if key == "goal":
                if not isinstance(value, str) or not value.strip():
                    errors.append(eid + ": missing experiment goal")
            elif not isinstance(value, list) or not all(
                    isinstance(v, str) and v.strip() for v in value):
                errors.append(eid + ": invalid " + key)
        for key in ("observed_head", "verified_commit"):
            value = entry.get(key)
            if value is not None and (not isinstance(value, str) or not SHA.fullmatch(value)):
                errors.append(eid + ": invalid " + key)
        tests = entry.get("tests")
        if not isinstance(tests, list):
            errors.append(eid + ": tests must be an array")
        else:
            for test in tests:
                if not isinstance(test, dict) or not isinstance(
                        test.get("sha"), str) or not SHA.fullmatch(test["sha"]) or (
                        test.get("result") not in {"pass", "fail", "inconclusive"}):
                    errors.append(eid + ": test evidence requires exact SHA and result")
        package = entry.get("package")
        if package is not None and (
                not isinstance(package, dict) or
                not isinstance(package.get("sha"), str) or
                not SHA.fullmatch(package["sha"]) or
                not isinstance(package.get("id"), str) or not package["id"]):
            errors.append(eid + ": package evidence requires exact SHA and ID")
        if (entry.get("status") == "accepted" and (
                not entry.get("verified_commit") or
                not isinstance(package, dict) or
                package.get("sha") != entry.get("verified_commit") or
                not any(isinstance(test, dict) and
                        test.get("sha") == entry.get("verified_commit") and
                        test.get("result") == "pass" for test in
                        (tests if isinstance(tests, list) else [])))):
            errors.append(eid + ": acceptance requires same-SHA verified package and passing game test")
    return errors

def route(ledger, module, requested=None):
    # Does NOT create branches or assert refs exist: check GitHub before any write.
    if requested:
        if not BRANCH.fullmatch(requested):
            raise ValueError("explicit requested branch is not a valid development branch")
        return {"branch": requested, "reason": "explicit-user-branch",
                "requires_live_head_check": True}
    matching = [e for e in ledger["experiments"] if module in e.get("modules", [])
                and e.get("status") in ("in_progress", "testing", "blocked")]
    if len(matching) == 1:
        return {"branch": matching[0]["branch"], "reason": "existing-module-owner",
                "experiment": matching[0]["id"], "requires_live_head_check": True}
    if matching:
        return {"branch": None, "reason": "ambiguous-owners-review-hooks-and-dependencies",
                "experiments": [e["id"] for e in matching], "requires_live_head_check": True}
    name = re.sub(r"[^a-z0-9-]+", "-", module.lower()).strip("-")[:48] or "module"
    return {"branch": "feature/" + name, "base": "work",
            "reason": "new-isolated-experiment-check-live-head-before-creation",
            "requires_live_head_check": True}


def scope_errors(ledger, runtime, branch):
    """Fail closed when a feature run would package unrelated active DLLs."""
    if not isinstance(branch, str) or not branch:
        return ["missing branch identity"]
    if not branch.startswith("feature/"):
        return []
    matches = [e for e in ledger.get("experiments", [])
               if isinstance(e, dict) and e.get("branch") == branch]
    if len(matches) != 1:
        return [branch + ": exactly one registered experiment is required before a game package"]
    experiment = matches[0]
    if experiment.get("status") not in ("in_progress", "testing", "accepted"):
        return [branch + ": experiment status does not permit packaging"]
    modules = experiment.get("modules")
    if not isinstance(modules, list) or not modules:
        return [branch + ": experiment has no declared game modules"]
    active = {row.get("component") for row in runtime.get("files", [])
              if isinstance(row, dict) and row.get("kind") == "dll"}
    missing = sorted(set(modules) - active)
    if missing:
        return [branch + ": INCOMPLETE_EXPERIMENT; missing active DLL components: " +
                ", ".join(missing)]
    return []

def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="command", required=True)
    sub.add_parser("validate")
    sub.add_parser("verify-scope")
    route_cmd = sub.add_parser("route")
    route_cmd.add_argument("--module", required=True)
    route_cmd.add_argument("--branch")
    args = ap.parse_args()
    try:
        ledger = load_json(ROOT / "runtime/ai_experiments.json")
        errors = validate(ledger)
        if errors:
            for error in errors:
                print("EXPERIMENT_LEDGER: FAIL", error)
            return 1
        if args.command == "validate":
            print("EXPERIMENT_LEDGER: PASS; experiments", len(ledger["experiments"]))
        elif args.command == "verify-scope":
            branch = os.environ.get("GITHUB_REF_NAME", "")
            runtime = load_json(ROOT / "runtime/current.json")
            errors = scope_errors(ledger, runtime, branch)
            if errors:
                for error in errors:
                    print("EXPERIMENT_SCOPE: FAIL", error)
                return 1
            print("EXPERIMENT_SCOPE: PASS; branch", branch)
        else:
            print(json.dumps(route(ledger, args.module, args.branch), indent=2))
        return 0
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print("EXPERIMENT_LEDGER: FAIL", exc)
        return 1

if __name__ == "__main__":
    sys.exit(main())
