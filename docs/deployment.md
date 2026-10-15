# TaskMesh Production Deployment Guide

This guide details how to build, configure, verify, and publicly deploy the TaskMesh C++20 concurrent job execution platform.

---

## 1. System Architecture Overview

```
                      +---------------------------------------+
                      |             Clients                   |
                      |  (Web Dashboard / REST API Consumers) |
                      +-------------------+-------------------+
                                          | HTTPS / HTTP
                                          v
                      +---------------------------------------+
                      |          TaskMesh Server              |
                      |  - cpp-httplib HTTP Host (Port 8080)   |
                      |  - Token Auth & Rate Limiter          |
                      |  - Embedded HTML5/JS Dark Dashboard   |
                      +-------------------+-------------------+
                                          |
                      +-------------------+-------------------+
                      |       Concurrent Execution Engine      |
                      |  - Worker Thread Pool (std::jthread)  |
                      |  - ConcurrentPriorityQueue (C++20 CV) |
                      |  - Cooperative Cancellation Tokens    |
                      +-------------------+-------------------+
                                          |
                                          v
                      +---------------------------------------+
                      |         SqliteJobStore (WAL)          |
                      |  - Atomic state transitions           |
                      |  - Crash recovery & replay on startup |
                      |  - Durable taskmesh.db                |
                      +---------------------------------------+
```

---

## 2. Configuration & Environment Variables

TaskMesh can be configured via command-line arguments or environment variables:

| Environment Variable | CLI Argument | Default | Description |
|---|---|---|---|
| `TASKMESH_PORT` or `PORT` | `--port <num>` | `8080` | TCP port for HTTP API and Dashboard |
| `TASKMESH_HOST` | `--host <addr>` | `0.0.0.0` | Network interface to bind (`0.0.0.0` for public/container) |
| `TASKMESH_WORKERS` | `--workers <num>` | `4` | Number of worker execution threads |
| `TASKMESH_DB_PATH` | `--db <path>` | `taskmesh.db` | Persistent SQLite database file path |
| `TASKMESH_API_KEY` | `--api-key <key>` | *(none)* | Shared secret token for write endpoints |

---

## 3. Deployment Methods

### Option A: Native Windows Executable

1. Build in Release mode:
   ```powershell
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```
2. Run the server:
   ```powershell
   $env:TASKMESH_API_KEY = "your-secure-secret-token"
   .\build\Release\taskmesh_server.exe --port 8080 --host 0.0.0.0 --db C:\data\taskmesh.db --workers 8
   ```

### Option B: Docker Container

TaskMesh provides a multi-stage `Dockerfile` that builds and validates tests before generating a minimal, non-root runtime image:

```bash
# Build Docker image
docker build -t taskmesh:latest .

# Run container with persistent data volume
docker run -d \
  --name taskmesh \
  -p 8080:8080 \
  -v taskmesh_data:/data \
  -e TASKMESH_API_KEY="your-secure-secret-token" \
  taskmesh:latest
```

### Option C: Docker Compose

```bash
docker compose up -d
```

Check logs and health:
```bash
docker compose logs -f taskmesh
docker inspect --format='{{json .State.Health}}' taskmesh-server
```

---

## 4. Public Deployment & Reachability

To expose TaskMesh publicly to users and remote services:

### 1. Cloud Virtual Machines (AWS EC2, Hetzner, DigitalOcean)
- Deploy the Docker container or native binary.
- Bind `--host 0.0.0.0`.
- Configure security groups / firewall to allow inbound traffic on port 80/443.
- Use Nginx or Caddy as reverse proxy with automatic TLS (Let's Encrypt).

### 2. Cloudflare Tunnel / Local Tunnel (Instant Public Reachability)
For public edge routing without exposing public IPs or opening router ports:
```bash
# Using Cloudflare Tunnel
cloudflared tunnel --url http://localhost:8080

# Using Localtunnel
npx localtunnel --port 8080
```

---

## 5. Health & Observability Endpoints

| Endpoint | Method | Auth Required | Purpose |
|---|---|---|---|
| `/health` | `GET` | No | Liveness probe (returns status, uptime, version) |
| `/ready` | `GET` | No | Readiness probe (verifies database read/write access) |
| `/api/v1/metrics` | `GET` | No | Real-time queue and worker execution metrics |
| `/api/v1/stats` | `GET` | No | Aggregate database job counts by status |
| `/api/v1/jobs` | `GET` | Yes | List jobs with optional status filter & pagination |
| `/api/v1/jobs` | `POST` | Yes | Submit a new background job with priority & payload |
| `/api/v1/jobs/:id` | `GET` | Yes | Inspect specific job state, result, and attempts |
| `/api/v1/jobs/:id/cancel` | `POST` | Yes | Cooperatively cancel a queued or running job |
| `/` or `/dashboard` | `GET` | No | Interactive dark-mode web monitoring dashboard |

---

## 6. Crash Recovery & Resilience Guarantees

1. **Write-Ahead Logging (WAL)**: SQLite uses WAL mode (`PRAGMA journal_mode=WAL`) ensuring high-concurrency read/write throughput and crash durability.
2. **Orphaned Job Recovery**: If the server process or host terminates unexpectedly while jobs are in `Running` state:
   - On startup, `SqliteJobStore::recover_uncompleted_jobs()` inspects jobs marked `Running`.
   - If `current_attempt < max_attempts`, the job status is reset to `Queued` and automatically scheduled into the execution engine.
   - If attempts are exhausted, the job status is transitioned to `Failed` with diagnostic error message `"Server process crashed or restarted during execution"`.
