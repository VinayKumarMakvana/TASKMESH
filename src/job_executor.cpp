#include "taskmesh/job_executor.hpp"

namespace taskmesh {

void InMemoryJobExecutor::register_handler(std::string job_type, Handler handler) {
    handlers_[std::move(job_type)] = std::move(handler);
}

void InMemoryJobExecutor::set_default_handler(Handler handler) {
    default_handler_ = std::move(handler);
}

ExecutionResult InMemoryJobExecutor::execute(Job& job) {
    ++executed_count_;

    // H-01: Invariant check - job must be in an executable state (Pending or Queued).
    // If not executable, reject immediately without mutating the job's lifecycle status.
    if (job.status() != JobStatus::Queued && job.status() != JobStatus::Pending) {
        ++failed_count_;
        return ExecutionResult::error(
            "Cannot execute job '" + job.id().value() + "': status must be Pending or Queued, but is " + 
            std::string(to_string(job.status())));
    }

    // H-02: Check attempt eligibility before mutating state.
    // An attempt cannot be started if attempt_count has already reached max_attempts.
    if (job.attempt_count() >= job.max_attempts()) {
        ++failed_count_;
        return ExecutionResult::error(
            "Cannot execute job '" + job.id().value() + "': attempt limit reached (" + 
            std::to_string(job.attempt_count()) + "/" + std::to_string(job.max_attempts()) + ").");
    }

    // Guard pre-execution lifecycle transitions
    try {
        job.increment_attempt_count();
        job.mark_running();
    } catch (const std::exception& ex) {
        ++failed_count_;
        return ExecutionResult::error(std::string("Pre-execution transition failed: ") + ex.what());
    }

    const auto start_time = std::chrono::steady_clock::now();

    ExecutionResult result;
    try {
        auto it = handlers_.find(job.type());
        if (it != handlers_.end()) {
            result = it->second(job);
        } else if (default_handler_) {
            result = default_handler_(job);
        } else {
            result = ExecutionResult::ok("Processed successfully by InMemoryJobExecutor fallback handler.");
        }
    } catch (const std::exception& ex) {
        result = ExecutionResult::error(std::string("Execution failed due to exception: ") + ex.what());
    } catch (...) {
        result = ExecutionResult::error("Execution failed due to unknown exception.");
    }

    const auto end_time = std::chrono::steady_clock::now();
    result.execution_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    if (result.success) {
        job.mark_completed();
        ++successful_count_;
    } else {
        job.mark_failed(result.message);
        ++failed_count_;
    }

    return result;
}

} // namespace taskmesh
