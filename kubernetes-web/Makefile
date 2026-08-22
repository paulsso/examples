# ============================================================================
# Makefile for the scalable Kubernetes course.
#
# Two tiers of targets:
#
#   VALIDATION (works anywhere — no Docker, no cluster; this is what CI runs)
#     make validate     - schema-validate every manifest (kubeconform)
#     make app-test     - run the demo service locally and verify its whole
#                         Kubernetes contract: probes, warmup, config from
#                         env, CPU endpoint, graceful SIGTERM drain
#
#   LIVE DEMO (needs Docker + kind + kubectl on your machine)
#     make cluster      - create a local kind cluster
#     make build        - build the app image and load it into kind
#     make deploy       - apply all manifests
#     make status       - rollout status + pods/svc/hpa/pdb overview
#     make smoke        - port-forward and curl the service
#     make rollout-demo - config change -> zero-downtime rolling update
#     make rollback     - kubectl rollout undo
#     make metrics      - install metrics-server (needed by the HPA)
#     make load         - start the load-generator Job (watch the HPA!)
#     make watch        - watch hpa + pods live
#     make down         - delete the kind cluster
# ============================================================================

NS      = web-course
CLUSTER = web-course
IMAGE   = web-demo:1.0

# ---------------------------------------------------------------- validation

.PHONY: validate
validate:
	kubeconform -strict -summary manifests/*.yaml

.PHONY: app-test
app-test:
	python3 app/smoke_test.py

# ----------------------------------------------------------------- live demo

.PHONY: cluster
cluster:
	kind create cluster --name $(CLUSTER)

.PHONY: build
build:
	docker build -t $(IMAGE) app/
	kind load docker-image $(IMAGE) --name $(CLUSTER)

.PHONY: deploy
deploy:
	kubectl apply -f manifests/00-namespace.yaml
	kubectl apply -f manifests/01-config.yaml
	kubectl apply -f manifests/02-deployment.yaml
	kubectl apply -f manifests/03-service.yaml
	kubectl apply -f manifests/04-hpa.yaml
	kubectl apply -f manifests/05-pdb.yaml
	kubectl -n $(NS) rollout status deployment/web

.PHONY: status
status:
	kubectl -n $(NS) get deploy,pods,svc,hpa,pdb -o wide

.PHONY: smoke
smoke:
	kubectl -n $(NS) port-forward service/web 8080:80 & \
	sleep 2; \
	curl -s http://localhost:8080/ ; echo; \
	curl -s http://localhost:8080/readyz ; echo; \
	kill %1

# Zero-downtime rolling update: a config change triggers a new rollout;
# maxUnavailable: 0 means capacity never dips. Run `make smoke` in another
# terminal in a loop — no request fails.
.PHONY: rollout-demo
rollout-demo:
	kubectl -n $(NS) set env deployment/web VERSION=2.0
	kubectl -n $(NS) rollout status deployment/web
	kubectl -n $(NS) rollout history deployment/web

.PHONY: rollback
rollback:
	kubectl -n $(NS) rollout undo deployment/web
	kubectl -n $(NS) rollout status deployment/web

# metrics-server feeds the HPA. The kubelet-insecure-tls patch is needed
# on kind (self-signed kubelet certs) — NOT in production.
.PHONY: metrics
metrics:
	kubectl apply -f https://github.com/kubernetes-sigs/metrics-server/releases/latest/download/components.yaml
	kubectl -n kube-system patch deployment metrics-server --type=json \
	  -p='[{"op":"add","path":"/spec/template/spec/containers/0/args/-","value":"--kubelet-insecure-tls"}]'
	kubectl -n kube-system rollout status deployment/metrics-server

.PHONY: load
load:
	kubectl apply -f manifests/07-loadgen.yaml

.PHONY: watch
watch:
	kubectl -n $(NS) get hpa,pods -w

# Ingress on kind needs the ingress-nginx controller and a cluster created
# with port mappings; see DOCUMENTATION.md Chapter 11 for the full recipe.
.PHONY: ingress
ingress:
	kubectl apply -f https://raw.githubusercontent.com/kubernetes/ingress-nginx/main/deploy/static/provider/kind/deploy.yaml
	kubectl apply -f manifests/06-ingress.yaml

.PHONY: down
down:
	kind delete cluster --name $(CLUSTER)

.PHONY: clean
clean:
	rm -f __pycache__ -r 2>/dev/null || true
