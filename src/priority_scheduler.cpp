#include "taskmesh/priority_scheduler.hpp"

namespace taskmesh {

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

    if (sequence_counter_ == std::numeric_limits<uint64_t>::max()) {
        throw std::overflow_error("PriorityScheduler sequence counter overflow: maximum limit reached.");
    }

    // Mark job as queued in scheduler
    job.mark_queued();

    const uint64_t seq = sequence_counter_++;
    queue_.push(ScheduledJobEntry{std::move(job), seq});
}

std::optional<Job> PriorityScheduler::dequeue() {
    if (queue_.empty()) {
        return std::nullopt;
    }
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
