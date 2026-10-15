# TaskMesh

**TaskMesh** is a high-performance background job-processing platform written in modern C++20. It provides priority-based job scheduling, deterministic FIFO tie-breaking for equal-priority tasks, strongly typed domain validation, and decoupled execution abstractions.

---

## Problem Being Solved

Modern distributed architectures rely heavily on background worker services to process time-consuming tasks (financial settlements, media transcoding, analytics aggregation, webhooks). Many existing job queues suffer from:
1. **Unstable Priority Queues**: In traditional binary heaps, equal-priority jobs are dequeued in undefined or nondeterministic order, causing starvation or unexpected processing sequences.
2. **Weak Domain Enforcements**: Missing boundary validation permits malformed payloads, invalid IDs, or illegal state jumps (e.g. creating jobs straight into "Completed" status).
3. **Overcoupled Architecture**: Business execution logic tangled with storage and scheduling mechanisms makes testing slow and fragile.

TaskMesh solves these core challenges starting from a robust, mathematically deterministic foundation.

---

## Current Status & Capabilities

TaskMesh is a fully implemented, production-minded C++20 concurrent background job execution platform with persistent storage, HTTP REST API, and an interactive dark-mode web dashboard.

### Core Implemented Capabilities:
- [x] **Strongly Typed Job Model**: `JobId`, `JobPriority`, `JobStatus`, microsecond timestamps, attempt tracking, and validation rules.
- [x] **In-Memory Priority Scheduler**: Array-backed binary heap with deterministic monotonic 64-bit FIFO tie-breaking for equal-priority jobs.
- [x] **Concurrent Execution Engine (Phase 2)**:
  - Thread-safe `ConcurrentPriorityQueue` utilizing condition variables and wait timeouts.
  - Multi-worker pool using modern `std::jthread` with cooperative interruption tokens.
  - Linearized cancellation protocol: guaranteed pre-execution cancellation (LP 3) & cooperative cancellation (LP 4).
  - Graceful draining (`GracefulDrain`) and immediate purge (`ImmediateStop`).
  - Thread-safe atomic metrics invariant: `submitted == queued + running + completed + failed + cancelled`.
- [x] **Durable Persistent Storage & Crash Recovery (Phase 3)**:
  - High-throughput SQLite store (`SqliteJobStore`) configured with Write-Ahead Logging (`PRAGMA journal_mode=WAL`).
  - Indexed queries on `(status, priority DESC, creation_time ASC)`.
  - Automatic crash recovery on engine startup (`recover_uncompleted_jobs`) requeuing orphaned running jobs.
- [x] **HTTP REST API & Security (Phase 4)**:
  - RESTful endpoints for submission, querying, metrics, and cooperative cancellation.
  - Token-based authentication (`Authorization: Bearer <token>` or `X-API-Key`).
  - Sliding-window IP rate limiting (rejection with HTTP 429 Too Many Requests).
  - Strongly validated JSON payloads using `nlohmann/json`.
- [x] **Embedded Responsive Web Dashboard (Phase 5)**:
  - Single-page application served directly from the server binary without external assets.
  - Live metric gauges, job submission modal, cancellation triggers, and real-time polling.
- [x] **Production Containerization & CI (Phase 6)**:
  - Multi-stage non-root `Dockerfile` with automated test validation.
  - `docker-compose.yml` with persistent volumes and healthchecks.
  - GitHub Actions CI matrix running Windows MSVC and Ubuntu Linux GCC 13.
- [x] **67 Automated Tests**: 100% pass rate in both Debug and Release configurations across 7 test suites.

---

## Architecture Summary

