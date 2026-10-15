# TaskMesh Data Structures and Algorithms (DSA) Design Decisions

This document records the justification, complexity analysis, trade-off evaluations, and verification references for every data structure and algorithm selected in TaskMesh, adhering to the principle of **Problem-Driven DSA Selection**.

> **Core Philosophy**: Never introduce a data structure or algorithm solely to demonstrate DSA knowledge or artificial sophistication. The simplest, most standard, and most maintainable solution that meets correctness, latency, and memory targets is always preferred.

---

## Decision Index

1. [DSA-01: Priority-Based Job Scheduling with Deterministic FIFO Tie-Breaking](#dsa-01-priority-based-job-scheduling-with-deterministic-fifo-tie-breaking)
2. [DSA-02: Execution Handler Lookup and Dispatch](#dsa-02-execution-handler-lookup-and-dispatch)
3. [DSA-03: Identifier Syntax Validation](#dsa-03-identifier-syntax-validation)
4. [DSA-04: Lifecycle State Transition Validation](#dsa-04-lifecycle-state-transition-validation)
5. [DSA-05: Planned Structures for Future Phases (Evaluation Preview)](#dsa-05-planned-structures-for-future-phases-evaluation-preview)

---

## DSA-01: Priority-Based Job Scheduling with Deterministic FIFO Tie-Breaking

### 1. Problem Being Solved
Background jobs arrive asynchronously with differing urgency levels (`Critical`, `High`, `Normal`, `Low`). The scheduling engine must:
- Extract higher-priority jobs before lower-priority jobs.
- Maintain deterministic First-In, First-Out (FIFO) ordering among jobs sharing the exact same priority.
- Handle queues scaling from dozens to tens of thousands of pending jobs with predictable latency.
- Provide safe operations on empty queues without exceptions.

### 2. Selected Data Structure & Algorithm
- **Container**: `std::priority_queue<ScheduledJobEntry, std::vector<ScheduledJobEntry>, ScheduledJobComparator>` from `<queue>`.
- **Backing Storage**: Contiguous `std::vector<ScheduledJobEntry>`.
- **Tie-Breaking Algorithm**: A monotonic 64-bit integer counter (`uint64_t sequence_counter_`) incremented upon each successful enqueue.
- **Comparator**:
  ```cpp
  if (lhs.job.priority() != rhs.job.priority()) {
      return lhs.job.priority() < rhs.job.priority(); // Higher numerical priority placed at heap top
  }
  return lhs.sequence_number > rhs.sequence_number;   // Lower sequence number (earlier arrival) placed at heap top
  ```

### 3. Workload Fit & Rationale
- **Contiguous Memory**: `std::vector` stores heap nodes sequentially in memory. Compared to node-based tree structures (like red-black trees in `std::map`), array-backed binary heaps exhibit outstanding CPU L1/L2 cache locality during sift-up and sift-down operations.
- **Minimal Per-Node Overhead**: Array nodes store only the payload without tree parent/child/color pointer overhead (saving 24–32 bytes per node on 64-bit platforms).
- **Stability via Monotonic Sequencing**: Standard binary heaps are unstable. Rather than writing a custom stable heap or maintaining multiple queues, augmenting each entry with a 64-bit sequence counter introduces stable FIFO tie-breaking with zero runtime algorithmic complexity penalty.
- **Overflow Resilience**: A 64-bit unsigned integer counter allows $1.84 \times 10^{19}$ operations. At 1,000,000,000 operations per second, overflow would require over 584 years of continuous operation without restart. The counter explicitly throws `std::overflow_error` at `UINT64_MAX` rather than silently wrapping.

### 4. Complexity Analysis
- **Enqueue (`push`)**:
  - Time Complexity: $O(\log N)$ worst-case heap sift-up; $O(1)$ amortized memory allocation when vector capacity grows.
  - Space Complexity: $O(1)$ auxiliary per operation.
- **Dequeue (`pop`)**:
  - Time Complexity: $O(\log N)$ heap sift-down.
  - Space Complexity: $O(1)$ auxiliary.
  - **Copy Trade-Off (L-02)**: `std::priority_queue::top()` returns `const value_type&`. In accordance with engineering safety principles, `const_cast` is deliberately rejected to avoid fragile or undefined behavior on container-managed heap nodes. Elements are safely copied out before popping. For Phase 1 payloads, this copy is negligible (~microsecond scale) and provides 100% standard conformance.
- **Peek (`top`)**:
  - Time Complexity: $O(1)$ direct access to root element.
- **Move Operations (L-01)**:
  - Both move construction and move assignment leave the source scheduler in a canonical empty state (`size() == 0`, `sequence_counter_ == 0`), making it immediately reusable without manual clearing.
- **Overall Container Space Complexity**:
  - $O(N)$ contiguous memory.

### 5. Alternatives Evaluated and Rejected

| Alternative | Mechanics | Pros | Why Rejected |
|:---|:---|:---|:---|
| **`std::map<JobPriority, std::queue<Job>>`** | Red-black tree of FIFO queues per priority | Natural FIFO per priority; $O(\log P)$ where $P$ is unique priorities | Node-based allocations for every tree node; separate dynamically allocated queue per priority; multiple pointer indirections destroying cache locality; excessive memory fragmentation under high job turnover. |
| **Sorted Contiguous Array (`std::vector`)** | Maintain sorted vector via binary search insert | $O(\log N)$ lookup, $O(1)$ peek | $O(N)$ insertion cost due to memory shifting (`memmove`). Catastrophic latency degradation at $N \ge 10,000$. |
| **Doubly Linked List (`std::list`)** | Linear traversal to insertion position | $O(1)$ deletion given node pointer | $O(N)$ search on insertion; pointer chasing on every operation; high cache miss penalty. |
| **Custom 4-ary / d-ary Heap** | Custom array-based d-ary heap | Flatter tree depth, potentially fewer cache misses during sift-up | Violates Rule 5 (avoid unnecessary custom implementations). Standard library `std::priority_queue` is proven, robust, and performs within sub-microsecond bounds for expected Phase 1 loads. |

### 6. Verification Tests
- `PrioritySchedulerTest.HigherPriorityComesFirst` in [tests/priority_scheduler_tests.cpp](file:///C:/Users/VINAY/OneDrive/Desktop/TASKMESH/tests/priority_scheduler_tests.cpp#L23-L53).
- `PrioritySchedulerTest.EqualPriorityFollowsStrictFIFO` in [tests/priority_scheduler_tests.cpp](file:///C:/Users/VINAY/OneDrive/Desktop/TASKMESH/tests/priority_scheduler_tests.cpp#L55-L69).
- `PrioritySchedulerTest.MultiplePrioritiesMixedTogether` in [tests/priority_scheduler_tests.cpp](file:///C:/Users/VINAY/OneDrive/Desktop/TASKMESH/tests/priority_scheduler_tests.cpp#L71-L101).
- `PrioritySchedulerTest.DeterministicStressBatchOrderingInvariant` in [tests/priority_scheduler_tests.cpp](file:///C:/Users/VINAY/OneDrive/Desktop/TASKMESH/tests/priority_scheduler_tests.cpp#L170-L243) (5,000 jobs batch, verifying $A.\text{priority} \ge B.\text{priority}$ and FIFO tie-breaking across randomized arrivals).
- `PrioritySchedulerTest.RejectEnqueueOfExhaustedFailedJob` in [tests/priority_scheduler_tests.cpp](file:///C:/Users/VINAY/OneDrive/Desktop/TASKMESH/tests/priority_scheduler_tests.cpp#L293-L309) (M-01: retry eligibility).
- `PrioritySchedulerTest.AcceptEnqueueOfRetryableFailedJob` in [tests/priority_scheduler_tests.cpp](file:///C:/Users/VINAY/OneDrive/Desktop/TASKMESH/tests/priority_scheduler_tests.cpp#L311-L337) (M-01: valid retry support).
- `PrioritySchedulerTest.MoveConstructorLeavesSourceInValidReusableState` in [tests/priority_scheduler_tests.cpp](file:///C:/Users/VINAY/OneDrive/Desktop/TASKMESH/tests/priority_scheduler_tests.cpp#L339-L368) (L-01: move constructor).
- `PrioritySchedulerTest.MoveAssignmentLeavesSourceInValidReusableState` in [tests/priority_scheduler_tests.cpp](file:///C:/Users/VINAY/OneDrive/Desktop/TASKMESH/tests/priority_scheduler_tests.cpp#L370-L394) (L-01: move assignment).

---

## DSA-02: Execution Handler Lookup and Dispatch

### 1. Problem Being Solved
When a job is dequeued for execution, the executor must dispatch the job to a handler callback corresponding to its `type` (e.g. `"payment_processing"`, `"email_dispatch"`). If no handler is registered, a default handler is invoked.

### 2. Selected Data Structure & Algorithm
- **Container**: `std::unordered_map<std::string, Handler>` from `<unordered_map>`.
- **Lookup**: Hash table lookup using `std::hash<std::string>`.

### 3. Workload Fit & Rationale
- The set of job types in a typical service ranges from 5 to 100 distinct string keys.
- Registrations occur primarily during initialization.
- Execution happens continuously on the hot path. Average $O(1)$ lookup prevents dispatcher bottlenecks.

### 4. Complexity Analysis
- **Registration**: $O(1)$ average, $O(M)$ worst-case rehashing where $M$ is registered handler count.
- **Dispatch Lookup**: $O(1)$ average, $O(K)$ string comparison where $K$ is string key length (typically $< 32$ bytes).
- **Space Complexity**: $O(M)$ where $M$ is number of distinct registered types.

### 5. Alternatives Evaluated and Rejected
- **`std::map<std::string, Handler>`**: $O(\log M \cdot K)$ lookup overhead with tree-node allocations. Unnecessary when alphabetical ordering of job types is never needed.
- **Linear Search in `std::vector<std::pair<std::string, Handler>>`**: For very small $M$ ($< 5$), a flat vector can be faster than hashing due to zero hash calculation overhead. However, `std::unordered_map` scales smoothly to dozens of job types without manual branching or algorithmic transitions.

### 6. Verification Tests
- `JobExecutorTest.CustomHandlerDispatchByType` in [tests/job_executor_tests.cpp](file:///C:/Users/VINAY/OneDrive/Desktop/TASKMESH/tests/job_executor_tests.cpp#L29-L49).
- `JobExecutorTest.FailedExecutionUpdatesJobToFailed` in [tests/job_executor_tests.cpp](file:///C:/Users/VINAY/OneDrive/Desktop/TASKMESH/tests/job_executor_tests.cpp#L51-L71).

---

## DSA-03: Identifier Syntax Validation

### 1. Problem Being Solved
Every `Job` requires a unique `JobId`. To prevent SQL/command injection, log corruption, and downstream URI encoding bugs in later phases, identifiers must conform strictly to `[a-zA-Z0-9_.-:]` with length bounded between 1 and 128 characters.

### 2. Selected Data Structure & Algorithm
- **Algorithm**: Single-pass linear character inspection ($O(L)$) using `std::isalnum` and explicit character comparisons on an unsigned char buffer.
- **Storage**: `std::string` inside `JobId`.

### 3. Workload Fit & Rationale
- **Zero Overhead**: Does not compile or execute regular expressions.
- **No Heap Allocation for Validation**: Operates directly on `std::string_view` without allocating temporary strings.
- **Short-Circuit Rejection**: Fails immediately on empty strings or characters outside the allowed ASCII character set.

### 4. Complexity Analysis
- **Time Complexity**: $O(L)$ where $L$ is identifier length ($L \le 128$). Bounded by 128 iterations.
- **Space Complexity**: $O(1)$ auxiliary memory.

### 5. Alternatives Evaluated and Rejected
- **`std::regex`**: Heavy engine startup cost, high memory overhead, potential catastrophic backtracking in complex patterns. For character-class membership, standard algorithmic loops are orders of magnitude faster and simpler.

### 6. Verification Tests
- `JobTest.InvalidJobIdEmptyThrows` in [tests/job_tests.cpp](file:///C:/Users/VINAY/OneDrive/Desktop/TASKMESH/tests/job_tests.cpp#L78-L91).
- `JobTest.InvalidJobIdContainingWhitespaceThrows` in [tests/job_tests.cpp](file:///C:/Users/VINAY/OneDrive/Desktop/TASKMESH/tests/job_tests.cpp#L93-L98).
- `JobTest.InvalidJobIdContainingDisallowedPunctuationThrows` in [tests/job_tests.cpp](file:///C:/Users/VINAY/OneDrive/Desktop/TASKMESH/tests/job_tests.cpp#L100-L107).
- `JobTest.ValidJobIdWithPermittedSymbols` in [tests/job_tests.cpp](file:///C:/Users/VINAY/OneDrive/Desktop/TASKMESH/tests/job_tests.cpp#L109-L113).
- `JobTest.InvalidJobIdTooLongThrows` in [tests/job_tests.cpp](file:///C:/Users/VINAY/OneDrive/Desktop/TASKMESH/tests/job_tests.cpp#L115-L127).

---

## DSA-04: Lifecycle State Transition Validation

### 1. Problem Being Solved
A `Job` moves across discrete states: `Pending`, `Queued`, `Running`, `Completed`, `Failed`, `Cancelled`. Illegal state jumps (such as marking a `Completed` job as `Running`, or jumping directly from `Pending` to `Completed`) must be rejected to preserve domain consistency.

### 2. Selected Data Structure & Algorithm
- **Algorithm**: Constant-time switch-case state-transition table (`is_valid_status_transition`).
- **Storage**: Enum class backed by `uint8_t` (1 byte).

### 3. Workload Fit & Rationale
- The state space is small (6 states).
- A switch-case construct is compiled by MSVC into a jump table or direct bitmask instructions, yielding deterministic $O(1)$ validation with zero memory allocations.

### 4. Complexity Analysis
- **Time Complexity**: $O(1)$ (single instruction branch or jump table).
- **Space Complexity**: $O(1)$ (zero dynamic memory, 1-byte enum values).

### 5. Verification Tests
- `JobTest.ValidLifecycleTransitionsSucceed` in [tests/job_tests.cpp](file:///C:/Users/VINAY/OneDrive/Desktop/TASKMESH/tests/job_tests.cpp#L162-L182).
- `JobTest.IllegalLifecycleTransitionsThrow` in [tests/job_tests.cpp](file:///C:/Users/VINAY/OneDrive/Desktop/TASKMESH/tests/job_tests.cpp#L215-L248).

---

## DSA-05: Planned Structures for Future Phases (Evaluation Preview)

In accordance with the selection rules, future phases will introduce data structures only when specific requirements demand them:

| Future Requirement | Candidate Data Structure | Workload Justification |
|:---|:---|:---|
| **Delayed / Scheduled Execution (Phase 2)** | Min-Heap ordered by execution deadline (`scheduled_time`) | When jobs specify a future execution timestamp, separating immediate jobs (max-heap by priority) from time-delayed jobs (min-heap by time point) allows $O(1)$ deadline inspection via `top()` without scanning. |
| **Concurrent Work Distribution (Phase 2)** | Synchronized bounded queue with `std::mutex` and `std::condition_variable` | Standard containers are not thread-safe. A synchronized queue with condition variables avoids busy-waiting and coordinates worker threads safely. |
| **Job Deduplication (Phase 3)** | Hash index (`std::unordered_set<JobId>` or DB unique constraint) | Fast $O(1)$ duplicate rejection before enqueueing. |
| **Crash Recovery & History (Phase 3)** | B-Tree on disk via relational storage index | Rather than forcing massive historical logs into memory, persistent storage uses indexed B-Trees for range and status scans. |
