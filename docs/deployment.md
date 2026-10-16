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

## 4. Permanent Cloud Hosting & 24/7 Deployment

To run TaskMesh continuously without requiring a local workstation to remain powered on, deploy the container to a cloud provider with persistent volume support.

### Platform Comparison & Cost Analysis

| Platform | Compute Plan | Persistent Storage | Estimated Cost | Uptime / Sleep Behavior | Recommendation |
|---|---|---|---|---|---|
| **Fly.io** | Shared CPU 1x (256MB) | 1 GB NVMe Volume (`/data`) | **$0.00 - $1.94 / mo** (Free allowance includes 3 VMs & 3GB volume) | 24/7 continuous (`min_machines_running = 1`) | **Recommended** (Native Docker + NVMe volume) |
| **Render.com** | Starter ($7.00/mo) | 1 GB Persistent Disk ($0.25/mo) | **~$7.25 / mo** | 24/7 continuous | Good alternative (Free tier lacks disk persistence) |
| **Railway.app** | Hobby ($5.00/mo) | 1 GB Volume ($0.25/mo) | **~$5.25 / mo** | 24/7 continuous | Good alternative with web UI |
| **Cloud VPS (Hetzner / Lightsail)** | 1 vCPU / 1GB RAM | 20-40 GB SSD block storage | **$3.50 - $5.00 / mo** | 24/7 continuous Linux host | Maximum control and fixed cost |

> [!WARNING]
> Free-tier services that provide only **ephemeral disks** (e.g. Render Free, Koyeb Free) wipe SQLite files (`taskmesh.db`) whenever the container idles or restarts. For durable job execution, always attach a **persistent volume** mounted to `/data`.

---

### Deploying to Fly.io (Step-by-Step)

The repository includes a ready-to-deploy [`fly.toml`](../fly.toml) preconfigured with persistent NVMe volume mounts and healthchecks.

1. **Install flyctl**:
   ```powershell
   # Windows PowerShell:
   iwr https://fly.io/install.ps1 -useb | iex
   ```
2. **Authenticate with Fly.io**:
   ```bash
   flyctl auth login
   ```
3. **Provision the Persistent Volume**:
   ```bash
   flyctl volumes create taskmesh_data --region iad --size 1
   ```
4. **Set Production Secrets (API Key)**:
   ```bash
   flyctl secrets set TASKMESH_API_KEY="your-unique-high-entropy-production-key"
   ```
5. **Deploy the Service**:
   ```bash
   flyctl deploy
   ```
6. **Verify Public Endpoints**:
   ```bash
   curl https://taskmesh-engine.fly.dev/health
   curl https://taskmesh-engine.fly.dev/dashboard
   ```

---

### Deploying to Render (Blueprint)

1. Connect your GitHub repository to Render.
2. In the Render Dashboard, click **New +** $\rightarrow$ **Blueprint**.
3. Select this repository; Render will automatically detect [`render.yaml`](../render.yaml).
4. Render will provision the Starter service with a 1GB persistent disk at `/data` and auto-generate a secret `TASKMESH_API_KEY`.

---

### Deploying to Cloud VPS via Docker Compose

1. Clone the repository onto the remote Linux server:
   ```bash
   git clone <repo-url> /opt/taskmesh && cd /opt/taskmesh
   ```
2. Create production `.env` from template:
   ```bash
   cp .env.example .env
   # Edit .env and set your secure TASKMESH_API_KEY
   ```
3. Launch with persistent volume:
   ```bash
   docker compose up -d
   ```
4. Configure Nginx / Caddy reverse proxy with automatic Let's Encrypt TLS.

---

## 5. Health & Observability Endpoints

| Endpoint | Method | Auth Required | Purpose |
|---|---|---|---|
| `/health` | `GET` | No | Liveness probe (returns status, uptime, worker count) |
| `/ready` | `GET` | No | Readiness probe (verifies database read/write access and queue health) |
| `/api/v1/metrics` | `GET` | No | Real-time queue and worker execution metrics (submitted, queued, running, completed, failed, cancelled) |
| `/api/v1/stats` | `GET` | No | Aggregate database job counts by status |
| `/api/v1/jobs` | `GET` | No | List jobs with optional status filter, limit, and offset |
| `/api/v1/jobs` | `POST` | **Yes** | Schedule a new background job with priority, attempts, and payload |
| `/api/v1/jobs/:id` | `GET` | No | Inspect specific job state, result, and attempts |
| `/api/v1/jobs/:id/cancel` | `POST` | **Yes** | Cooperatively cancel an active (Queued or Running) job |
| `/api/v1/jobs/:id/retry` | `POST` | **Yes** | Re-queue a Failed or Cancelled job for retry |
| `/api/v1/jobs/:id` | `DELETE` | **Yes** | Permanently delete a terminal (Completed, Failed, Cancelled) job record |
| `/` or `/dashboard` | `GET` | No | Interactive dark-mode web monitoring dashboard SPA |

---

## 6. Backup and Recovery Procedures

### SQLite Online Hot Backup (Zero Downtime)
Because TaskMesh runs SQLite in WAL mode (`PRAGMA journal_mode=WAL`), you can create a consistent online backup while the server is running without blocking reads or writes:

```bash
# On the host (or inside the container):
sqlite3 /data/taskmesh.db ".backup /data/taskmesh_backup_$(date +%Y%m%d_%H%M%S).db"
```

### Database Restore Procedure
1. Stop the TaskMesh service or container:
   ```bash
   docker stop taskmesh
   ```
2. Replace `/data/taskmesh.db` with your verified backup:
   ```bash
   cp /backups/taskmesh_backup.db /data/taskmesh.db
   rm -f /data/taskmesh.db-wal /data/taskmesh.db-shm
   ```
3. Ensure file ownership belongs to non-root user `taskmesh`:
   ```bash
   chown taskmesh:taskmesh /data/taskmesh.db
   ```
4. Restart the TaskMesh service or container:
   ```bash
   docker start taskmesh
   ```

---

## 7. Crash Recovery & Resilience Guarantees

1. **Write-Ahead Logging (WAL)**: SQLite uses WAL mode (`PRAGMA journal_mode=WAL`) ensuring high-concurrency read/write throughput and crash durability.
2. **Orphaned Job Recovery**: If the server process or host terminates unexpectedly while jobs are in `Running` state:
   - On startup, `SqliteJobStore::recover_uncompleted_jobs()` inspects jobs marked `Running`.
   - If `current_attempt < max_attempts`, the job status is reset to `Queued` and automatically scheduled into the execution engine.
   - If attempts are exhausted, the job status is transitioned to `Failed` with diagnostic error message `"Server process crashed or restarted during execution"`.