```
+-------------------------------------------------------------+
|                      TaskMesh Architecture                  |
+-------------------------------------------------------------+
|  Presentation:                                              |
|    - taskmesh_cli (Demonstration & Validation Harness)       |
|                                                             |
|  Core Domain & Scheduling (taskmesh_core):                  |
|    - Job: Strongly typed domain entity                      |
|    - JobId: Validated identifier ([a-zA-Z0-9_.-:])          |
|    - JobPriority: Strict partial ordering wrapper           |
|    - JobStatus: Lifecycle state machine                     |
|    - PriorityScheduler: Binary max-heap + Monotonic counter |
|                                                             |
|  Execution Abstraction:                                     |
|    - IJobExecutor: Interface separating queuing from work   |
|    - InMemoryJobExecutor: Concrete handler dispatch         |
+-------------------------------------------------------------+
```

See [docs/architecture.md](docs/architecture.md) for full architecture specifications and lifecycle state diagrams.

---

## Repository Structure

```
TaskMesh/
├── CMakeLists.txt              # Root build configuration (C++20, MSVC flags)
├── CMakePresets.json           # Standardized configure/build/test presets
├── README.md                   # Product documentation and project overview
├── .gitignore                  # Git ignore rules for CMake and Visual Studio
├── docs/
│   ├── architecture.md         # In-depth architectural specification
│   └── development.md          # Developer environment & build instructions
├── include/
│   └── taskmesh/
│       ├── job.hpp             # Job domain entity, JobId, JobPriority, Builder
│       ├── job_status.hpp      # JobStatus enum & lifecycle transition validation
│       ├── priority_scheduler.hpp # In-memory PriorityScheduler interface
│       └── job_executor.hpp    # IJobExecutor abstraction & InMemoryJobExecutor
├── src/
│   ├── main.cpp                # CLI demonstration executable
│   ├── job.cpp                 # Domain logic and invariant validations
│   ├── priority_scheduler.cpp  # Scheduler heap and FIFO tie-breaking logic
│   └── job_executor.cpp        # Executor dispatch and lifecycle transitions
└── tests/
    ├── CMakeLists.txt          # Test configuration with GoogleTest FetchContent
    ├── job_tests.cpp           # Unit tests for domain models & invariants
    ├── priority_scheduler_tests.cpp # Priority ordering, FIFO & stress tests
    └── job_executor_tests.cpp  # Executor handling & lifecycle outcome tests
```

---

## Prerequisites

- **Operating System**: Windows 10/11 (x64)
- **Compiler**: Microsoft Visual C++ (MSVC) 19.51+ (Visual Studio Build Tools 2026 or VS 2022+)
- **Build System**: CMake 3.24+ (tested with CMake 4.3.1)
- **SDK**: Windows SDK 10.0.26100.0 or compatible
- **Version Control**: Git (for CMake FetchContent dependency acquisition)

---

## Build & Test Instructions

### 1. Configure the Build
Open PowerShell in the repository root:

```powershell
# If cmake is not in PATH, add Visual Studio tools:
$env:PATH = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$env:PATH"

# Configure with Visual Studio 18 2026 generator (x64)
cmake -B build -S . -G "Visual Studio 18 2026" -A x64
```

### 2. Build the Project
```powershell
# Build Debug targets
cmake --build build --config Debug

# Build Release targets
cmake --build build --config Release
```

### 3. Run Automated Tests
```powershell
# Run tests via CTest
ctest --test-dir build -C Debug --output-on-failure

# Or run the GoogleTest executable directly
.\build\tests\Debug\taskmesh_tests.exe
```

### 4. Run the CLI Demonstration
```powershell
.\build\Debug\taskmesh_cli.exe
```

---

## Example CLI Output

Running `taskmesh_cli.exe` produces the following trace:

