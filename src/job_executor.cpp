#include "taskmesh/job_executor.hpp"

#include <mutex>

namespace taskmesh {

void InMemoryJobExecutor::register_handler(std::string job_type, SimpleHandler handler) {
    std::unique_lock<std::shared_mutex> lock(handlers_mutex_);
    handlers_[std::move(job_type)] = [h = std::move(handler)](const Job& job, const JobExecutionContext&) {
        return h(job);
    };
}

void InMemoryJobExecutor::register_handler(std::string job_type, CancellableHandler handler) {
    std::unique_lock<std::shared_mutex> lock(handlers_mutex_);
    handlers_[std::move(job_type)] = std::move(handler);
}

void InMemoryJobExecutor::set_default_handler(SimpleHandler handler) {
    std::unique_lock<std::shared_mutex> lock(handlers_mutex_);
    default_handler_ = [h = std::move(handler)](const Job& job, const JobExecutionContext&) {
        return h(job);
    };
}

void InMemoryJobExecutor::set_default_handler(CancellableHandler handler) {
    std::unique_lock<std::shared_mutex> lock(handlers_mutex_);
    default_handler_ = std::move(handler);
}

ExecutionResult InMemoryJobExecutor::execute(Job& job) {
    return execute(job, JobExecutionContext::empty());
}

ExecutionResult InMemoryJobExecutor::execute(Job& job, const JobExecutionContext& context) {
    executed_count_.fetch_add(1, std::memory_order_relaxed);

    // Invariant check - job must be in an executable state (Pending, Queued, or Running).
    if (job.status() != JobStatus::Queued && job.status() != JobStatus::Pending && job.status() != JobStatus::Running) {
        failed_count_.fetch_add(1, std::memory_order_relaxed);
        return ExecutionResult::error(
            "Cannot execute job '" + job.id().value() + "': status must be Pending or Queued, but is " + 
            std::string(to_string(job.status())));
    }

    // H-02: Check attempt eligibility before mutating state.
    if (job.attempt_count() >= job.max_attempts()) {
        failed_count_.fetch_add(1, std::memory_order_relaxed);
        return ExecutionResult::error(
            "Cannot execute job '" + job.id().value() + "': attempt limit reached (" + 
            std::to_string(job.attempt_count()) + "/" + std::to_string(job.max_attempts()) + ").");
    }

    // Check cancellation before starting execution
    if (context.is_cancellation_requested()) {
        job.mark_cancelled("Cancellation requested prior to handler start.");
        failed_count_.fetch_add(1, std::memory_order_relaxed);
        return ExecutionResult::cancelled("Job cancelled before execution.");
    }

    // Guard pre-execution lifecycle transitions
    try {
        job.increment_attempt_count();
        if (job.status() != JobStatus::Running) {
            job.mark_running();
        }
    } catch (const std::exception& ex) {
        failed_count_.fetch_add(1, std::memory_order_relaxed);
        return ExecutionResult::error(std::string("Pre-execution transition failed: ") + ex.what());
    }

    // Copy handler reference under shared lock
    CancellableHandler handler_to_run;
    {
        std::shared_lock<std::shared_mutex> lock(handlers_mutex_);
        auto it = handlers_.find(job.type());
        if (it != handlers_.end()) {
            handler_to_run = it->second;
        } else if (default_handler_) {
            handler_to_run = default_handler_;
        }
    }

    const auto start_time = std::chrono::steady_clock::now();

    ExecutionResult result;
    try {
        if (handler_to_run) {
            result = handler_to_run(job, context);
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
        successful_count_.fetch_add(1, std::memory_order_relaxed);
    } else {
        if (context.is_cancellation_requested()) {
            job.mark_cancelled(result.message);
        } else {
            job.mark_failed(result.message);
        }
        failed_count_.fetch_add(1, std::memory_order_relaxed);
    }

    return result;
}

} // namespace taskmesh
