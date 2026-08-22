# Scalable Kubernetes for Web Services

A course on running **web services that scale** on Kubernetes: from "why an
orchestrator at all" through zero-downtime rolling deploys, autoscaling
under load, and surviving node failures and cluster maintenance. The course
text is [DOCUMENTATION.md](DOCUMENTATION.md).

## Three artifacts, three ways to learn

| Artifact | What it teaches | How to verify it |
|---|---|---|
| `app/` — a tiny demo web service | the **workload contract**: statelessness, env config, readiness vs liveness, graceful SIGTERM drain | `make app-test` — locally, no Docker, ~3 s |
| `manifests/` — chapter-numbered YAML | production-shaped Deployment, Service, HPA, PDB, Ingress, quota — **every field commented** | `make validate` — offline schema check (kubeconform) |
| `Makefile` — the live runbook | deploy, roll out, roll back, autoscale on a local [kind](https://kind.sigs.k8s.io/) cluster | needs Docker + kind + kubectl on your machine |

## Quick start (no cluster needed)

```bash
make validate    # schema-validate all manifests
make app-test    # verify the app's full Kubernetes contract locally
```

`app-test` proves, in seconds and without any cluster: readiness fails
during warmup, flips after; liveness stays up; config comes from env;
SIGTERM starts a drain (readiness 503, traffic still served) and the
process exits 0.

## The live demo (any machine with Docker)

```bash
make cluster           # kind create cluster
make build             # build the image, load it into kind
make deploy status     # apply manifests, wait for rollout
make smoke             # port-forward + curl

make rollout-demo      # config change -> zero-downtime rolling update
make rollback          # kubectl rollout undo

make metrics           # install metrics-server (HPA's data source)
make load              # start the load-generator Job
make watch             # watch the HPA scale 2 -> N and slowly back

make down              # delete the cluster
```

## Chapters

1. Why Kubernetes for web services — the four mechanisms every scaled team reinvents
2. The workload contract — the demo app and why statelessness is the prerequisite
3. Pods, ReplicaSets, Deployments — labels as wiring, quotas as blast radius
4. Services — stable names, load balancing, readiness-driven membership
5. Health probes — readiness ≠ liveness, and the outage caused by confusing them
6. Resources — requests (the scheduler's & HPA's number), limits, QoS
7. Configuration & secrets — same image everywhere, config injected
8. Rolling updates & rollbacks — `maxUnavailable: 0` and the full zero-downtime choreography
9. Autoscaling — HPA math, behavior tuning, metrics-server, the load-test demo
10. Surviving disruption — PDBs, topology spread, graceful termination end-to-end
11. Ingress — one entry point for many services, TLS, canary, Gateway API
12. The production playbook — the checklist, honest scaling limits, CI for YAML

## Layout

| Path | Purpose |
|---|---|
| `DOCUMENTATION.md` | **The course text** |
| `app/server.py` | The demo service (stdlib Python, ~100 lines) |
| `app/smoke_test.py` | Verifies the workload contract locally |
| `app/Dockerfile` | Minimal non-root image |
| `manifests/00-…07-*.yaml` | Chapter-numbered, fully commented manifests |
| `Makefile` | validate / app-test (anywhere) + the live kind runbook |
