#pragma once

#include "taskmesh/job.hpp"
#include "taskmesh/priority_scheduler.hpp"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <vector>

namespace taskmesh {

/**
 * @brief Thread-safe priority queue wrapping PriorityScheduler.
 *
 * Implements non-busy condition variable waiting, timeouts, graceful draining,
 * and immediate stop purging.
 */
class ConcurrentPriorityQueue {
public:
    ConcurrentPriorityQueue() = default;
    ~ConcurrentPriorityQueue() = default;

    // Non-copyable, non-movable synchronization boundary
    ConcurrentPriorityQueue(const ConcurrentPriorityQueue&) = delete;
    ConcurrentPriorityQueue& operator=(const ConcurrentPriorityQueue&) = delete;
    ConcurrentPriorityQueue(ConcurrentPriorityQueue&&) = delete;
    ConcurrentPriorityQueue& operator=(ConcurrentPriorityQueue&&) = delete;

    /**
     * @brief Thread-safe enqueue.
     * @throws std::runtime_error if the queue is closed or draining.
     */
    void push(Job job);

    /**
     * @brief Thread-safe re-enqueue for retries during active or draining states.
     * @throws std::runtime_error only if the queue is closed (ImmediateStop).
     */
    void push_retry(Job job);

    /**
     * @brief Non-blocking pop.
     */
    [[nodiscard]] std::optional<Job> try_pop();

    /**
     * @brief Blocking pop with optional timeout.
     * Suspends calling worker on condition variable without CPU spinning.
     */
    [[nodiscard]] std::optional<Job> pop_wait(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * @brief Closes the queue.
     * @param drain If true, allows existing queued jobs to be popped before closing.
     *              If false, closes immediately.
     */
    void close(bool drain = true);

    /**
     * @brief Extracts and returns all remaining jobs currently in the queue.
     * Used by ImmediateStop to account for discarded jobs.
     */
    [[nodiscard]] std::vector<Job> drain_all();

    [[nodiscard]] bool is_closed() const noexcept;
    [[nodiscard]] bool is_draining() const noexcept;
    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    void clear() noexcept;

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    PriorityScheduler scheduler_;
    bool closed_{false};
    bool draining_{false};
};

} // namespace taskmesh