```text
==============================================================================
  TASKMESH — Priority Job Scheduling Engine (Phase 1 CLI Demonstration)
  Engine: C++20 | Monotonic FIFO Tie-Breaking | Modular Architecture
==============================================================================

[1] DEMONSTRATING DOMAIN VALIDATION AND ERROR HANDLING:
------------------------------------------------------------------------------
  Attempting to create job with invalid ID 'job/invalid#1': REJECTED (as expected)
    Reason: Job identifier contains illegal character: '/'. Only [a-zA-Z0-9_.-:] are permitted.
  Attempting to create job with max_attempts = 0: REJECTED (as expected)
    Reason: Job max_attempts must be at least 1.
  Attempting to enqueue already completed job: REJECTED (as expected)
    Reason: Cannot enqueue job in terminal status ('Completed').

[2] ENQUEUING REPRESENTATIVE JOBS (Interleaved Priorities):
------------------------------------------------------------------------------
  Enqueued: [Normal(50)   ] ID: notif-email-01   (Normal Priority #1)
  Enqueued: [Low(10)      ] ID: telemetry-99     (Low Priority)
  Enqueued: [High(100)    ] ID: pay-proc-101     (High Priority)
  Enqueued: [Normal(50)   ] ID: report-gen-02    (Normal Priority #2)
  Enqueued: [Critical(200)] ID: sec-audit-001    (Critical Priority)
  Enqueued: [Normal(50)   ] ID: cache-sync-03    (Normal Priority #3)

  Queue size after enqueue: 6 jobs pending.

[3] EXECUTING JOBS (Observed Dequeue Order):
------------------------------------------------------------------------------
  Expected Order:
    1. Critical (sec-audit-001)
    2. High     (pay-proc-101)
    3. Normal 1 (notif-email-01)  <-- FIFO Equal Priority Check
    4. Normal 2 (report-gen-02)   <-- FIFO Equal Priority Check
    5. Normal 3 (cache-sync-03)   <-- FIFO Equal Priority Check
    6. Low      (telemetry-99)
------------------------------------------------------------------------------
  Step 1: Dequeued [Critical(200)] ID: sec-audit-001    Type: security_audit
      [Worker] Scanning critical infrastructure: {"target":"auth_subsystem"}
      Result: SUCCESS (Vulnerability scan clean; no CVEs found.)
      Final Job Status: Completed, Attempts: 1
  Step 2: Dequeued [High(100)    ] ID: pay-proc-101     Type: payment_processing
      [Worker] Executing financial transaction payload: {"amount":12500.00,"ccy":"USD"}
      Result: SUCCESS (Funds settled and transaction ledger updated.)
      Final Job Status: Completed, Attempts: 1
  Step 3: Dequeued [Normal(50)   ] ID: notif-email-01   Type: email_dispatch
      Result: SUCCESS (Processed successfully by InMemoryJobExecutor fallback handler.)
      Final Job Status: Completed, Attempts: 1
  Step 4: Dequeued [Normal(50)   ] ID: report-gen-02    Type: report_generation
      Result: SUCCESS (Processed successfully by InMemoryJobExecutor fallback handler.)
      Final Job Status: Completed, Attempts: 1
  Step 5: Dequeued [Normal(50)   ] ID: cache-sync-03    Type: cache_warmup
      Result: SUCCESS (Processed successfully by InMemoryJobExecutor fallback handler.)
      Final Job Status: Completed, Attempts: 1
  Step 6: Dequeued [Low(10)      ] ID: telemetry-99     Type: telemetry_archive
      Result: SUCCESS (Processed successfully by InMemoryJobExecutor fallback handler.)
      Final Job Status: Completed, Attempts: 1

[4] EMPTY QUEUE SAFETY CHECK:
------------------------------------------------------------------------------
  Safely dequeued from empty queue -> std::nullopt received as expected.

[5] EXECUTION SUMMARY:
------------------------------------------------------------------------------
  Total Executed  : 6
  Successful Jobs : 6
  Failed Jobs     : 0
  Remaining Queue : 0
==============================================================================
  TaskMesh Phase 1 Demonstration Completed Successfully.
==============================================================================
```

---

## Problem-Driven DSA Selection & Design Decisions

TaskMesh selects data structures and algorithms strictly based on operation requirements, cache behavior, memory footprint, maintainability, and correctness—never to artificially demonstrate algorithm complexity.

