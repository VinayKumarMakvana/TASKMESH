# Multi-stage production build for TaskMesh Server
# Stage 1: Build & Test
FROM gcc:13-bookworm AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    ninja-build \
    git \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

# Copy repository source code
COPY . /workspace

# Configure, build with optimization, and run full test suite
RUN cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DTASKMESH_BUILD_TESTS=ON \
    && cmake --build build --target taskmesh_server taskmesh_cli taskmesh_tests

# Run unit and integration tests inside the builder container
RUN ctest --test-dir build --output-on-failure

# Stage 2: Minimal Production Runtime
FROM debian:bookworm-slim AS runtime

# Install minimal runtime dependencies (curl for healthchecks)
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Create dedicated non-root application user
RUN groupadd -r taskmesh && useradd -r -g taskmesh -d /data -s /usr/sbin/nologin taskmesh

# Copy compiled binaries from builder stage
COPY --from=builder /workspace/build/taskmesh_server /usr/local/bin/taskmesh_server
COPY --from=builder /workspace/build/taskmesh_cli /usr/local/bin/taskmesh_cli

# Create data directory for persistent SQLite database and WAL files
RUN mkdir -p /data && chown -R taskmesh:taskmesh /data

USER taskmesh
WORKDIR /data

# Default production configuration via environment variables
ENV TASKMESH_HOST=0.0.0.0 \
    TASKMESH_PORT=8080 \
    TASKMESH_DB_PATH=/data/taskmesh.db \
    TASKMESH_WORKERS=4

EXPOSE 8080

# Healthcheck monitoring the /health endpoint
HEALTHCHECK --interval=15s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://127.0.0.1:8080/health || exit 1

ENTRYPOINT ["/usr/local/bin/taskmesh_server"]
