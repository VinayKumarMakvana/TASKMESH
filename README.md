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

## Current Status (Phase 1 Implemented)

TaskMesh is currently at **Phase 1: Production-Minded Engineering Foundation**.

### What Is Implemented & Verified:
- [x] **Strongly Typed Job Model**: `JobId`, `JobPriority`, `JobStatus`, timestamps, attempt tracking, and validation rules.
- [x] **In-Memory Priority Scheduler**: $O(\log N)$ enqueue/dequeue using `std::priority_queue` with contiguous `std::vector` backing.
- [x] **Deterministic Equal-Priority FIFO Tie-Breaking**: Monotonic 64-bit sequence counter ensuring older jobs execute first among equal priorities.
- [x] **Execution Abstraction**: `IJobExecutor` interface and `InMemoryJobExecutor` implementation with custom type handlers and error isolation.
- [x] **CLI Demonstration Application**: Real-world job scheduling demonstration with error reporting and execution traces.
- [x] **Automated Test Suite**: 31 unit and stress tests passing under GoogleTest and CTest.
- [x] **Zero-Warning MSVC Build**: Strict `/W4` and `/permissive-` standard conformance under Visual Studio 2026.

### What Is Planned For Later Phases (NOT Yet Implemented):
- [ ] Concurrent worker thread pools with work-stealing (Phase 2).
- [ ] Thread-safe concurrent queue synchronization (Phase 2).
- [ ] Persistent job storage (PostgreSQL / SQLite) (Phase 3).
- [ ] Distributed broker integration (Redis / Kafka) (Phase 4).
- [ ] HTTP REST API & Metrics Dashboard (Phase 5).

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

## Design Decisions & Complexity Analysis

### Time and Space Complexities
- **`enqueue`**: $O(\log N)$ amortized time. Pushes the job into the binary heap backed by contiguous storage.
- **`dequeue`**: $O(\log N)$ time. Extracts top job and reheapifies.
- **`peek`**: $O(1)$ time. Direct inspection of root element.
- **`size` / `empty`**: $O(1)$ time.
- **Space Complexity**: $O(N)$ contiguous storage inside `std::vector`, minimizing pointer chasing and maximizing cache locality.

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

## Known Limitations (Phase 1)

1. **Single-Threaded**: The `PriorityScheduler` is intentionally single-threaded. Concurrency and synchronization primitives belong to Phase 2.
2. **In-Memory Volatile**: Jobs do not persist across process restarts. Persistence layers (SQLite / PostgreSQL) are planned for Phase 3.
3. **No Distributed Networking**: No remote RPC, HTTP server, or Redis broker is active in Phase 1.

---

## Roadmap

- **Phase 1 (Complete)**: Core domain model, priority scheduler with FIFO tie-breaking, executor abstraction, CLI demo, and automated test suite.
- **Phase 2**: Multi-threaded worker pool, thread-safe concurrent queue with condition variables, cancellation tokens, and shutdown synchronization.
- **Phase 3**: Persistent storage engine (WAL / SQLite / PostgreSQL) and crash recovery.
- **Phase 4**: Exponential backoff retry policies, dead-letter queues (DLQ), and circuit breakers.
- **Phase 5**: REST API server, structured JSON telemetry, and metrics dashboard.
