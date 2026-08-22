# Scalable Kubernetes for DevSecOps

A course on **security as code on Kubernetes**: hardening workloads,
containing compromises, and — the DevSecOps part — turning every security
decision into something a **pipeline enforces automatically**. The course
text is [DOCUMENTATION.md](DOCUMENTATION.md).

The through-line: *a security rule in a wiki is a suggestion; a rule that
fails the build is a control.* Every rule in this course exists at three
enforcement points — CI (blocks the merge), admission (blocks the apply),
runtime (blunts what slips through).

## The artifacts

| Path | What it is |
|---|---|
| `manifests/` | A fully hardened workload stack, chapter-numbered and commented: PSA-restricted namespace + quota, least-privilege RBAC (no token automount), a Deployment passing the restricted profile (non-root, seccomp, read-only rootfs, dropped caps, secret as file mount), default-deny NetworkPolicies, and native **CEL admission policies** (`ValidatingAdmissionPolicy`) |
| `examples/insecure-deployment.yaml` | The counter-example: *schema-valid* Kubernetes with 15 numbered security violations — proof that valid YAML ≠ acceptable YAML |
| `tools/audit.py` | A readable mini-Kyverno: 15 policy-as-code rules (R01–R15) enforced against any manifests; exits 1 on violations so CI blocks the merge |
| `Makefile` | The pipeline gate (runs anywhere) + the live kind runbook |

## The pipeline gate (runs anywhere — no Docker, no cluster)

```bash
make validate    # kubeconform: schema-validate all manifests (incl. the bad one!)
make audit       # policy audit of the hardened manifests -> PASS
make audit-bad   # the same audit vs the insecure example -> catches all 15
make gate        # validate + audit: the full CI gate
```

`make audit-bad` output (abridged):

```
FAIL  Deployment/legacy-app: 15 violation(s)
      R01  hostNetwork: true shares the node's network namespace
      R06  [app] image 'nginx:latest' is not pinned
      R07  [app] privileged: true — full kernel access
      ...
audit correctly REJECTED the insecure example (exit 1 blocks CI)
```

## The live demo (any machine with Docker + kind + kubectl)

```bash
make cluster deploy status   # hardened stack up
make psa-test                # apply the insecure example: Pod Security
                             # Admission AND the CEL policy reject it live
make trivy                   # CVE-scan the app image (needs trivy)
make kube-bench              # run the CIS benchmark as a Job
make down
```

## Chapters

1. What DevSecOps means on Kubernetes — shift left, rules as code, defense in depth
2. Supply chain — minimal images, pinning (tags → digests), scanning, SBOM/signing
3. Pod hardening & Pod Security Admission — every field and the attack it blunts
4. RBAC & workload identity — dedicated SAs, `automountServiceAccountToken: false`, named-resource grants
5. Network policy — default deny, earn each flow, egress as exfil resistance
6. Secrets, done honestly — base64 ≠ encryption, files not env vars, external managers
7. Admission control & policy as code — native CEL policies, Kyverno/Gatekeeper, warn→enforce
8. Runtime security — immutability as detection substrate, Falco, audit logs, drift
9. Scaling security — namespaces as policy units, quotas as blast radius, sandboxed runtimes
10. The pipeline — the blocking gate, GitOps pull-not-push, no god-credentials in CI
11. Compliance — CIS benchmark/kube-bench, NSA/CISA guide, continuous evidence
12. Incident response — isolate/preserve/trace/eradicate/feed-back, plus the one-page checklist

## Requirements

- Pipeline gate: `python3` + PyYAML, `kubeconform` ([releases](https://github.com/yannh/kubeconform/releases))
- Live demo: Docker, kind, kubectl (and optionally trivy)
