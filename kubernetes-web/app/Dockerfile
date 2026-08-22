# A minimal, production-shaped image for the demo service.
#
#   - small base (alpine): less to pull on every node at scale-out,
#     smaller attack surface
#   - non-root user: matches the Deployment's runAsNonRoot security context
#   - python -u: unbuffered stdout so `kubectl logs` streams in real time
#
# Build + load into a kind cluster:
#   docker build -t web-demo:1.0 app/
#   kind load docker-image web-demo:1.0 --name web-course

FROM python:3.12-alpine

RUN adduser -D -u 10001 web

WORKDIR /app
COPY server.py .

USER 10001
EXPOSE 8080
ENV PORT=8080

CMD ["python", "-u", "server.py"]
