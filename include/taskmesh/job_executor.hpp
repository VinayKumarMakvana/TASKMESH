#pragma once

#include "taskmesh/cancellation_token.hpp"
#include "taskmesh/job.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace taskmesh {

/**
 * @brief Encapsulates the outcome of a job execution attempt.
 */
struct ExecutionResult {
    bool success{false};
    std::string message;
    std::chrono::milliseconds execution_duration{0};

    [[nodiscard]] static ExecutionResult ok(std::string message = "Execution succeeded",
                                           std::chrono::milliseconds duration = std::chrono::milliseconds{0}) {
        return ExecutionResult{true, std::move(message), duration};
    }

    [[nodiscard]] static ExecutionResult error(std::string message,
                                              std::chrono::milliseconds duration = std::chrono::milliseconds{0}) {
        return ExecutionResult{false, std::move(message), duration};
    }

    [[nodiscard]] static ExecutionResult cancelled(std::string message = "Execution cancelled",
                                                  std::chrono::milliseconds duration = std::chrono::milliseconds{0}) {
        return ExecutionResult{false, std::move(message), duration};
    }
};

/**
 * @brief Abstract interface separating job scheduling from job execution.
 *
 * Designed for backward compatibility:
 * - Subclasses only implementing Phase 1 `execute(Job&)` remain fully functional.
 * - Subclasses supporting cancellation override `execute(Job&, const JobExecutionContext&)`.
 */
class IJobExecutor {
public:
    virtual ~IJobExecutor() = default;

    /**
     * @brief Phase 1 pure virtual execution contract.
     */
    [[nodiscard]] virtual ExecutionResult execute(Job& job) = 0;

    /**
     * @brief Phase 2 context-aware execution contract.
     * Default implementation delegates to execute(job) for backward compatibility.
     */
    [[nodiscard]] virtual ExecutionResult execute(Job& job, const JobExecutionContext& context) {
        (void)context;
        return execute(job);
    }
};

/**
 * @brief Thread-safe in-memory job executor.
 *
 * Supports both simple and cancellable handlers. Thread-safe for concurrent invocations
 * from multiple worker threads.
 */
class InMemoryJobExecutor : public IJobExecutor {
public:
    using SimpleHandler = std::function<ExecutionResult(const Job&)>;
    using CancellableHandler = std::function<ExecutionResult(const Job&, const JobExecutionContext&)>;

    InMemoryJobExecutor() = default;

    /**
     * @brief Registers a simple handler (ignores cancellation).
     */
    void register_handler(std::string job_type, SimpleHandler handler);

    /**
     * @brief Registers a cancellable handler.
     */
    void register_handler(std::string job_type, CancellableHandler handler);

    /**
     * @brief Sets a fallback simple handler.
     */
    void set_default_handler(SimpleHandler handler);

    /**
     * @brief Sets a fallback cancellable handler.
     */
    void set_default_handler(CancellableHandler handler);

    // Overrides
    [[nodiscard]] ExecutionResult execute(Job& job) override;
    [[nodiscard]] ExecutionResult execute(Job& job, const JobExecutionContext& context) override;

    [[nodiscard]] uint64_t executed_count() const noexcept { return executed_count_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t successful_count() const noexcept { return successful_count_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t failed_count() const noexcept { return failed_count_.load(std::memory_order_relaxed); }

private:
    mutable std::shared_mutex handlers_mutex_;
    std::unordered_map<std::string, CancellableHandler> handlers_;
    CancellableHandler default_handler_;

    std::atomic<uint64_t> executed_count_{0};
    std::atomic<uint64_t> successful_count_{0};
    std::atomic<uint64_t> failed_count_{0};
};

} // namespace taskmesh
