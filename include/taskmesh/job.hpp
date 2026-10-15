#pragma once

#include "taskmesh/job_status.hpp"

#include <chrono>
#include <compare>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace taskmesh {

/**
 * @brief Exception thrown when job validation fails or an illegal domain transition is requested.
 */
class JobValidationError : public std::invalid_argument {
public:
    explicit JobValidationError(const std::string& message)
        : std::invalid_argument(message) {}
    explicit JobValidationError(const char* message)
        : std::invalid_argument(message) {}
};

/**
 * @brief Strongly typed Job Identifier.
 *
 * Valid identifiers must be non-empty, at most 128 characters, and contain only
 * alphanumeric characters and safe punctuation: [a-zA-Z0-9_\\-\\.:].
 */
class JobId {
public:
    JobId() = default;
    explicit JobId(std::string id);
    explicit JobId(std::string_view id);
    explicit JobId(const char* id);

    [[nodiscard]] const std::string& value() const noexcept { return id_; }
    [[nodiscard]] std::string_view view() const noexcept { return id_; }
    [[nodiscard]] bool empty() const noexcept { return id_.empty(); }

    [[nodiscard]] bool operator==(const JobId& other) const noexcept = default;
    [[nodiscard]] auto operator<=>(const JobId& other) const noexcept = default;

    friend std::ostream& operator<<(std::ostream& os, const JobId& id);

    static void validate(std::string_view id);

private:
    std::string id_;
};

/**
 * @brief Predefined standard priority levels. Higher numerical value indicates higher priority.
 */
enum class PriorityLevel : int32_t {
    Low = 10,
    Normal = 50,
    High = 100,
    Critical = 200
};

/**
 * @brief Strongly typed Job Priority.
 *
 * Allows predefined priority levels (Low, Normal, High, Critical) as well as
 * arbitrary integer priorities. Comparison semantics guarantee that higher numerical values
 * have strictly higher scheduling precedence.
 */
class JobPriority {
public:
    constexpr JobPriority() noexcept : value_(static_cast<int32_t>(PriorityLevel::Normal)) {}
    constexpr JobPriority(PriorityLevel level) noexcept : value_(static_cast<int32_t>(level)) {}
    constexpr explicit JobPriority(int32_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr int32_t value() const noexcept { return value_; }

    [[nodiscard]] constexpr auto operator<=>(const JobPriority& other) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(const JobPriority& other) const noexcept = default;

    [[nodiscard]] std::string to_string() const;

    static const JobPriority Low;
    static const JobPriority Normal;
    static const JobPriority High;
    static const JobPriority Critical;

    friend std::ostream& operator<<(std::ostream& os, const JobPriority& priority);

private:
    int32_t value_;
};

/**
 * @brief Strongly typed Job domain entity.
 *
 * Encapsulates job metadata, priority, execution parameters, status, attempt counts,
 * and error details. Supports value semantics and enforces domain invariants.
 */
class Job {
public:
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;

    class Builder;

    Job() = default;

    /**
     * @brief Direct constructor with full invariant validation.
     */
    Job(JobId id,
        std::string type,
        std::string payload,
        JobPriority priority = JobPriority::Normal,
        TimePoint creation_time = Clock::now(),
        std::optional<TimePoint> scheduled_time = std::nullopt,
        JobStatus status = JobStatus::Pending,
        uint32_t attempt_count = 0,
        uint32_t max_attempts = 1,
        std::optional<std::string> error_details = std::nullopt);

    // Accessors
    [[nodiscard]] const JobId& id() const noexcept { return id_; }
    [[nodiscard]] const std::string& type() const noexcept { return type_; }
    [[nodiscard]] const std::string& payload() const noexcept { return payload_; }
    [[nodiscard]] JobPriority priority() const noexcept { return priority_; }
    [[nodiscard]] TimePoint creation_time() const noexcept { return creation_time_; }
    [[nodiscard]] const std::optional<TimePoint>& scheduled_time() const noexcept { return scheduled_time_; }
    [[nodiscard]] JobStatus status() const noexcept { return status_; }
    [[nodiscard]] uint32_t attempt_count() const noexcept { return attempt_count_; }
    [[nodiscard]] uint32_t max_attempts() const noexcept { return max_attempts_; }
    [[nodiscard]] const std::optional<std::string>& error_details() const noexcept { return error_details_; }

    // Predicates
    [[nodiscard]] bool can_retry() const noexcept {
        return status_ == JobStatus::Failed && attempt_count_ < max_attempts_;
    }

    [[nodiscard]] bool is_terminal() const noexcept {
        return is_terminal_status(status_);
    }

    // Lifecycle state mutations (enforces valid transition graph)
    void set_status(JobStatus new_status);
    void mark_queued();
    void mark_running();
    void mark_completed();
    void mark_failed(std::string error_message);
    void mark_cancelled(std::string reason = "");
    void increment_attempt_count();

    // Value comparisons
    [[nodiscard]] bool operator==(const Job& other) const noexcept;

    friend std::ostream& operator<<(std::ostream& os, const Job& job);

private:
    void validate_invariants() const;

    JobId id_;
    std::string type_;
    std::string payload_;
    JobPriority priority_{JobPriority::Normal};
    TimePoint creation_time_{Clock::now()};
    std::optional<TimePoint> scheduled_time_{std::nullopt};
    JobStatus status_{JobStatus::Pending};
    uint32_t attempt_count_{0};
    uint32_t max_attempts_{1};
    std::optional<std::string> error_details_{std::nullopt};
};

/**
 * @brief Fluent Builder for constructing Job objects with explicit validation.
 */
class Job::Builder {
public:
    Builder() = default;

    Builder& with_id(JobId id);
    Builder& with_id(std::string id);
    Builder& with_type(std::string type);
    Builder& with_payload(std::string payload);
    Builder& with_priority(JobPriority priority);
    Builder& with_creation_time(TimePoint creation_time);
    Builder& with_scheduled_time(TimePoint scheduled_time);
    Builder& with_status(JobStatus status);
    Builder& with_max_attempts(uint32_t max_attempts);
    Builder& with_attempt_count(uint32_t attempt_count);
    Builder& with_error_details(std::string error_details);

    [[nodiscard]] Job build() const;

private:
    JobId id_;
    std::string type_;
    std::string payload_;
    JobPriority priority_{JobPriority::Normal};
    TimePoint creation_time_{Clock::now()};
    std::optional<TimePoint> scheduled_time_{std::nullopt};
    JobStatus status_{JobStatus::Pending};
    uint32_t attempt_count_{0};
    uint32_t max_attempts_{1};
    std::optional<std::string> error_details_{std::nullopt};
};

} // namespace taskmesh
