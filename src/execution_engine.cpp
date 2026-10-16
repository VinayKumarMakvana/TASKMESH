#include "taskmesh/execution_engine.hpp"

#include <iostream>
#include <stdexcept>

namespace taskmesh {

ExecutionEngine::ExecutionEngine(IJobExecutor& executor, ExecutionEngineConfig config)
    : executor_(executor), config_(config) {
    if (config_.num_workers == 0) {
        config_.num_workers = 1;
    }

    std::lock_guard<std::mutex> lock(lifecycle_mutex_);
    active_workers_ = config_.num_workers;
    workers_.reserve(config_.num_workers);
    for (size_t i = 0; i < config_.num_workers; ++i) {
        workers_.emplace_back([this, i]() {
            worker_loop(i);
        });
    }
}

ExecutionEngine::~ExecutionEngine() {
    shutdown(ShutdownMode::ImmediateStop);
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

bool ExecutionEngine::submit(Job job) {
    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        if (!running_ || draining_ || stopped_) {
            return false;
        }
    }

    const JobId id = job.id();

    if (job.status() == JobStatus::Pending) {
        try {
            job.mark_queued();
        } catch (const std::exception&) {
            return false;
        }
    }

    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        if (active_job_ids_.contains(id) || running_jobs_.contains(id)) {
            return false;
        }
        terminal_jobs_.erase(id);
        cancelled_before_execution_.erase(id);
        submitted_job_ids_.insert(id);
        active_job_ids_.insert(id);
    }

    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.submitted++;
        metrics_.queued++;
    }

    try {
        queue_.push(std::move(job));
    } catch (const std::exception&) {
        {
            std::lock_guard<std::mutex> lock(jobs_mutex_);
            submitted_job_ids_.erase(id);
            active_job_ids_.erase(id);
        }
        {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            metrics_.submitted--;
            metrics_.queued--;
        }
        return false;
    }

    return true;
}

bool ExecutionEngine::cancel_job(const JobId& id) {
    std::shared_ptr<CancellationSource> cancel_source;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        // If already terminal, cannot cancel
        if (terminal_jobs_.contains(id)) {
            return false;
        }

        // If never submitted, cannot cancel
        if (!submitted_job_ids_.contains(id)) {
            return false;
        }

        // If currently running, signal cancellation source
        auto it = running_jobs_.find(id);
        if (it != running_jobs_.end()) {
            cancel_source = it->second;
        } else {
            // Job is queued, mark for pre-execution cancellation
            cancelled_before_execution_.insert(id);
            return true;
        }
    }

    if (cancel_source) {
        return cancel_source->request_cancellation();
    }

    return false;
}

void ExecutionEngine::shutdown(ShutdownMode mode) {
    std::vector<Job> drained_jobs;
    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        if (stopped_) {
            return;
        }
        if (mode == ShutdownMode::GracefulDrain) {
            draining_ = true;
            queue_.close(true);
        } else { // ImmediateStop
            running_ = false;
            draining_ = false;
            queue_.close(false);
            drained_jobs = queue_.drain_all();
        }
    }

    if (mode == ShutdownMode::ImmediateStop) {
        // 1. Signal cancellation to all currently running jobs
        {
            std::lock_guard<std::mutex> lock(jobs_mutex_);
            for (auto& [id, cancel_src] : running_jobs_) {
                cancel_src->request_cancellation();
            }
            // Mark all drained jobs as terminal / cancelled
            for (const auto& job : drained_jobs) {
                cancelled_before_execution_.insert(job.id());
                active_job_ids_.erase(job.id());
                terminal_jobs_.insert(job.id());
            }
        }

        // 2. Account for drained jobs in metrics
        if (!drained_jobs.empty()) {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            for (size_t i = 0; i < drained_jobs.size(); ++i) {
                if (metrics_.queued > 0) {
                    metrics_.queued--;
                    metrics_.cancelled++;
                }
            }
        }
    }
}

