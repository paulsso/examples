#!/usr/bin/env python3
# ============================================================================
#  audit.py — a policy-as-code auditor for workload manifests
# ============================================================================
#
#  A miniature, readable version of what Kyverno / OPA Gatekeeper /
#  Checkov / kube-score do: load Kubernetes manifests and enforce the
#  course's hardening rules against every workload. Schema validation
#  (kubeconform) checks that YAML has the right SHAPE; this checks that
#  it has the right POLICY — the two together are the CI gate.
#
#  Exit code 0: everything passes (pipeline proceeds).
#  Exit code 1: violations found (pipeline blocks the merge).
#
#  Usage:  python3 tools/audit.py <dir-or-file> [...]
#
#  Each rule maps to a chapter of DOCUMENTATION.md; R-numbers appear in
#  the report.
# ============================================================================

import sys
import glob
import os

import yaml

WORKLOAD_KINDS = {"Deployment", "StatefulSet", "DaemonSet", "Job"}


def get(obj, *path, default=None):
    """Safe nested lookup: get(d, 'a', 'b') -> d['a']['b'] or default."""
    for key in path:
        if not isinstance(obj, dict) or key not in obj:
            return default
        obj = obj[key]
    return obj


def audit_workload(doc):
    """Returns a list of (rule_id, message) violations for one workload."""
    violations = []
    pod = get(doc, "spec", "template", "spec", default={})
    pod_sc = get(pod, "securityContext", default={})
    containers = get(pod, "containers", default=[]) or []

    def flag(rule, message):
        violations.append((rule, message))

    # ---- pod-level rules ---------------------------------------------------
    if get(pod, "hostNetwork", default=False):
        flag("R01", "hostNetwork: true shares the node's network namespace")
    if get(pod, "hostPID", default=False) or get(pod, "hostIPC", default=False):
        flag("R02", "hostPID/hostIPC break process isolation from the node")
    for volume in get(pod, "volumes", default=[]) or []:
        if "hostPath" in volume:
            flag("R03", f"hostPath volume '{volume.get('name')}' mounts the "
                        f"node's filesystem")
    if get(pod, "automountServiceAccountToken", default=True):
        flag("R04", "API token auto-mounted; set "
                    "automountServiceAccountToken: false unless the app "
                    "calls the API")
    if not get(pod, "serviceAccountName"):
        flag("R05", "no dedicated ServiceAccount (runs as 'default')")

    pod_seccomp = get(pod_sc, "seccompProfile", "type")
    pod_nonroot = get(pod_sc, "runAsNonRoot", default=False)

    # ---- container-level rules ----------------------------------------------
    for c in containers:
        name = c.get("name", "?")
        sc = get(c, "securityContext", default={})

        image = c.get("image", "")
        if ":" not in image or image.endswith(":latest"):
            flag("R06", f"[{name}] image '{image}' is not pinned "
                        f"(missing tag or :latest)")

        if get(sc, "privileged", default=False):
            flag("R07", f"[{name}] privileged: true — full kernel access")

        if not (pod_nonroot or get(sc, "runAsNonRoot", default=False)):
            flag("R08", f"[{name}] does not require runAsNonRoot")

        if get(sc, "allowPrivilegeEscalation", default=True):
            flag("R09", f"[{name}] allowPrivilegeEscalation not disabled")

        drops = [d.upper() for d in get(sc, "capabilities", "drop",
                                        default=[]) or []]
        if "ALL" not in drops:
            flag("R10", f"[{name}] does not drop ALL capabilities")

        if not get(sc, "readOnlyRootFilesystem", default=False):
            flag("R11", f"[{name}] root filesystem is writable")

        if pod_seccomp != "RuntimeDefault" and \
           get(sc, "seccompProfile", "type") != "RuntimeDefault":
            flag("R12", f"[{name}] no seccomp profile (RuntimeDefault)")

        if not (get(c, "resources", "requests") and
                get(c, "resources", "limits")):
            flag("R13", f"[{name}] resource requests/limits not set")

        if doc.get("kind") != "Job":
            if not get(c, "readinessProbe"):
                flag("R14", f"[{name}] no readiness probe")
            if not get(c, "livenessProbe"):
                flag("R15", f"[{name}] no liveness probe")

    return violations


def main(paths):
    files = []
    for path in paths:
        if os.path.isdir(path):
            files.extend(sorted(glob.glob(os.path.join(path, "*.yaml"))))
        else:
            files.append(path)

    total_workloads = 0
    total_violations = 0

    for filename in files:
        with open(filename) as f:
            docs = [d for d in yaml.safe_load_all(f) if d]
        for doc in docs:
            if doc.get("kind") not in WORKLOAD_KINDS:
                continue
            total_workloads += 1
            name = f"{doc['kind']}/{get(doc, 'metadata', 'name', default='?')}"
            violations = audit_workload(doc)
            if violations:
                print(f"FAIL  {name}  ({filename}): "
                      f"{len(violations)} violation(s)")
                for rule, message in violations:
                    print(f"      {rule}  {message}")
                total_violations += len(violations)
            else:
                print(f"PASS  {name}  ({filename})")

    print(f"\naudited {total_workloads} workload(s): "
          f"{total_violations} violation(s)")
    return 1 if total_violations else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:] or ["manifests"]))
