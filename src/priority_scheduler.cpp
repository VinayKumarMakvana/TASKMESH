#include "taskmesh/priority_scheduler.hpp"

#include <utility>

namespace taskmesh {

PriorityScheduler::PriorityScheduler(PriorityScheduler&& other) noexcept
    : queue_(std::move(other.queue_)),
      sequence_counter_(std::exchange(other.sequence_counter_, 0)) {}

PriorityScheduler& PriorityScheduler::operator=(PriorityScheduler&& other) noexcept {
    if (this != &other) {
        queue_ = std::move(other.queue_);
        sequence_counter_ = std::exchange(other.sequence_counter_, 0);
    }
    return *this;
}

void PriorityScheduler::enqueue(Job job) {
    if (job.id().empty()) {
        throw JobValidationError("Cannot enqueue job with empty identifier.");
    }

    if (is_terminal_status(job.status())) {
        throw JobValidationError("Cannot enqueue job in terminal status ('" + 
                                 std::string(to_string(job.status())) + "').");
    }

    if (job.status() == JobStatus::Running) {
        throw JobValidationError("Cannot enqueue job that is already in 'Running' status.");
    }

    // M-01: Disallow enqueuing Failed jobs that have exhausted all retry attempts
    if (job.status() == JobStatus::Failed && !job.can_retry()) {
        throw JobValidationError("Cannot enqueue failed job: retry attempts exhausted (" + 
                                 std::to_string(job.attempt_count()) + "/" + 
                                 std::to_string(job.max_attempts()) + ").");
    }

    if (sequence_counter_ == std::numeric_limits<uint64_t>::max()) {
        throw std::overflow_error("PriorityScheduler sequence counter overflow: maximum limit reached.");
    }

    // Mark job as queued in scheduler (only done after all validation checks pass)
    job.mark_queued();

    const uint64_t seq = sequence_counter_;
    queue_.push(ScheduledJobEntry{std::move(job), seq});
    ++sequence_counter_;
}

std::optional<Job> PriorityScheduler::dequeue() {
    if (queue_.empty()) {
        return std::nullopt;
    }
    // L-02: std::priority_queue::top() returns a const reference. To preserve strict
    // const-correctness and avoid fragile/undefined const_cast operations on container
    // internal heap nodes, the element is safely copied before removal.
    Job job = queue_.top().job;
    queue_.pop();
    return job;
}

std::optional<Job> PriorityScheduler::peek() const {
    if (queue_.empty()) {
        return std::nullopt;
    }
    return queue_.top().job;
}

const Job* PriorityScheduler::peek_ptr() const noexcept {
    if (queue_.empty()) {
        return nullptr;
    }
    return &queue_.top().job;
}

void PriorityScheduler::clear() noexcept {
    while (!queue_.empty()) {
        queue_.pop();
    }
    sequence_counter_ = 0;
}

} // namespace taskmesh
