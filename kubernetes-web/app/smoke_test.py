#!/usr/bin/env python3
# ============================================================================
#  smoke_test.py — verifies the demo service's Kubernetes contract locally
# ============================================================================
#
#  Runs server.py as a subprocess (no Docker, no cluster needed) and checks
#  every behavior the manifests rely on:
#
#    1. /readyz returns 503 during the warmup window   (readiness probe)
#    2. /readyz flips to 200 after warmup              (traffic gating)
#    3. /healthz is 200 the whole time                 (liveness probe)
#    4. / serves config from environment variables     (ConfigMap contract)
#    5. /work burns CPU and responds                   (HPA load endpoint)
#    6. on SIGTERM: /readyz -> 503 (draining) while    (graceful shutdown /
#       in-flight traffic still works, then exit 0      zero-downtime deploys)
#
#  This is the course's point about testability: the pod lifecycle contract
#  is just process behavior — testable on any machine in seconds.
# ============================================================================

import json
import os
import signal
import subprocess
import sys
import time
import urllib.error
import urllib.request

PORT = 18080
BASE = f"http://127.0.0.1:{PORT}"


def get(path):
    """Returns (status_code, parsed_json) without raising on 4xx/5xx."""
    try:
        with urllib.request.urlopen(BASE + path, timeout=2) as resp:
            return resp.status, json.loads(resp.read())
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read())


def expect(label, condition):
    print(f"  {'PASS' if condition else 'FAIL'}  {label}")
    if not condition:
        sys.exit(1)


def main():
    env = dict(os.environ,
               PORT=str(PORT),
               GREETING="smoke-test",
               VERSION="9.9",
               READY_DELAY_SECONDS="1.5",
               DRAIN_SECONDS="1.0")
    proc = subprocess.Popen([sys.executable, "server.py"], env=env,
                            cwd=os.path.dirname(os.path.abspath(__file__)))
    try:
        time.sleep(0.5)  # server is up, but inside its warmup window

        code, body = get("/readyz")
        expect("readiness is 503 while warming up", code == 503
               and body["status"] == "warming up")

        code, _ = get("/healthz")
        expect("liveness is 200 during warmup", code == 200)

        deadline = time.time() + 5
        code = 0
        while time.time() < deadline:
            code, _ = get("/readyz")
            if code == 200:
                break
            time.sleep(0.2)
        expect("readiness flips to 200 after warmup", code == 200)

        code, body = get("/")
        expect("config comes from the environment",
               code == 200 and body["message"] == "smoke-test"
               and body["version"] == "9.9")

        code, body = get("/work")
        expect("the CPU work endpoint responds", code == 200
               and body["spins"] > 0)

        # Graceful shutdown: SIGTERM must flip readiness while the server
        # still answers, then the process must exit cleanly on its own.
        proc.send_signal(signal.SIGTERM)
        time.sleep(0.3)

        code, body = get("/readyz")
        expect("after SIGTERM, readiness is 503 (draining)",
               code == 503 and body["status"] == "draining")

        code, _ = get("/")
        expect("in-flight traffic still served while draining", code == 200)

        exit_code = proc.wait(timeout=5)
        expect("process exits 0 after the drain window", exit_code == 0)

        print("smoke test: ALL CHECKS PASSED")
    finally:
        if proc.poll() is None:
            proc.kill()


if __name__ == "__main__":
    main()
