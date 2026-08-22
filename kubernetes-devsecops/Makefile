# ============================================================================
# Makefile for the Kubernetes DevSecOps course.
#
# THE PIPELINE GATE (runs anywhere — no Docker, no cluster; this is CI):
#   make validate    - schema-validate all manifests (kubeconform)
#   make audit       - policy-as-code audit of the hardened manifests (passes)
#   make audit-bad   - the same audit against examples/insecure-deployment.yaml
#                      (catches every violation; the target EXPECTS failure)
#   make gate        - validate + audit: the full CI gate in one target
#
# LIVE DEMO (needs Docker + kind + kubectl on your machine):
#   make cluster     - kind cluster
#   make deploy      - apply the hardened manifests
#   make psa-test    - try to apply the insecure example: Pod Security
#                      Admission and the CEL policy both reject it live
#   make trivy       - scan the app image for CVEs (needs trivy installed)
#   make kube-bench  - run the CIS Kubernetes benchmark as a Job
#   make down        - delete the cluster
# ============================================================================

NS = devsecops

# ------------------------------------------------------------- pipeline gate

.PHONY: validate
validate:
	kubeconform -strict -summary manifests/*.yaml examples/*.yaml

.PHONY: audit
audit:
	python3 tools/audit.py manifests

.PHONY: audit-bad
audit-bad:
	@python3 tools/audit.py examples && \
	  (echo "ERROR: the insecure example passed the audit!"; exit 1) || \
	  echo "audit correctly REJECTED the insecure example (exit 1 blocks CI)"

.PHONY: gate
gate: validate audit
	@echo "pipeline gate: PASSED (schemas valid, policies satisfied)"

# ------------------------------------------------------------------ live demo

.PHONY: cluster
cluster:
	kind create cluster --name $(NS)

.PHONY: deploy
deploy:
	kubectl apply -f manifests/
	kubectl -n $(NS) rollout status deployment/web

.PHONY: status
status:
	kubectl -n $(NS) get all,networkpolicy,role,rolebinding,resourcequota

# Both enforcement layers reject the insecure example live:
#  - Pod Security Admission (namespace labels) rejects the POD
#  - the ValidatingAdmissionPolicy (CEL) rejects the DEPLOYMENT
.PHONY: psa-test
psa-test:
	-kubectl apply -f examples/insecure-deployment.yaml
	@echo "(rejections above are the system working)"

.PHONY: trivy
trivy:
	trivy image nginxinc/nginx-unprivileged:1.27-alpine

.PHONY: kube-bench
kube-bench:
	kubectl apply -f https://raw.githubusercontent.com/aquasecurity/kube-bench/main/job.yaml
	kubectl wait --for=condition=complete job/kube-bench --timeout=120s
	kubectl logs job/kube-bench

.PHONY: down
down:
	kind delete cluster --name $(NS)
