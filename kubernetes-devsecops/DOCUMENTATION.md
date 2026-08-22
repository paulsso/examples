# Scalable Kubernetes for DevSecOps — Course Text

This course teaches **security as code on Kubernetes, at scale**: how to
harden workloads, contain compromises, and — the DevSecOps part — turn
every one of those decisions into something a **pipeline enforces
automatically**, so security scales with the fleet instead of with the
security team's headcount.

The through-line: *a security rule that lives in a wiki is a suggestion;
a rule that fails the build is a control.* Every chapter therefore ends in
one of three enforcement points, all present in this directory:

| Enforcement point | Artifact here | When it fires |
|---|---|---|
| **CI pipeline** | `tools/audit.py` + kubeconform (`make gate`) | before merge |
| **API server admission** | PSA labels + `ValidatingAdmissionPolicy` (CEL) | at `kubectl apply` |
| **Runtime** | read-only rootfs, seccomp, NetworkPolicies, quotas | while running |

```bash
make gate        # the CI gate: schema validation + policy audit (runs anywhere)
make audit-bad   # watch the auditor catch all 15 violations in the
                 # deliberately insecure example
```

The counter-example matters: `examples/insecure-deployment.yaml` is
*schema-valid* Kubernetes that kubeconform happily accepts — and a
security disaster. The gap between "valid YAML" and "acceptable YAML" is
the entire reason the policy layer exists.

---

## Table of Contents

