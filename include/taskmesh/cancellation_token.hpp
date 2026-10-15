#pragma once

#include <memory>
#include <stop_token>

namespace taskmesh {

/**
 * @brief Lightweight, non-owning handle for querying cooperative cancellation.
 */
class CancellationToken {
public:
    CancellationToken() = default;
    explicit CancellationToken(std::stop_token token) : token_(token) {}

    [[nodiscard]] bool is_cancellation_requested() const noexcept {
        return token_.stop_requested();
    }

    [[nodiscard]] std::stop_token native_token() const noexcept {
        return token_;
    }

private:
    std::stop_token token_;
};

/**
 * @brief Origin of a cooperative cancellation signal.
 */
class CancellationSource {
public:
    CancellationSource() = default;

    [[nodiscard]] CancellationToken get_token() const noexcept {
        return CancellationToken(stop_source_.get_token());
    }

    bool request_cancellation() noexcept {
        return stop_source_.request_stop();
    }

    [[nodiscard]] bool is_cancellation_requested() const noexcept {
        return stop_source_.stop_requested();
    }

private:
    std::stop_source stop_source_;
};

/**
 * @brief Thread-local execution context passed into job executors.
 */
class JobExecutionContext {
public:
    JobExecutionContext() = default;
    explicit JobExecutionContext(CancellationToken token, size_t worker_id = 0)
        : cancellation_token_(std::move(token)), worker_id_(worker_id) {}

    [[nodiscard]] const CancellationToken& cancellation_token() const noexcept {
        return cancellation_token_;
    }

    [[nodiscard]] bool is_cancellation_requested() const noexcept {
        return cancellation_token_.is_cancellation_requested();
    }

    [[nodiscard]] size_t worker_id() const noexcept { return worker_id_; }

    static const JobExecutionContext& empty() noexcept {
        static const JobExecutionContext kEmpty;
        return kEmpty;
    }

private:
    CancellationToken cancellation_token_;
    size_t worker_id_{0};
};

} // namespace taskmesh
