#include "taskmesh/concurrent_priority_queue.hpp"

#include <stdexcept>

namespace taskmesh {

void ConcurrentPriorityQueue::push(Job job) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_) {
            throw std::runtime_error("Cannot push to closed ConcurrentPriorityQueue.");
        }
        if (draining_) {
            throw std::runtime_error("Cannot push to draining ConcurrentPriorityQueue.");
        }
        scheduler_.enqueue(std::move(job));
    }
    cv_.notify_one();
}

std::optional<Job> ConcurrentPriorityQueue::try_pop() {
    std::lock_guard<std::mutex> lock(mutex_);
    return scheduler_.dequeue();
}

std::optional<Job> ConcurrentPriorityQueue::pop_wait(std::optional<std::chrono::milliseconds> timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto predicate = [this]() {
        return !scheduler_.empty() || closed_ || (draining_ && scheduler_.empty());
    };

    if (timeout.has_value()) {
        if (!cv_.wait_for(lock, *timeout, predicate)) {
            return std::nullopt; // Timed out waiting
        }
    } else {
        cv_.wait(lock, predicate);
    }

    if (!scheduler_.empty()) {
        return scheduler_.dequeue();
    }

    return std::nullopt;
}

void ConcurrentPriorityQueue::close(bool drain) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (drain) {
            draining_ = true;
        } else {
            closed_ = true;
            draining_ = false;
        }
    }
    cv_.notify_all();
}

std::vector<Job> ConcurrentPriorityQueue::drain_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Job> drained;
    drained.reserve(scheduler_.size());
    while (!scheduler_.empty()) {
        auto job_opt = scheduler_.dequeue();
        if (job_opt.has_value()) {
            drained.push_back(std::move(*job_opt));
        }
    }
    return drained;
}

bool ConcurrentPriorityQueue::is_closed() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return closed_;
}

bool ConcurrentPriorityQueue::is_draining() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return draining_;
}

size_t ConcurrentPriorityQueue::size() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return scheduler_.size();
}

bool ConcurrentPriorityQueue::empty() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return scheduler_.empty();
}

void ConcurrentPriorityQueue::clear() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    scheduler_.clear();
}

} // namespace taskmesh
