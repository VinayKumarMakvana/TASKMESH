#include "taskmesh/job.hpp"

#include <cctype>
#include <iomanip>
#include <ostream>
#include <sstream>

namespace taskmesh {

// -----------------------------------------------------------------------------
// JobStatus Implementation
// -----------------------------------------------------------------------------

std::optional<JobStatus> job_status_from_string(std::string_view str) noexcept {
    if (str == "Pending" || str == "pending") return JobStatus::Pending;
    if (str == "Queued" || str == "queued") return JobStatus::Queued;
    if (str == "Running" || str == "running") return JobStatus::Running;
    if (str == "Completed" || str == "completed") return JobStatus::Completed;
    if (str == "Failed" || str == "failed") return JobStatus::Failed;
    if (str == "Cancelled" || str == "cancelled") return JobStatus::Cancelled;
    return std::nullopt;
}

bool is_valid_status_transition(JobStatus current, JobStatus target) noexcept {
    if (current == target) {
        return true;
    }

    switch (current) {
        case JobStatus::Pending:
            return target == JobStatus::Queued || target == JobStatus::Cancelled;

        case JobStatus::Queued:
            return target == JobStatus::Running || target == JobStatus::Cancelled;

        case JobStatus::Running:
            return target == JobStatus::Completed || 
                   target == JobStatus::Failed || 
                   target == JobStatus::Cancelled;

        case JobStatus::Failed:
            // Failed jobs can be re-queued if retry attempts remain, or marked cancelled
            return target == JobStatus::Queued || target == JobStatus::Cancelled;

        case JobStatus::Completed:
        case JobStatus::Cancelled:
            // Terminal states cannot transition to any other state
            return false;
    }

    return false;
}

std::ostream& operator<<(std::ostream& os, JobStatus status) {
    os << to_string(status);
    return os;
}

// -----------------------------------------------------------------------------
// JobId Implementation
// -----------------------------------------------------------------------------

void JobId::validate(std::string_view id) {
    if (id.empty()) {
        throw JobValidationError("Job identifier cannot be empty.");
    }
    if (id.size() > 128) {
        throw JobValidationError("Job identifier exceeds maximum allowed length of 128 characters.");
    }
    for (char ch : id) {
        const unsigned char uc = static_cast<unsigned char>(ch);
        const bool is_alphanumeric = (std::isalnum(uc) != 0);
        const bool is_safe_symbol = (ch == '-' || ch == '_' || ch == '.' || ch == ':');
        if (!is_alphanumeric && !is_safe_symbol) {
            std::ostringstream ss;
            ss << "Job identifier contains illegal character: '" << ch << "'. Only [a-zA-Z0-9_.-:] are permitted.";
            throw JobValidationError(ss.str());
        }
    }
}

JobId::JobId(std::string id) : id_(std::move(id)) {
    validate(id_);
}

JobId::JobId(std::string_view id) : id_(id) {
    validate(id_);
}

JobId::JobId(const char* id) : id_(id != nullptr ? id : "") {
    validate(id_);
}

std::ostream& operator<<(std::ostream& os, const JobId& id) {
    os << id.value();
    return os;
}

// -----------------------------------------------------------------------------
// JobPriority Implementation
// -----------------------------------------------------------------------------

const JobPriority JobPriority::Low{PriorityLevel::Low};
const JobPriority JobPriority::Normal{PriorityLevel::Normal};
const JobPriority JobPriority::High{PriorityLevel::High};
const JobPriority JobPriority::Critical{PriorityLevel::Critical};

std::string JobPriority::to_string() const {
    switch (value_) {
        case static_cast<int32_t>(PriorityLevel::Low):
            return "Low(10)";
        case static_cast<int32_t>(PriorityLevel::Normal):
            return "Normal(50)";
        case static_cast<int32_t>(PriorityLevel::High):
            return "High(100)";
        case static_cast<int32_t>(PriorityLevel::Critical):
            return "Critical(200)";
        default:
            return "Custom(" + std::to_string(value_) + ")";
    }
}

std::ostream& operator<<(std::ostream& os, const JobPriority& priority) {
    os << priority.to_string();
    return os;
}

// -----------------------------------------------------------------------------
// Job Implementation
// -----------------------------------------------------------------------------

Job::Job(JobId id,
         std::string type,
         std::string payload,
         JobPriority priority,
         TimePoint creation_time,
         std::optional<TimePoint> scheduled_time,
         JobStatus status,
         uint32_t attempt_count,
         uint32_t max_attempts,
         std::optional<std::string> error_details)
    : id_(std::move(id)),
      type_(std::move(type)),
      payload_(std::move(payload)),
      priority_(priority),
      creation_time_(creation_time),
      scheduled_time_(scheduled_time),
      status_(status),
      attempt_count_(attempt_count),
      max_attempts_(max_attempts),
      error_details_(std::move(error_details)) {
    validate_invariants();
}

void Job::validate_invariants() const {
    if (id_.empty()) {
        throw JobValidationError("Job must have a valid non-empty identifier.");
    }
    if (type_.empty()) {
        throw JobValidationError("Job type cannot be empty.");
    }
    if (max_attempts_ == 0) {
        throw JobValidationError("Job max_attempts must be at least 1.");
    }
    if (attempt_count_ > max_attempts_) {
        std::ostringstream ss;
        ss << "Job attempt_count (" << attempt_count_ 
           << ") cannot exceed max_attempts (" << max_attempts_ << ").";
        throw JobValidationError(ss.str());
    }
}

void Job::set_status(JobStatus new_status) {
    if (!is_valid_status_transition(status_, new_status)) {
        std::ostringstream ss;
        ss << "Illegal lifecycle transition for job '" << id_.value() << "': "
           << to_string(status_) << " -> " << to_string(new_status);
        throw JobValidationError(ss.str());
    }
    status_ = new_status;
}

void Job::mark_queued() {
    set_status(JobStatus::Queued);
}

void Job::mark_running() {
    set_status(JobStatus::Running);
}

void Job::mark_completed() {
    set_status(JobStatus::Completed);
    error_details_.reset();
}

void Job::mark_failed(std::string error_message) {
    set_status(JobStatus::Failed);
    error_details_ = std::move(error_message);
}

void Job::mark_cancelled(std::string reason) {
    set_status(JobStatus::Cancelled);
    if (!reason.empty()) {
        error_details_ = std::move(reason);
    }
}

void Job::increment_attempt_count() {
    if (attempt_count_ >= max_attempts_) {
        std::ostringstream ss;
        ss << "Cannot increment attempt_count for job '" << id_.value() 
           << "': already reached max_attempts (" << max_attempts_ << ").";
        throw JobValidationError(ss.str());
    }
    ++attempt_count_;
}

bool Job::operator==(const Job& other) const noexcept {
    return id_ == other.id_ &&
           type_ == other.type_ &&
           payload_ == other.payload_ &&
           priority_ == other.priority_ &&
           creation_time_ == other.creation_time_ &&
           scheduled_time_ == other.scheduled_time_ &&
           status_ == other.status_ &&
           attempt_count_ == other.attempt_count_ &&
           max_attempts_ == other.max_attempts_ &&
           error_details_ == other.error_details_;
}

std::ostream& operator<<(std::ostream& os, const Job& job) {
    os << "Job[id=" << job.id().value()
       << ", type=" << job.type()
       << ", priority=" << job.priority()
       << ", status=" << to_string(job.status())
       << ", attempts=" << job.attempt_count() << "/" << job.max_attempts();
    if (job.error_details().has_value()) {
        os << ", error=\"" << *job.error_details() << "\"";
    }
    os << "]";
    return os;
}

// -----------------------------------------------------------------------------
// Job::Builder Implementation
// -----------------------------------------------------------------------------

Job::Builder& Job::Builder::with_id(JobId id) {
    id_ = std::move(id);
    return *this;
}

Job::Builder& Job::Builder::with_id(std::string id) {
    id_ = JobId(std::move(id));
    return *this;
}

Job::Builder& Job::Builder::with_type(std::string type) {
    type_ = std::move(type);
    return *this;
}

Job::Builder& Job::Builder::with_payload(std::string payload) {
    payload_ = std::move(payload);
    return *this;
}

Job::Builder& Job::Builder::with_priority(JobPriority priority) {
    priority_ = priority;
    return *this;
}

Job::Builder& Job::Builder::with_creation_time(TimePoint creation_time) {
    creation_time_ = creation_time;
    return *this;
}

Job::Builder& Job::Builder::with_scheduled_time(TimePoint scheduled_time) {
    scheduled_time_ = scheduled_time;
    return *this;
}

Job::Builder& Job::Builder::with_status(JobStatus status) {
    status_ = status;
    return *this;
}

Job::Builder& Job::Builder::with_max_attempts(uint32_t max_attempts) {
    max_attempts_ = max_attempts;
    return *this;
}

Job::Builder& Job::Builder::with_attempt_count(uint32_t attempt_count) {
    attempt_count_ = attempt_count;
    return *this;
}

Job::Builder& Job::Builder::with_error_details(std::string error_details) {
    error_details_ = std::move(error_details);
    return *this;
}

Job Job::Builder::build() const {
    return Job(id_,
               type_,
               payload_,
               priority_,
               creation_time_,
               scheduled_time_,
               status_,
               attempt_count_,
               max_attempts_,
               error_details_);
}

} // namespace taskmesh
