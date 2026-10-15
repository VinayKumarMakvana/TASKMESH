#pragma once

#include "taskmesh/job.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <vector>

namespace taskmesh {

/**
 * @brief In-memory priority job scheduler using a binary max-heap.
 *
 * Scheduling Policy:
 * 1. Higher-priority jobs are dequeued strictly before lower-priority jobs.
 * 2. Equal-priority jobs preserve strict deterministic FIFO (First-In, First-Out) ordering,
 *    enforced via an internal 64-bit monotonically increasing sequence counter.
 *
 * Complexity Guarantees:
 * - enqueue(job): O(log N) where N is the current queue size.
 * - dequeue():     O(log N)
 * - peek():        O(1)
 * - size():        O(1)
 * - empty():       O(1)
 *
 * Space Complexity:
 * - O(N) contiguous storage inside the underlying vector backing the binary heap.
 *
 * Sequence Counter Overflow:
 * The sequence counter uses an unsigned 64-bit integer (`uint64_t`). At 1 billion operations
 * per second, an overflow would take ~584 years of continuous operation. If the sequence
 * counter ever reaches std::numeric_limits<uint64_t>::max(), enqueue() throws std::overflow_error
 * to prevent silent wrap-around that would violate the FIFO invariant.
 *
 * Thread Safety Note:
 * This scheduler implementation is intentionally single-threaded and NOT thread-safe.
 * Concurrent access without external synchronization will produce data races. Thread-safe
 * concurrent queues and worker execution pools are designed for Phase 2.
 */
class PriorityScheduler {
public:
    PriorityScheduler() = default;
    ~PriorityScheduler() = default;

    // Non-copyable (queue state is unique), but movable
    PriorityScheduler(const PriorityScheduler&) = delete;
    PriorityScheduler& operator=(const PriorityScheduler&) = delete;
    PriorityScheduler(PriorityScheduler&&) noexcept = default;
    PriorityScheduler& operator=(PriorityScheduler&&) noexcept = default;

    /**
     * @brief Enqueues a job into the priority scheduler.
     *
     * Validates the job, assigns a monotonic sequence counter, transitions
     * job status to JobStatus::Queued, and inserts it into the priority heap.
     *
     * @param job The job to enqueue.
     * @throws JobValidationError or std::invalid_argument if the job is invalid or in a terminal/running status.
     * @throws std::overflow_error if the 64-bit monotonic sequence counter overflows.
     */
    void enqueue(Job job);

    /**
     * @brief Dequeues the highest-priority job from the scheduler.
     *
     * If multiple jobs share the highest priority, the one enqueued earliest (FIFO) is returned.
     *
     * @return std::optional containing the highest-priority Job, or std::nullopt if queue is empty.
     */
    [[nodiscard]] std::optional<Job> dequeue();

    /**
     * @brief Peeks at the highest-priority job without removing it from the queue.
     *
     * @return std::optional containing a copy of the highest-priority Job, or std::nullopt if empty.
     */
    [[nodiscard]] std::optional<Job> peek() const;

    /**
     * @brief Returns a non-owning pointer to the highest-priority job without removing it.
     *
     * @return Pointer to top Job, or nullptr if queue is empty.
     */
    [[nodiscard]] const Job* peek_ptr() const noexcept;

    /**
     * @brief Returns the total number of jobs currently in the scheduler.
     */
    [[nodiscard]] size_t size() const noexcept { return queue_.size(); }

    /**
     * @brief Checks whether the scheduler contains any jobs.
     */
    [[nodiscard]] bool empty() const noexcept { return queue_.empty(); }

    /**
     * @brief Clears all scheduled jobs and resets the sequence counter.
     */
    void clear() noexcept;

    /**
     * @brief Returns the current sequence counter value (useful for diagnostics and metrics).
     */
    [[nodiscard]] uint64_t current_sequence_number() const noexcept { return sequence_counter_; }

private:
    struct ScheduledJobEntry {
        Job job;
        uint64_t sequence_number;
    };

    struct ScheduledJobComparator {
        bool operator()(const ScheduledJobEntry& lhs, const ScheduledJobEntry& rhs) const noexcept {
            // Max-heap ordering: operator() returns true if lhs has lower precedence than rhs.
            if (lhs.job.priority() != rhs.job.priority()) {
                return lhs.job.priority() < rhs.job.priority();
            }
            // Equal priority: FIFO tie-break.
            // A smaller sequence number indicates earlier arrival and must have higher precedence.
            // Therefore, lhs has lower precedence than rhs if lhs.sequence_number > rhs.sequence_number.
            return lhs.sequence_number > rhs.sequence_number;
        }
    };

    std::priority_queue<ScheduledJobEntry, std::vector<ScheduledJobEntry>, ScheduledJobComparator> queue_;
    uint64_t sequence_counter_{0};
};

} // namespace taskmesh