1. [What DevSecOps means on Kubernetes](#chapter-1--what-devsecops-means-on-kubernetes)
2. [Supply chain: images you can trust](#chapter-2--supply-chain-images-you-can-trust)
3. [Pod hardening & Pod Security Admission](#chapter-3--pod-hardening--pod-security-admission)
4. [RBAC & workload identity](#chapter-4--rbac--workload-identity)
5. [Network policy: default deny](#chapter-5--network-policy-default-deny)
6. [Secrets, done honestly](#chapter-6--secrets-done-honestly)
7. [Admission control & policy as code](#chapter-7--admission-control--policy-as-code)
8. [Runtime security](#chapter-8--runtime-security)
9. [Scaling security: multi-tenancy & blast radius](#chapter-9--scaling-security-multi-tenancy--blast-radius)
10. [The pipeline: CI/CD as the enforcement point](#chapter-10--the-pipeline-cicd-as-the-enforcement-point)
11. [Compliance, benchmarks, audit](#chapter-11--compliance-benchmarks-audit)
12. [Incident response & the checklist](#chapter-12--incident-response--the-checklist)

---

## Chapter 1 — What DevSecOps Means on Kubernetes

Traditional security reviews happen *after* development — a gate at the
end, staffed by humans, that either rubber-stamps or blocks. That model
collapses at Kubernetes scale: hundreds of services deploying daily
cannot wait for manual review, and manual review cannot catch a
`privileged: true` buried in the 40th manifest of the day.

DevSecOps inverts the model — **shift left, automate, and make the safe
path the easy path**:

- Security requirements become **code** (policies, admission rules,
  audit scripts) that runs on every commit, exactly like tests.
- The **pipeline** becomes the control point: what doesn't pass the gate
  doesn't merge; what doesn't pass admission doesn't run.
- Developers get **fast, specific feedback** ("R06: image not pinned")
  instead of a review meeting three weeks later.
- Security engineers write and tune the *rules* instead of reviewing the
  *instances* — that is what lets 5 people secure 500 services.

Kubernetes is unusually well suited to this because *everything is a
declarative object*: if the entire system state is YAML, then the entire
system state can be linted, diffed, policy-checked and audited like any
other code. This course's `make gate` is that idea reduced to its
essence.

Defense in depth structures the whole course: the same mistake
(say, a privileged container) is caught **three times** — by the CI
audit (Ch10), by admission control (Ch3, Ch7), and blunted at runtime if
it somehow lands (Ch8). Any single layer failing is survivable.

---

## Chapter 2 — Supply Chain: Images You Can Trust

Your cluster runs whatever bytes the image registry serves. The supply
chain — base image, build, registry, pull — is the attack surface *before
the cluster even starts*, and modern incidents (typosquatted packages,
poisoned base images, compromised registries) live exactly there.

The disciplines, in pipeline order:

- **Minimal base images.** `03-deployment.yaml` uses an unprivileged
  nginx on alpine; the stricter end is distroless/scratch. Every binary
  in the image is attack surface and CVE-surface; the C course's static
  binaries make the point — a scratch image containing one binary has
  almost nothing to exploit.
- **Pin versions — never `:latest`.** An unpinned image changes under
  you on every pull: unreproducible rollbacks, unreviewable diffs.
  Production pins the **digest**
  (`image@sha256:…`), which is immune even to a tag being re-pushed by
  an attacker with registry access. Enforced twice here: audit rule R06
  and CEL rule 1.
- **Scan continuously.** `make trivy` scans the app image for known
  CVEs. The nuance: scanning is *necessary but reactive* — it catches
  known-bad, not unknown-bad; minimal images shrink what there is to
  scan at all. Scan in CI (block on critical), and *rescan* deployed
  images (new CVEs are published against old layers daily).
- **SBOMs and signing** close the loop: an SBOM (syft) records what's
  inside; a signature (cosign/sigstore) proves who built it; an
  admission rule ("only images signed by our CI") makes provenance
  enforceable. That is the supply-chain endgame: the cluster refuses
  bytes that didn't come from your pipeline.

---

## Chapter 3 — Pod Hardening & Pod Security Admission

A container is a Linux process with namespaces and cgroups — not a VM.
The kernel is shared; hardening is about making the process as boring as
possible for an attacker who lands in it. `03-deployment.yaml` carries
the full kit, each field annotated with the attack it blunts:

- **`runAsNonRoot` + numeric uid** — root in a container is root against
  the shared kernel; a container-escape CVE turns uid 0 into node
  compromise. This is the single highest-value field.
- **`seccompProfile: RuntimeDefault`** — filters the syscall table;
  escape exploits overwhelmingly need the exotic syscalls (bpf, keyctl,
  mount…) that the default profile blocks.
- **`capabilities: drop ALL`** — capabilities are root's powers sliced
  thin; a web service needs none of them.
- **`allowPrivilegeEscalation: false`** — neuters setuid binaries.
- **`readOnlyRootFilesystem: true`** — the attacker cannot drop tools,
  modify the binary, or persist. Legitimate scratch paths become
  explicit, size-bounded `emptyDir` mounts (`/tmp`, the nginx cache) —
  the manifest shows the pattern.

**Pod Security Admission** (`00-namespace.yaml`) is the cluster-side
enforcement of all of the above: namespace labels that make the API
server reject pods violating the **restricted** profile — with
`enforce`, `audit`, and `warn` modes enabling gradual rollout, and the
profile *version pinned* so a cluster upgrade can't silently change the
rules. PSA replaced PodSecurityPolicy (removed in 1.25); it is the
built-in baseline, with Chapter 7's policy engines layered on for
anything finer.

---

## Chapter 4 — RBAC & Workload Identity

Kubernetes RBAC answers "who may do what to which objects". The
workload-side hygiene (`01-rbac.yaml`) is where most real-world gaps
live:

- **A dedicated ServiceAccount per workload** — `default` shared by
  everything makes audit logs meaningless and privilege creep invisible.
- **`automountServiceAccountToken: false`** — the underrated one. Every
  pod gets an API token *by default*, so every RCE in any container is
  an authenticated API client. If the app doesn't call the Kubernetes
  API (most web services don't), there should be no token to steal.
  Enforced by audit rule R04.
- **Minimal grants**: the example Role allows `get` on *one named
  ConfigMap*. Not `list`, not `watch`, not `*`, not secrets. RBAC has no
  deny rules — everything not granted is denied — so small grants are
  the whole game.

The anti-patterns to hunt in real clusters: `cluster-admin`
RoleBindings to service accounts ("it fixed the permission error"),
wildcard verbs/resources, and grants to `system:authenticated`. Tools
like `kubectl auth can-i --list --as=system:serviceaccount:ns:sa` and
RBAC auditors make the review mechanical — pipeline material (Ch10).

---

## Chapter 5 — Network Policy: Default Deny

Without NetworkPolicies, the cluster is flat: any pod can reach any pod,
plus (in clouds) the node metadata endpoint. One compromised container
gets free lateral movement to every database and internal admin API in
the fleet. Most real-world Kubernetes breach writeups have a lateral
movement step that a default-deny would have stopped.

`04-networkpolicy.yaml` shows the pattern that scales:

1. **`default-deny-all`** — empty `podSelector`, both directions, whole
   namespace. This is the ground state; everything else is an earned
   exception.
2. **`allow-dns`** — egress to kube-dns on port 53 only (or nothing
   with a hostname works). Note the precision: to *those pods*, on
   *that port* — not "allow UDP".
3. **`allow-web-from-ingress`** — ingress to the web pods *only* from
   the ingress controller's namespace, *only* on 8080. Other pods in
   the same namespace stay blocked: default deny is also
   *intra*-namespace segmentation.

Operational notes that matter at scale: policies are enforced by the
CNI (Calico, Cilium — not all CNIs enforce them; kind's default doesn't
without swapping CNI); policies are *additive* (allows union, no
precedence puzzles); and egress control is the underrated half —
blocking arbitrary egress is what turns "attacker exfiltrates the
database" into "attacker is stuck".

---

## Chapter 6 — Secrets, Done Honestly

The uncomfortable truths, then the mitigations (`02-config-secret.yaml`
and the Deployment's volume mount):

1. **base64 is encoding.** Anyone with `get secret` — RBAC is the first
   defense — reads the value. The course's reader Role deliberately
   *cannot* read secrets.
2. **etcd stores secrets plaintext by default.** Enable
   `EncryptionConfiguration` (KMS-backed in clouds) or an etcd backup
   is your secrets.
3. **Mount as files, not env vars.** Environment variables leak: into
   `kubectl describe pod`, crash reports, child processes, and every
   debug endpoint that dumps the environment. The Deployment mounts the
   token at `/etc/secrets`, read-only. (Audit-friendly bonus: file
   mounts update on rotation; env vars need a restart.)
4. **At scale, move the source of truth outside the cluster**:
   external-secrets-operator or CSI secret-store drivers syncing from
   Vault / cloud secret managers — with short-lived, auto-rotated
   credentials, so a leaked secret has a shelf life.

The pipeline angle: secrets *in Git* is the classic failure. GitOps
repos get secret-scanners (gitleaks/trufflehog) in the same CI gate as
everything else, and sealed-secrets/SOPS make encrypted-at-rest-in-Git
possible when it's unavoidable.

---

## Chapter 7 — Admission Control & Policy as Code

Admission is the API server's checkpoint: every write passes through it,
regardless of who sent it — a developer, CI, or an attacker with a
stolen kubeconfig. That last property is why admission is strictly
stronger than CI-only enforcement.

`05-admission-policy.yaml` uses **ValidatingAdmissionPolicy** — built
into Kubernetes (GA 1.30), no webhook infrastructure, rules written in
**CEL**:

```
object.spec.template.spec.containers.all(c,
    c.image.contains(':') && !c.image.endsWith(':latest'))
```

Three rules encode the non-negotiables (pinned images, no privileged,
non-root), and the **binding** scopes enforcement by namespace label —
the same gradual-rollout thinking as PSA's warn/audit/enforce ladder:
warn teams first, enforce when the fleet is clean.

Where the ecosystem engines fit: **Kyverno** (policies as YAML,
mutation + generation — e.g. auto-inject NetworkPolicies into new
namespaces) and **OPA Gatekeeper** (Rego, cross-object context) add
what CEL policies can't do; `tools/audit.py` is this course's readable
miniature of their validation half, and running *the same rules* in CI
(fast feedback) and at admission (actual enforcement) is the mature
setup — developers hear "no" from the linter in seconds, and the
cluster still says "no" to anyone who bypassed the linter.

---

## Chapter 8 — Runtime Security

Everything so far is *prevention*. Runtime security assumes prevention
eventually fails and asks: how quickly is a compromise noticed, and how
little can it do?

- **Immutability as detection substrate** (already deployed): with
  read-only rootfs, dropped capabilities, seccomp, and no API token, a
  compromised web pod can't install tools, can't persist, can't call
  the API, can't reach non-allowed networks (Ch5). The attacker's
  every next move is *forced to be loud*.
- **Behavioral detection**: Falco (or Tetragon/KubeArmor — eBPF-based)
  watches syscalls against rules like "shell spawned in a container",
  "write below /etc", "outbound connection from a pod that never makes
  them". The quieter the baseline (immutable pods), the sharper these
  alerts.
- **Kubernetes audit logs**: the API server can log every request —
  who created what, who read which secret, which service account got
  used from where. At scale this feeds the SIEM, and unusual API
  patterns (a workload SA suddenly listing secrets) are among the
  highest-signal alerts that exist.
- **Drift**: GitOps (Ch10) turns "cluster state differs from Git" into
  an alert in itself — anything applied by hand, benign or malicious,
  surfaces.

---

## Chapter 9 — Scaling Security: Multi-Tenancy & Blast Radius

Security architecture at fleet scale is blast-radius engineering — the
same instinct as the availability chapters of the sibling Kubernetes
course, pointed at compromise instead of failure:

- **Namespaces as the unit of policy**: PSA labels, quotas
  (`00-namespace.yaml` — a hijacked namespace cannot fork-bomb the
  cluster into 10,000 cryptominers), LimitRanges, NetworkPolicies and
  RBAC are all namespace-scoped. A tenant = a namespace (or a set),
  and onboarding a team = stamping out the policy bundle — which is
  exactly what Kyverno *generate* policies or GitOps templates do.
- **Node-level isolation** for hostile-tenant or crown-jewel cases:
  taints/nodeSelectors segregate workloads onto node pools; gVisor /
  Kata (RuntimeClass) put a kernel boundary around truly untrusted
  code — the answer to "containers share a kernel" when that's not
  acceptable.
- **Cluster-level isolation** is the bluntest and most effective knife:
  prod separate from dev, PCI separate from everything. Many small
  clusters with identical GitOps-managed policy beat one giant
  hand-tuned one.
- **The control plane scales too**: admission policies are evaluated on
  every write (CEL is cheap, webhooks are a latency/availability
  dependency — set failurePolicy consciously); audit log volume needs
  filtering before the SIEM; and secrets/API traffic from thousands of
  pods is why `automountServiceAccountToken: false` is also a
  *performance* optimization.

---

## Chapter 10 — The Pipeline: CI/CD as the Enforcement Point

The DevSecOps assembly line, stage by stage — each with the tool
category and this course's runnable stand-in:

| Stage | Checks | Here |
|---|---|---|
| Pre-commit / PR | schema validity | `make validate` (kubeconform) |
| PR | policy audit | `make audit` (`tools/audit.py`) |
| PR | secret scanning | gitleaks/trufflehog (pointer) |
| Build | image scan, SBOM, sign | trivy/syft/cosign (`make trivy`) |
| Deploy | GitOps pull, not push | Argo CD / Flux (pattern below) |
| Admission | PSA + CEL policies | live in `manifests/` |
| Runtime | Falco, audit logs | Ch8 |

Two structural decisions carry most of the value:

- **The gate blocks.** `audit.py` exits 1 on violations; `make
  audit-bad` demonstrates a merge being refused. Warnings that don't
  block are decoration — the counter-example file is *schema-valid*,
  remember; only the policy layer stops it.
- **GitOps, pull not push.** CI never holds cluster credentials;
  an in-cluster agent (Argo CD, Flux) pulls from Git and reconciles.
  Consequences: the cluster's state is reviewable history, rollback is
  `git revert`, drift is detectable, and there is no god-credential
  sitting in the CI system waiting to be stolen — CI systems are among
  the most-attacked infrastructure there is.

---

## Chapter 11 — Compliance, Benchmarks, Audit

Turning "we're secure" into evidence:

- **CIS Kubernetes Benchmark** — the standard hardening checklist for
  the control plane and nodes; `make kube-bench` runs Aqua's kube-bench
  as a Job and prints pass/fail per control. Managed clusters (EKS,
  GKE, AKS) shift many controls to the provider — run the matching
  benchmark variant.
- **NSA/CISA Kubernetes Hardening Guide** — the readable companion;
  this course's manifests implement its workload-security chapter
  almost point for point (non-root, immutable fs, network policy,
  RBAC minimalism, audit logging).
- **Continuous, not annual**: the same checks that gate PRs run
  nightly against the *live* cluster (policy engines report
  compliance percentages per namespace; kube-bench runs on a
  schedule). Compliance drift becomes a metric, and the audit
  artifact for the actual auditors is generated, not hand-assembled.

---

## Chapter 12 — Incident Response & the Checklist

When runtime detection fires, the Kubernetes-native IR sequence:

1. **Isolate, don't delete.** A one-line NetworkPolicy selecting the
   pod cuts it off from everything (deny all both directions). Cordon
   the node if node compromise is possible.
2. **Preserve**: `kubectl get pod -o yaml`, logs, `kubectl debug`
   with an ephemeral forensics container (the pod's own filesystem is
   read-only — which *preserved the evidence*), node-level container
   checkpoint if available.
3. **Trace with the audit log**: what did this pod's service account
   touch? (This is where per-workload SAs from Ch4 pay off — `default`
   would mean "could be anyone".)
4. **Eradicate via the pipeline**: fix the image/manifest in Git, let
   GitOps roll the fleet; rotate every secret the workload could read
   (Ch6's short-lived credentials shrink this step).
5. **Feed back**: every incident becomes a new audit rule / CEL policy
   / Falco rule — the security system *learns* through the same
   code-review loop as everything else. That loop is DevSecOps.

### The checklist (one page)

**Supply chain**: minimal base images · pinned tags (digests in prod) ·
CI scanning + rescan of deployed images · SBOM + signature verification
at admission.

**Workload**: non-root · seccomp RuntimeDefault · drop ALL caps · no
privilege escalation · read-only rootfs with bounded scratch mounts ·
requests/limits · probes.

**Identity & access**: dedicated SA per workload · no token automount ·
named-resource Roles · no wildcard grants · audit `can-i --list`
regularly.

**Network**: default-deny both directions per namespace · explicit DNS
egress · scoped ingress allows · egress control for exfil resistance.

**Secrets**: RBAC first · etcd encryption · file mounts not env ·
external manager + rotation at scale · secret scanning in CI.

**Enforcement**: PSA restricted (pinned version) · CEL/Kyverno policies
warn→enforce · the CI gate blocks merges (`make gate`).

**Detect & respond**: Falco/eBPF rules · API audit logs to SIEM ·
GitOps drift alerts · rehearsed isolate/preserve/rotate playbook.

## Suggested Course Progression

| Stage | Chapters | Exercises to assign |
|---|---|---|
| Foundations | 1–3 | Harden a given insecure Deployment until `make audit` passes; break one PSA rule and read the API server's rejection |
| Access | 4–6 | Write the minimal Role for an app that lists its own pods; move a secret from env to file and prove `kubectl describe` no longer shows it |
| Enforcement | 7, 10 | Add a CEL rule requiring resource limits; add a new R-rule to audit.py with a test in examples/ |
| Operations | 8, 9, 11, 12 | Write the isolation NetworkPolicy for a "compromised" pod; run kube-bench on kind and triage 3 findings; table-top the IR playbook |
