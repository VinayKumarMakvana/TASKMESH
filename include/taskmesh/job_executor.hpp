#pragma once

#include "taskmesh/job.hpp"

#include <chrono>
#include <functional>
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
};

/**
 * @brief Abstract interface separating job scheduling from job execution.
 */
class IJobExecutor {
public:
    virtual ~IJobExecutor() = default;

    /**
     * @brief Executes a single job, updating its attempt count and lifecycle status.
     * @param job Reference to the job to be executed.
     * @return ExecutionResult containing outcome and timing details.
     */
    [[nodiscard]] virtual ExecutionResult execute(Job& job) = 0;
};

/**
 * @brief In-memory test and demonstration executor.
 *
 * Dispatches jobs to registered handlers by job type, or uses a default execution strategy.
 * Updates the job lifecycle: transitions to Running, increments attempt count, and sets
 * Completed or Failed according to handler outcome.
 */
class InMemoryJobExecutor : public IJobExecutor {
public:
    using Handler = std::function<ExecutionResult(const Job&)>;

    InMemoryJobExecutor() = default;

    /**
     * @brief Registers a custom handler for a given job type.
     */
    void register_handler(std::string job_type, Handler handler);

    /**
     * @brief Sets a fallback handler for jobs without a specifically registered type handler.
     */
    void set_default_handler(Handler handler);

    /**
     * @brief Executes the job synchronously using the registered or default handler.
     */
    [[nodiscard]] ExecutionResult execute(Job& job) override;

    [[nodiscard]] size_t executed_count() const noexcept { return executed_count_; }
    [[nodiscard]] size_t successful_count() const noexcept { return successful_count_; }
    [[nodiscard]] size_t failed_count() const noexcept { return failed_count_; }

private:
    std::unordered_map<std::string, Handler> handlers_;
    Handler default_handler_;
    size_t executed_count_{0};
    size_t successful_count_{0};
    size_t failed_count_{0};
};

} // namespace taskmesh
