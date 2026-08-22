#!/usr/bin/env python3
# ============================================================================
#  server.py — the course's demo web service
# ============================================================================
#
#  A deliberately tiny HTTP service (stdlib only) that implements everything
#  Kubernetes expects from a WELL-BEHAVED, SCALABLE web workload:
#
#    - stateless: any replica can answer any request (12-factor)
#    - configured by environment variables (ConfigMap/Secret-friendly)
#    - /healthz  liveness  — "is this process alive?"
#    - /readyz   readiness — "should this pod receive traffic RIGHT NOW?"
#                 * fails during a startup warmup window
#                 * fails while draining after SIGTERM
#    - /work     burns ~50 ms of CPU — load for the HPA demo
#    - graceful shutdown: on SIGTERM it starts failing readiness (so the
#      endpoint controller removes it from Services), keeps serving
#      in-flight requests for DRAIN_SECONDS, then exits 0
#
#  Every one of those behaviors maps to a chapter in DOCUMENTATION.md, and
#  app/smoke_test.py verifies all of them without a cluster.
# ============================================================================

import json
import os
import signal
import socket
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

START = time.monotonic()

# 12-factor configuration: everything from the environment, with defaults.
PORT = int(os.environ.get("PORT", "8080"))
GREETING = os.environ.get("GREETING", "hello")
VERSION = os.environ.get("VERSION", "dev")
READY_DELAY = float(os.environ.get("READY_DELAY_SECONDS", "2"))
DRAIN_SECONDS = float(os.environ.get("DRAIN_SECONDS", "3"))

draining = threading.Event()


class Handler(BaseHTTPRequestHandler):
    server_version = "webcourse/1.0"

    def _reply(self, code, payload):
        body = json.dumps(payload).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/healthz":
            # Liveness: alive if we can answer at all. Deliberately does NOT
            # check dependencies — a database outage must not make Kubernetes
            # restart every web pod (that's the classic liveness mistake).
            self._reply(200, {"status": "ok"})

        elif self.path == "/readyz":
            # Readiness: gate traffic. Not ready while warming up, and not
            # ready once draining — both cause the endpoint controller to
            # take this pod out of Service load balancing.
            if draining.is_set():
                self._reply(503, {"status": "draining"})
            elif time.monotonic() - START < READY_DELAY:
                self._reply(503, {"status": "warming up"})
            else:
                self._reply(200, {"status": "ready"})

        elif self.path == "/work":
            # ~50 ms of CPU per request: measurable load for the HPA demo.
            t0 = time.perf_counter()
            spins = 0
            while time.perf_counter() - t0 < 0.05:
                spins += 1
            self._reply(200, {"work": "done", "spins": spins})

        elif self.path == "/":
            # The pod name in the response makes load balancing VISIBLE:
            # curl the Service repeatedly and watch the name change.
            self._reply(200, {
                "message": GREETING,
                "version": VERSION,
                "pod": socket.gethostname(),
            })

        else:
            self._reply(404, {"error": "not found"})

    def log_message(self, fmt, *args):
        pass  # probes every few seconds would flood stdout


def main():
    server = ThreadingHTTPServer(("0.0.0.0", PORT), Handler)

    def handle_sigterm(signum, frame):
        # Graceful shutdown, the Kubernetes way:
        #  1. flip readiness to 503 (draining) — new traffic stops arriving
        #  2. keep serving in-flight requests for DRAIN_SECONDS
        #  3. stop the server; main() returns; exit code 0
        # Kubernetes kills the container with SIGKILL only if this takes
        # longer than terminationGracePeriodSeconds.
        print("SIGTERM: draining for %.1fs, then exiting" % DRAIN_SECONDS,
              flush=True)
        draining.set()

        def stop_later():
            time.sleep(DRAIN_SECONDS)
            server.shutdown()

        threading.Thread(target=stop_later, daemon=True).start()

    signal.signal(signal.SIGTERM, handle_sigterm)

    print("web-demo %s listening on :%d (warmup %.1fs)"
          % (VERSION, PORT, READY_DELAY), flush=True)
    server.serve_forever()
    print("drained, exiting cleanly", flush=True)
    sys.exit(0)


if __name__ == "__main__":
    main()