bool ExecutionEngine::wait_until_stopped(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(lifecycle_mutex_);
    if (stopped_) {
        return true;
    }
    return stopped_cv_.wait_for(lock, timeout, [this] {
        return stopped_;
    });
}

bool ExecutionEngine::is_running() const noexcept {
    std::lock_guard<std::mutex> lock(lifecycle_mutex_);
    return running_ && !stopped_;
}

bool ExecutionEngine::is_stopped() const noexcept {
    std::lock_guard<std::mutex> lock(lifecycle_mutex_);
    return stopped_;
}

bool ExecutionEngine::is_job_running(const JobId& id) const noexcept {
    std::lock_guard<std::mutex> lock(jobs_mutex_);
    return running_jobs_.contains(id);
}

bool ExecutionEngine::is_job_active(const JobId& id) const noexcept {
    std::lock_guard<std::mutex> lock(jobs_mutex_);
    return active_job_ids_.contains(id) || running_jobs_.contains(id);
}

EngineMetrics ExecutionEngine::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_;
}

void ExecutionEngine::set_lifecycle_callback(JobLifecycleCallback callback) {
    lifecycle_callback_ = std::move(callback);
}

size_t ExecutionEngine::worker_count() const noexcept {
    return config_.num_workers;
}

size_t ExecutionEngine::queue_size() const noexcept {
    return queue_.size();
}

