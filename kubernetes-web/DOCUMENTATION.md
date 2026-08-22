# Scalable Kubernetes for Web Services — Course Text

This course teaches how to run **web services that scale** on Kubernetes:
from "why an orchestrator at all" to zero-downtime deploys, autoscaling,
and surviving node failures and cluster maintenance. It is built around
three artifacts you can read, validate, and run:

- **`app/`** — a tiny demo web service that implements the full contract
  Kubernetes expects from a well-behaved workload (probes, env config,
  graceful shutdown). Its contract is *testable without a cluster*:
  `make app-test` verifies all of it locally in seconds.
- **`manifests/`** — production-shaped YAML, numbered by chapter, where
  **every field is load-bearing** and commented. `make validate`
  schema-checks all of it offline (kubeconform) — the same check a CI
  pipeline would run.
- **`Makefile`** — the live-demo runbook for a local
  [kind](https://kind.sigs.k8s.io/) cluster: deploy, roll out, roll back,
  autoscale under load. Requires Docker on your machine.

```bash
make validate    # schema-validate every manifest (no cluster needed)
make app-test    # verify the app's Kubernetes contract (no Docker needed)

# live demo, on any machine with Docker + kind + kubectl:
make cluster build deploy status
make rollout-demo && make rollback
make metrics && make load && make watch     # autoscaling under load
make down
```

---

## Table of Contents

1. [Why Kubernetes for web services](#chapter-1--why-kubernetes-for-web-services)
2. [The workload contract: the demo app](#chapter-2--the-workload-contract-the-demo-app)
3. [Pods, ReplicaSets, Deployments](#chapter-3--pods-replicasets-deployments)
4. [Services: stable names for ephemeral pods](#chapter-4--services-stable-names-for-ephemeral-pods)
5. [Health probes: readiness vs liveness](#chapter-5--health-probes-readiness-vs-liveness)
6. [Resources: requests, limits, QoS](#chapter-6--resources-requests-limits-qos)
7. [Configuration and secrets](#chapter-7--configuration-and-secrets)
8. [Rolling updates and rollbacks](#chapter-8--rolling-updates-and-rollbacks)
9. [Autoscaling: the HPA](#chapter-9--autoscaling-the-hpa)
10. [Surviving disruption: PDBs, spread, graceful shutdown](#chapter-10--surviving-disruption)
11. [Ingress: traffic from the outside](#chapter-11--ingress-traffic-from-the-outside)
12. [The production playbook](#chapter-12--the-production-playbook)

---

## Chapter 1 — Why Kubernetes for Web Services

A single web server on a single machine fails in known ways: the process
crashes (nobody restarts it), the machine dies (the site is down), traffic
grows (one machine is the ceiling), and deploys mean downtime. Every team
that scales past one box reinvents the same four mechanisms:

1. **Supervision** — restart the process when it dies.
2. **Scheduling** — decide which machine runs which copy.
3. **Service discovery & load balancing** — route traffic to the copies
   that are alive *right now*.
4. **Orchestrated change** — replace version N with N+1 without dropping
   requests.

Kubernetes is those four mechanisms, generalized, driven by one idea:
**declarative reconciliation**. You declare the desired state ("3 replicas
of image X behind name Y"); controllers continuously compare it against
reality and fix the difference. A crashed pod isn't an *incident*, it's a
*discrepancy* — and the ReplicaSet controller resolves it within seconds,
at 3 a.m., without a human.

The cost is real: YAML volume, cluster operations, a learning curve. For a
single small app, a PaaS is often the better trade. Kubernetes pays off
when there are *many* services, *changing* load, or availability
requirements a single machine can't meet — which is exactly the "scalable
web services" brief of this course.

---

## Chapter 2 — The Workload Contract: the Demo App

Kubernetes can only manage what cooperates with it. `app/server.py` is
deliberately tiny (stdlib Python, ~100 lines of logic) but implements the
complete contract of a scalable workload:

- **Stateless.** No sessions, no local files. Any replica can serve any
  request, so replicas are interchangeable — the property every scaling
  mechanism in this course depends on. State belongs in databases and
  caches *behind* the service. (`/` returns the pod's hostname precisely
  so you can watch load balancing spread requests across replicas.)
- **Configured by environment** (12-factor): `GREETING`, `VERSION`,
  `READY_DELAY_SECONDS`, `DRAIN_SECONDS` — injected by the ConfigMap in
  Chapter 7. Same image everywhere; only config differs.
- **`/healthz`** — liveness: "is this process alive?" It checks *nothing
  else* (Chapter 5 explains why that's crucial).
- **`/readyz`** — readiness: "should I receive traffic right now?" It
  fails during a startup warmup window and fails again while draining
  after SIGTERM.
- **`/work`** — burns ~50 ms of CPU: measurable load for the autoscaling
  demo.
- **Graceful shutdown** — on SIGTERM: flip readiness to 503, keep serving
  in-flight requests for `DRAIN_SECONDS`, exit 0. This single behavior is
  what makes zero-downtime deploys (Ch8) and safe node drains (Ch10)
  possible.

`app/smoke_test.py` proves all of it locally — warmup 503s, the readiness
flip, env config, the drain sequence, clean exit — because the pod
lifecycle contract is ultimately just *process behavior*.

The `Dockerfile` is minimal and production-shaped: small alpine base
(fast pulls during scale-out), non-root user (matches the pod security
context), unbuffered stdout (so `kubectl logs` streams).

---

## Chapter 3 — Pods, ReplicaSets, Deployments

**Pod** — the unit of scheduling: one or more containers sharing network
and lifetime. Pods are *cattle*: they are created, killed, and replaced by
controllers. Nobody hand-creates pods for a web service.

**ReplicaSet** — the supervisor: keeps exactly N pods matching a label
selector alive. Kill one, it's replaced in seconds.

**Deployment** (`02-deployment.yaml`) — the object you actually manage:
it owns ReplicaSets and orchestrates transitions between them (Chapter 8).
Three fields carry the scalability story:

- `replicas: 3` — capacity AND availability: never run one replica of
  anything you care about. (Once the HPA arrives in Chapter 9, *it*
  effectively owns this number.)
- `selector.matchLabels` — **labels are the wiring** of Kubernetes: the
  Service (Ch4) and the PDB (Ch10) target pods by the same label. Labels,
  not names, are how objects find each other.
- The pod **template** — everything Chapters 5–7 and 10 add lives here.

The namespace + **ResourceQuota** (`00-namespace.yaml`) is the isolation
story: per-team/per-environment namespaces, with quotas capping the *sum*
of resources so one team's runaway autoscaler cannot starve the cluster.

---

## Chapter 4 — Services: Stable Names for Ephemeral Pods

Scaling means pods constantly appear and disappear; their IPs are
meaningless. The **Service** (`03-service.yaml`) provides:

- A **stable virtual IP and DNS name** —
  `web.web-course.svc.cluster.local` (or just `web` within the
  namespace — the load generator uses exactly that).
- **Load balancing** across all pods matching the selector, implemented
  by kube-proxy on every node (iptables/IPVS rules — connection-level
  spraying, not smart per-request balancing).
- **Membership driven by readiness**: only pods whose readiness probe
  passes are in the endpoint list. This single link — readiness ⇄
  receiving traffic — is the hinge that Chapters 5, 8, and 10 all turn on.

Service types are rungs on a ladder: `ClusterIP` (in-cluster only — what
web backends should be), `NodePort` (a port on every node),
`LoadBalancer` (a cloud LB per service — expensive at scale), and in
front of them all, Ingress (Chapter 11) — one entry point for many
services.

---

## Chapter 5 — Health Probes: Readiness vs Liveness

The most misconfigured pair in production Kubernetes, and the demo app
exists largely to make their difference visible:

- **Readiness — "should this pod receive traffic right now?"**
  Failing readiness is *normal*: during startup warmup, while draining,
  while an upstream dependency is briefly down. The only consequence is
  removal from Service endpoints — the pod is left alone to recover.
  Readiness MAY consider dependencies.
- **Liveness — "is this process irrecoverably stuck?"**
  Failing liveness gets the container **restarted**. It must therefore
  test only "can the process respond at all" — never dependencies.

The classic outage: putting a database check in the liveness probe. The
database blips; every web pod "fails liveness" simultaneously; Kubernetes
restarts the entire fleet; the thundering herd of restarts turns a
30-second blip into a 30-minute outage. The rule: **liveness checks the
process, readiness checks the traffic-worthiness.**

In `02-deployment.yaml`: readiness polls `/readyz` every 3 s with
`failureThreshold: 2` (react fast — traffic is at stake); liveness polls
`/healthz` every 10 s with `failureThreshold: 3` (restart reluctantly).
For slow-starting apps (JVMs), a `startupProbe` holds off both until
first success.

---

## Chapter 6 — Resources: Requests, Limits, QoS

Two numbers per container drive scheduling, autoscaling, and stability:

- **`requests`** — what the scheduler *reserves*: a pod is placed only on
  a node with that much free. Requests are also the **denominator of HPA
  percentages** (Chapter 9) — an HPA targeting 70% CPU is meaningless
  without a CPU request.
- **`limits`** — the hard ceiling: CPU above the limit is *throttled*;
  memory above the limit is an **OOM-kill**.

The demo requests `100m` CPU / `64Mi` and limits `500m` / `128Mi`:
requests sized to what the app actually needs (right-sizing is a
continuous activity — this is what VPA recommendations are for), limits
protecting the node from a runaway.

**QoS classes** fall out of these numbers: `Guaranteed`
(requests == limits — evicted last), `Burstable` (the demo — the middle),
`BestEffort` (no numbers — first against the wall under node pressure).
Web services should never be BestEffort.

Sizing intuition for scale-out: many small replicas beat few huge ones —
finer-grained scheduling (small pods fit into fragmented free space),
smaller blast radius per failure, smoother HPA steps.

---

## Chapter 7 — Configuration and Secrets

`01-config.yaml` separates what changes from what doesn't:

- The **image** is identical across dev/staging/prod — build once,
  promote the same artifact.
- The **ConfigMap** carries the environment-specific part, injected via
  `envFrom` — the app (12-factor) reads plain env vars and stays
  completely Kubernetes-unaware.
- Config changes still need a rollout to take effect as env vars; the
  `make rollout-demo` target exploits exactly that (`set env` → new
  ReplicaSet → rolling update). Mounting a ConfigMap as a *volume*
  enables live file updates instead — for apps that can re-read config.

**Secrets** hold credentials, but the base64 in a Secret is *encoding,
not encryption*. Production hardening is layered on top: encryption at
rest for etcd, RBAC narrowing who can `get secret`, and usually an
external source of truth (Vault, cloud secret managers,
external-secrets-operator). The manifest carries a placeholder token to
demonstrate the wiring (`secretKeyRef` → env var).

---

## Chapter 8 — Rolling Updates and Rollbacks

The Deployment's update strategy in `02-deployment.yaml` is the
zero-downtime recipe:

```yaml
strategy:
  rollingUpdate:
    maxSurge: 1          # one extra pod may start early
    maxUnavailable: 0    # capacity never dips below desired
```

The choreography for each replacement: new pod starts → warmup (readiness
503) → readiness passes → endpoints add it → an old pod gets SIGTERM →
old pod drains gracefully (readiness 503, in-flight requests finish) →
endpoints remove it → repeat. Every arrow depends on a chapter: probes
(5), graceful shutdown (2/10), endpoint membership (4). **Zero-downtime
deploys are not a feature you enable; they are the sum of the whole
contract.**

`make rollout-demo` triggers this live (a `VERSION` env change),
`kubectl rollout history` shows revisions, and `make rollback`
(`rollout undo`) restores the previous ReplicaSet — rollback is cheap
*because* the old ReplicaSet still exists at scale 0.

Beyond rolling updates: blue/green (two full stacks, switch the Service
selector) and canary (a small percentage on the new version — via a
second Deployment sharing the Service label, or Ingress-level weighting,
Chapter 11).

---

## Chapter 9 — Autoscaling: the HPA

`04-hpa.yaml` makes capacity follow load. The control loop:

```
desired = ceil(current_replicas × current_usage / target_usage)
```

with CPU usage read from **metrics-server** and expressed as a percentage
of the pod's CPU **request** (Chapter 6's denominator). The demo targets
70% of the 100m request, floor 2 (availability), ceiling 10 (budget).

The `behavior` block is where production maturity lives: scale **up**
immediately (spikes cost users), scale **down** slowly
(`stabilizationWindowSeconds: 120`, one pod per minute) — because
flapping — scale-down followed by immediate scale-up — is worse than a
few minutes of overprovisioning.

The live demo: `make metrics` (installs metrics-server; on kind it needs
the `--kubelet-insecure-tls` patch that the Makefile applies — a kind
quirk, not a production setting), then `make load` starts the Job in
`07-loadgen.yaml` — four busybox workers hammering `/work` through the
Service DNS name — and `make watch` shows replicas climb, then decay
slowly after the Job finishes.

The wider family, for the docs-reader: **VPA** (right-sizes requests),
**KEDA** (event-driven scaling on queue depth, RPS, custom metrics —
often the right trigger for web services since CPU lags traffic), and the
**cluster autoscaler** (adds *nodes* when pods can't schedule — the HPA's
partner: HPA asks for pods, CA makes room for them).

---

## Chapter 10 — Surviving Disruption

Scale-out is also an availability strategy — if the pieces are arranged
for it. Three mechanisms in the manifests:

- **Topology spread** (`02-deployment.yaml`): replicas spread across
  nodes (`topologyKey: kubernetes.io/hostname`, `maxSkew: 1`) so one node
  failure cannot take out the whole service. In the cloud, the same
  mechanism with `topology.kubernetes.io/zone` spreads across
  availability zones. `whenUnsatisfiable: ScheduleAnyway` keeps it a
  *preference* (a single-node kind cluster would otherwise refuse to
  schedule at all — a hard constraint is a real production choice).
- **PodDisruptionBudget** (`05-pdb.yaml`): node drains — upgrades, spot
  reclaims, cluster-autoscaler consolidation — are *voluntary*
  disruptions, and the eviction API respects budgets: with
  `minAvailable: 2`, a drain that would leave fewer than 2 replicas
  simply waits. The embedded warning matters: a PDB equal to the replica
  count makes nodes **undrainable** — a common self-inflicted operations
  wound.
- **Graceful termination**, end to end: SIGTERM → app flips readiness and
  drains (Chapter 2) → `preStop` sleep gives every kube-proxy time to
  drop the pod from its rules (endpoint propagation is asynchronous!) →
  `terminationGracePeriodSeconds: 30` bounds the whole affair before
  SIGKILL. The `preStop`+drain pair closes the classic race where a pod
  stops listening while some node still routes to it.

---

## Chapter 11 — Ingress: Traffic from the Outside

`ClusterIP` services are invisible outside the cluster, and one cloud
LoadBalancer *per service* doesn't scale economically. **Ingress**
(`06-ingress.yaml`) is the standard answer: one controller (nginx,
traefik, HAProxy, cloud-native) behind one external IP, routing by host
and path to any number of Services.

The mental model matters: the Ingress object is **just data** — an
ingress *controller* must be installed to read it and program an actual
proxy. The manifest routes `web.example.local/` to the `web` Service, and
shows where the production concerns attach: TLS via a `tls:` block whose
Secret is typically managed by **cert-manager** (automatic Let's Encrypt
issuance and renewal), and controller-specific behavior via annotations —
including nginx's canary annotations for percentage-based traffic
splitting, which turns Ingress into a deployment tool.

The **Gateway API** is the typed, role-separated successor
(GatewayClass/Gateway/HTTPRoute) worth learning next; Ingress remains
what most clusters run today.

On kind, `make ingress` installs ingress-nginx; note kind needs its
cluster created with host port mappings for the controller to be
reachable from your machine (the kind docs' ingress recipe).

---

## Chapter 12 — The Production Playbook

The checklist this course has been building, as one page:

**Workload contract**
- [ ] Stateless replicas; state in external stores
- [ ] Readiness ≠ liveness; liveness never checks dependencies
- [ ] Graceful SIGTERM drain + `preStop` + adequate grace period
- [ ] Config from env/ConfigMaps; same image every environment
- [ ] Non-root, read-only filesystem, no privilege escalation

**Capacity & scaling**
- [ ] Requests set (they are the HPA's denominator); limits protect nodes
- [ ] HPA with floor ≥ 2, sane ceiling, slow scale-down
- [ ] Namespace ResourceQuota as the blast-radius cap
- [ ] Cluster autoscaler (or equivalent) so pods can actually get nodes

**Availability**
- [ ] ≥ 2 replicas, spread across nodes/zones
- [ ] PDB strictly below replica count
- [ ] `maxUnavailable: 0` rolling updates; rollback rehearsed

**Traffic**
- [ ] ClusterIP + Ingress (or Gateway API); TLS automated via cert-manager

**Observability (the part this course only points at)**
- [ ] Metrics (Prometheus), logs (stdout → collector), traces
- [ ] Alerts on symptoms (error rate, latency) not causes (CPU)

And the honest scaling limits to know about: etcd object counts and watch
volume, scheduling throughput, DNS QPS (enable node-local DNS caching),
and kube-proxy iptables scaling (IPVS/eBPF modes) — the reasons very
large fleets shard into multiple clusters.

### CI for YAML

`make validate` runs **kubeconform** against the upstream schemas —
machine-checkable manifests, the same idea as every other course in this
repository: claims you can verify. In a real pipeline, add `kubectl apply
--dry-run=server` against a staging cluster (validates admission webhooks
and policies too) and a policy engine (OPA/Gatekeeper or Kyverno) for
organizational rules ("every Deployment must have a PDB").

## Suggested Course Progression

| Stage | Chapters | Exercises to assign |
|---|---|---|
| Foundations | 1–4 | Scale replicas up/down and watch endpoints change; break a label selector on purpose and diagnose the empty Service |
| The contract | 5–7 | Put a fake dependency check in liveness, kill the dependency, watch the restart storm (in kind!); move config to a mounted volume |
| Change & scale | 8–9 | Run a rollout with `maxUnavailable: 1` under load and measure failed requests; retune the HPA behavior block and re-run the load test |
| Resilience | 10–12 | `kubectl drain` a node with and without the PDB; write a Kyverno/OPA rule requiring probes; sketch the same app on Gateway API |