A comprehensive, formal design record is maintained in **[docs/dsa_design_decisions.md](docs/dsa_design_decisions.md)**.

### Summary of Core DSA Choices

| Component | Selected Structure | Workload Rationale | Time Complexity | Space Complexity |
|:---|:---|:---|:---:|:---:|
| **Priority Scheduling** | `std::priority_queue` with monotonic 64-bit sequence counter | Array-backed binary max-heap maximizes cache locality; monotonic sequence preserves strict FIFO tie-breaking for equal-priority jobs. | Enqueue: $O(\log N)$<br>Dequeue: $O(\log N)$<br>Peek: $O(1)$ | $O(N)$ contiguous storage |
| **Handler Dispatch** | `std::unordered_map<std::string, Handler>` | Fast, constant-time lookup by job type on hot execution path. | $O(1)$ average | $O(M)$ where $M$ is handler count |
| **ID Validation** | Linear character check loop | Rejects invalid characters without heavy regular expression engine overhead. | $O(L)$ where $L \le 128$ | $O(1)$ auxiliary |
| **State Machine** | Compile-time jump table (`switch`) | Constant-time validation of valid lifecycle state transitions. | $O(1)$ | $O(1)$ |

### Deterministic FIFO Tie-Breaking
Standard binary heaps are unstable. TaskMesh tags every arriving job with a 64-bit monotonically increasing sequence number:
```cpp
if (lhs.job.priority() != rhs.job.priority()) {
    return lhs.job.priority() < rhs.job.priority(); // Higher priority first
}
return lhs.sequence_number > rhs.sequence_number;   // Smaller sequence number first (FIFO)
```
- **Overflow Consideration**: An unsigned 64-bit counter sustains 1 billion enqueues/sec for ~584 years before saturation. If saturated, it explicitly throws `std::overflow_error` rather than silently wrapping around and corrupting the FIFO invariant.


---

## Deployment & Production Usage

Please refer to **[docs/deployment.md](docs/deployment.md)** for exhaustive details on running the server natively, via Docker, or across cloud platforms.

### 1. Local Server Quick Start (Embedded Dashboard)

```powershell
# Run TaskMesh server on port 8080 with 4 workers and persistent SQLite database
.\build\Release\taskmesh_server.exe --port 8080 --host 0.0.0.0 --workers 4 --db taskmesh.db
```

- **Web Dashboard**: `http://localhost:8080/dashboard` or `http://localhost:8080/`
- **Health Check**: `http://localhost:8080/health`
- **Live Metrics**: `http://localhost:8080/api/v1/metrics`
- **Database Stats**: `http://localhost:8080/api/v1/stats`

### 2. 24/7 Cloud Deployment (Fly.io, Render, Docker Compose)

TaskMesh provides turnkey cloud configuration files:
- **Fly.io** ([`fly.toml`](fly.toml)): Continuous 24/7 execution with persistent NVMe volume (`/data`) and automated health checks (~$0.00 - $1.94/mo).
- **Render** ([`render.yaml`](render.yaml)): Infrastructure-as-code Blueprint with persistent disk attached at `/data`.
- **Docker Compose** ([`docker-compose.yml`](docker-compose.yml) & [`.env.example`](.env.example)): Multi-stage non-root container deployment for Linux VPS.

---

## Known Limitations

1. **Single Node (Vertical Scaling)**: TaskMesh uses high-concurrency multi-threaded workers and embedded SQLite WAL storage on a single host. Distributed multi-node consensus (e.g. Raft) is outside the single-binary scope.
2. **Fixed Handlers in Embedded Binary**: Custom job executors can be registered programmatically via the C++ `IJobExecutor` API; the default standalone binary executes built-in handler types (`payment_processing`, `data_aggregation`, `email_dispatch`, `heavy_compute`, `io_simulation`) with realistic simulated work and cooperative cancellation checks.
