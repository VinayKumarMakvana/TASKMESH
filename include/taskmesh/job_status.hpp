#pragma once

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string_view>

namespace taskmesh {

/**
 * @brief Represents the lifecycle states of a Job in TaskMesh.
 */
enum class JobStatus : uint8_t {
    Pending = 0,    ///< Created but not yet submitted to the scheduler.
    Queued = 1,     ///< Accepted by scheduler and waiting in queue.
    Running = 2,    ///< Currently executing by a worker.
    Completed = 3,  ///< Execution finished successfully (terminal state).
    Failed = 4,     ///< Execution failed.
    Cancelled = 5   ///< Cancelled before completion (terminal state).
};

/**
 * @brief Returns the human-readable string representation of a JobStatus.
 */
[[nodiscard]] constexpr std::string_view to_string(JobStatus status) noexcept {
    switch (status) {
        case JobStatus::Pending:
            return "Pending";
        case JobStatus::Queued:
            return "Queued";
        case JobStatus::Running:
            return "Running";
        case JobStatus::Completed:
            return "Completed";
        case JobStatus::Failed:
            return "Failed";
        case JobStatus::Cancelled:
            return "Cancelled";
    }
    return "Unknown";
}

/**
 * @brief Parses a JobStatus from a string representation.
 */
[[nodiscard]] std::optional<JobStatus> job_status_from_string(std::string_view str) noexcept;

/**
 * @brief Checks if the status represents a permanent terminal state from which no further transitions can occur.
 */
[[nodiscard]] constexpr bool is_terminal_status(JobStatus status) noexcept {
    return status == JobStatus::Completed || status == JobStatus::Cancelled;
}

/**
 * @brief Validates if a lifecycle transition from current to target status is permitted.
 */
[[nodiscard]] bool is_valid_status_transition(JobStatus current, JobStatus target) noexcept;

std::ostream& operator<<(std::ostream& os, JobStatus status);

} // namespace taskmesh