void ExecutionEngine::worker_loop(size_t worker_id) {
    while (true) {
        // Check lifecycle status
        {
            std::lock_guard<std::mutex> lock(lifecycle_mutex_);
            if (stopped_ || (!running_ && !draining_)) {
                break;
            }
        }

        // LP 2: Pop from queue (blocking with poll timeout)
        auto job_opt = queue_.pop_wait(config_.worker_poll_timeout);
        if (!job_opt.has_value()) {
            std::lock_guard<std::mutex> lock(lifecycle_mutex_);
            if (stopped_ || (!running_ && !draining_)) {
                break;
            }
            if (draining_) {
                bool has_active_work = false;
                {
                    std::lock_guard<std::mutex> m_lock(metrics_mutex_);
                    has_active_work = (metrics_.running > 0) || (metrics_.queued > 0);
                }
                if (!has_active_work && queue_.empty()) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            continue;
        }

        Job job = std::move(*job_opt);
        const JobId id = job.id();

        // Atomically transition queued -> running in metrics
        {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            if (metrics_.queued > 0) {
                metrics_.queued--;
            }
            metrics_.running++;
        }

        // LP 3: Pre-execution gate
        std::shared_ptr<CancellationSource> cancel_source;
        bool cancelled_pre_exec = false;
        {
            std::lock_guard<std::mutex> lock(jobs_mutex_);
            if (cancelled_before_execution_.contains(id)) {
                cancelled_pre_exec = true;
                cancelled_before_execution_.erase(id);
                active_job_ids_.erase(id);
                terminal_jobs_.insert(id);
            } else {
                cancel_source = std::make_shared<CancellationSource>();
                running_jobs_[id] = cancel_source;
            }
        }

        if (cancelled_pre_exec) {
            // LP 4: Terminal commitment for pre-execution cancellation
            job.mark_cancelled("Cancelled prior to execution start.");
            {
                std::lock_guard<std::mutex> lock(metrics_mutex_);
                metrics_.running--;
                metrics_.cancelled++;
            }
            if (lifecycle_callback_) {
                try { lifecycle_callback_(job, JobStatus::Cancelled, "Cancelled prior to execution start."); } catch (...) {}
            }
            continue;
        }

        try {
            if (job.status() != JobStatus::Running) {
                job.mark_running();
            }
        } catch (...) {}

        if (lifecycle_callback_) {
            try { lifecycle_callback_(job, JobStatus::Running, ""); } catch (...) {}
        }

        // Create execution context
        JobExecutionContext ctx(cancel_source->get_token(), worker_id);
        const size_t prev_attempts = job.attempt_count();

        // Execute handler WITHOUT holding ANY locks
        ExecutionResult result;
        try {
            result = executor_.execute(job, ctx);
        } catch (const std::exception& ex) {
            result = ExecutionResult::error(std::string("Unhandled exception in handler: ") + ex.what());
        } catch (...) {
            result = ExecutionResult::error("Unhandled unknown exception in handler.");
        }

        // Ensure attempt count was tracked even if legacy executor didn't call increment
        if (job.attempt_count() == prev_attempts && job.attempt_count() < job.max_attempts()) {
            try { job.increment_attempt_count(); } catch (...) {}
        }

        // Process execution outcome
        if (ctx.is_cancellation_requested()) {
            job.mark_cancelled("Execution cancelled.");
            {
                std::lock_guard<std::mutex> lock(jobs_mutex_);
                running_jobs_.erase(id);
                active_job_ids_.erase(id);
                terminal_jobs_.insert(id);
            }
            {
                std::lock_guard<std::mutex> lock(metrics_mutex_);
                metrics_.running--;
                metrics_.cancelled++;
            }
            if (lifecycle_callback_) {
                try { lifecycle_callback_(job, JobStatus::Cancelled, "Execution cancelled."); } catch (...) {}
            }
        } else if (result.success) {
            job.mark_completed();
            {
                std::lock_guard<std::mutex> lock(jobs_mutex_);
                running_jobs_.erase(id);
                active_job_ids_.erase(id);
                terminal_jobs_.insert(id);
            }
            {
                std::lock_guard<std::mutex> lock(metrics_mutex_);
                metrics_.running--;
                metrics_.completed++;
            }
            if (lifecycle_callback_) {
                try { lifecycle_callback_(job, JobStatus::Completed, result.message); } catch (...) {}
            }
        } else {
            // Execution failed
            std::string err = result.message.empty() ? "Execution failed" : result.message;
            job.mark_failed(err);

            const bool can_retry = (job.attempt_count() < job.max_attempts()) && config_.auto_retry;
            bool retry_pushed = false;
            if (can_retry) {
                // Re-enqueue for retry
                {
                    std::lock_guard<std::mutex> lock(jobs_mutex_);
                    running_jobs_.erase(id);
                }

                job.mark_queued();

                if (config_.retry_delay.count() > 0) {
                    std::this_thread::sleep_for(config_.retry_delay);
                }

                try {
                    queue_.push_retry(job);
                    retry_pushed = true;

                    {
                        std::lock_guard<std::mutex> lock(metrics_mutex_);
                        metrics_.running--;
                        metrics_.queued++;
                        metrics_.retries++;
                    }

                    if (lifecycle_callback_) {
                        std::string msg = "Retry scheduled (attempt " + std::to_string(job.attempt_count()) + "/" + std::to_string(job.max_attempts()) + "): " + err;
                        try { lifecycle_callback_(job, JobStatus::Queued, msg); } catch (...) {}
                    }
                } catch (...) {
                    retry_pushed = false;
                }
            }

            if (!retry_pushed) {
                // Attempts exhausted: final terminal commitment
                {
                    std::lock_guard<std::mutex> lock(jobs_mutex_);
                    running_jobs_.erase(id);
                    active_job_ids_.erase(id);
                    terminal_jobs_.insert(id);
                }

                {
                    std::lock_guard<std::mutex> lock(metrics_mutex_);
                    metrics_.running--;
                    metrics_.failed++;
                }

                if (lifecycle_callback_) {
                    try { lifecycle_callback_(job, JobStatus::Failed, err); } catch (...) {}
                }
            }
        }
    }

    // Worker exit: decrement active workers under lifecycle lock and notify if all done
    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        if (active_workers_ > 0) {
            active_workers_--;
        }
        if (active_workers_ == 0) {
            stopped_ = true;
            stopped_cv_.notify_all();
        }
    }
}

} // namespace taskmesh
