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

    // Invariant check: job must be capable of execution
    if (job.status() != JobStatus::Queued && job.status() != JobStatus::Pending) {
        ExecutionResult res = ExecutionResult::error(
            "Cannot execute job: status must be Pending or Queued, but is " + 
            std::string(to_string(job.status())));
        job.mark_failed(res.message);
        ++failed_count_;
        return res;
    }

    // Transition job to Running and increment attempts
    job.increment_attempt_count();
    job.mark_running();

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
