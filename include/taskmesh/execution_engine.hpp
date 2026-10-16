#pragma once

#include "taskmesh/cancellation_token.hpp"
#include "taskmesh/concurrent_priority_queue.hpp"
#include "taskmesh/job.hpp"
#include "taskmesh/job_executor.hpp"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace taskmesh {

/**
 * @brief Engine configuration options.
 */
struct ExecutionEngineConfig {
    size_t num_workers{4};
    std::chrono::milliseconds worker_poll_timeout{100};
    bool auto_retry{true};
    std::chrono::milliseconds retry_delay{0};
};

/**
 * @brief Shutdown mode for the execution engine.
 */
enum class ShutdownMode {
    GracefulDrain, //!< Finish all queued jobs before terminating workers.
    ImmediateStop  //!< Drop queued jobs, request cancellation on running jobs, and exit immediately.
};

/**
 * @brief Consistent snapshot of execution engine metrics.
 *
 * Invariant: submitted == queued + running + completed + failed + cancelled
 */
struct EngineMetrics {
    uint64_t submitted{0};
    uint64_t queued{0};
    uint64_t running{0};
    uint64_t completed{0};
    uint64_t failed{0};
    uint64_t cancelled{0};
    uint64_t retries{0};

    [[nodiscard]] bool check_invariant() const noexcept {
        return submitted == (queued + running + completed + failed + cancelled);
    }
};

/**
 * @brief Callback invoked whenever a job transitions lifecycle state.
 */
using JobLifecycleCallback = std::function<void(const Job& job, JobStatus new_status, const std::string& message)>;

/**
 * @brief Concurrent multi-threaded execution engine for TaskMesh.
 *
 * Manages worker thread lifecycle, priority job scheduling, cooperative cancellation,
 * graceful draining, and strictly consistent metrics accounting.
 *
 * Lock Hierarchy (must be acquired in this order):
 * 1. lifecycle_mutex_
 * 2. jobs_mutex_
 * 3. metrics_mutex_
 *
 * Queue internal mutex is isolated and never held while holding engine locks.
 * User-supplied job handlers are NEVER executed while holding any engine locks.
 */
class ExecutionEngine {
public:
    explicit ExecutionEngine(IJobExecutor& executor, ExecutionEngineConfig config = {});

    /**
     * @brief Destructor guarantees safe shutdown and join.
     *
     * Invokes shutdown(ShutdownMode::ImmediateStop) and waits for worker threads to join.
     * Note: Workers are never detached to prevent use-after-free of engine state.
     * If user handlers are uncooperative (spin without checking cancellation), the destructor
     * will block until their threads terminate to ensure process safety.
     */
    ~ExecutionEngine();

    // Non-copyable, non-movable synchronization coordinator
    ExecutionEngine(const ExecutionEngine&) = delete;
    ExecutionEngine& operator=(const ExecutionEngine&) = delete;
    ExecutionEngine(ExecutionEngine&&) = delete;
    ExecutionEngine& operator=(ExecutionEngine&&) = delete;

    /**
     * @brief Submits a job to the execution engine.
     *
     * @param job Job to schedule and execute.
     * @return true if accepted into queue, false if engine is stopping or stopped.
     */
    bool submit(Job job);

    /**
     * @brief Requests cooperative cancellation of a job.
     *
     * Linearization:
     * - If job is queued: marked for pre-execution cancellation; worker will drop it at LP3.
     * - If job is running: cancellation token is signaled.
     * - If job is already in a terminal state (Completed, Failed, Cancelled): returns false.
     *
     * @param id The ID of the job to cancel.
     * @return true if cancellation was accepted, false if job is unknown or already terminal.
     */
    bool cancel_job(const JobId& id);

    /**
     * @brief Shuts down the engine.
     *
     * @param mode GracefulDrain waits for queue exhaustion; ImmediateStop cancels immediately.
     */
    void shutdown(ShutdownMode mode = ShutdownMode::GracefulDrain);

    /**
     * @brief Blocks calling thread until all workers have stopped or timeout expires.
     *
     * @param timeout Maximum duration to wait.
     * @return true if all workers stopped, false if timeout expired.
     */
    bool wait_until_stopped(std::chrono::milliseconds timeout = std::chrono::milliseconds::max());

    void set_lifecycle_callback(JobLifecycleCallback callback);

    [[nodiscard]] bool is_running() const noexcept;
    [[nodiscard]] bool is_stopped() const noexcept;
    [[nodiscard]] bool is_job_running(const JobId& id) const noexcept;
    [[nodiscard]] bool is_job_active(const JobId& id) const noexcept;
    [[nodiscard]] EngineMetrics get_metrics() const;
    [[nodiscard]] size_t worker_count() const noexcept;
    [[nodiscard]] size_t queue_size() const noexcept;

private:
    void worker_loop(size_t worker_id);

    IJobExecutor& executor_;
    ExecutionEngineConfig config_;
    JobLifecycleCallback lifecycle_callback_{nullptr};

    // 1. Lifecycle management
    mutable std::mutex lifecycle_mutex_;
    bool running_{true};
    bool draining_{false};
    bool stopped_{false};
    std::vector<std::jthread> workers_;
    size_t active_workers_{0};
    std::condition_variable stopped_cv_;

    // 2. Priority queue
    ConcurrentPriorityQueue queue_;

    // 3. Job tracking and cancellation registry
    mutable std::mutex jobs_mutex_;
    std::unordered_map<JobId, std::shared_ptr<CancellationSource>> running_jobs_;
    std::unordered_set<JobId> cancelled_before_execution_;
    std::unordered_set<JobId> terminal_jobs_;
    std::unordered_set<JobId> submitted_job_ids_;
    std::unordered_set<JobId> active_job_ids_;

    // 4. Thread-safe metrics accounting
    mutable std::mutex metrics_mutex_;
    EngineMetrics metrics_;
};

} // namespace taskmesh
