#pragma once

#include "taskmesh/job.hpp"
#include "taskmesh/job_status.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct sqlite3;

namespace taskmesh {

/**
 * @brief Abstract interface for persistent job storage.
 */
class IJobStore {
public:
    virtual ~IJobStore() = default;

    /**
     * @brief Inserts or updates a job in persistent storage.
     */
    virtual void save_job(const Job& job) = 0;

    /**
     * @brief Retrieves a job by ID.
     */
    [[nodiscard]] virtual std::optional<Job> get_job(const JobId& id) = 0;

    /**
     * @brief Updates status, error details, and attempt count for a job.
     */
    virtual void update_status(const JobId& id,
                               JobStatus status,
                               std::optional<std::string> error = std::nullopt,
                               std::optional<uint32_t> attempt_count = std::nullopt) = 0;

    /**
     * @brief Lists jobs with optional status filter, limit, and offset.
     */
    [[nodiscard]] virtual std::vector<Job> list_jobs(std::optional<JobStatus> filter_status = std::nullopt,
                                                    size_t limit = 100,
                                                    size_t offset = 0) = 0;

    /**
     * @brief Recovers uncompleted jobs after a process crash or restart.
     *
     * Transitions orphaned 'Running' jobs into 'Failed' (or re-queues them if can_retry is true).
     * Returns all jobs that should be scheduled into the execution engine.
     */
    [[nodiscard]] virtual std::vector<Job> recover_uncompleted_jobs() = 0;

    /**
     * @brief Deletes a job by ID.
     * @param id The job ID to delete.
     * @param only_terminal If true, only deletes the record if the job is in a terminal status (Completed, Failed, Cancelled).
     */
    virtual bool delete_job(const JobId& id, bool only_terminal = false) = 0;

    /**
     * @brief Atomically transitions a Failed or Cancelled job back to Queued with 0 attempts.
     * @return true if status was updated, false if job was not found or was in an active/completed state.
     */
    virtual bool reset_job_for_retry(const JobId& id) = 0;

    /**
     * @brief Returns count of jobs grouped by status.
     */
    [[nodiscard]] virtual std::unordered_map<JobStatus, size_t> count_by_status() = 0;
};

/**
 * @brief Production SQLite3 implementation of IJobStore with WAL mode and crash recovery.
 */
class SqliteJobStore : public IJobStore {
public:
    /**
     * @brief Opens or creates an SQLite job database at the given path.
     * @param db_path Filepath to the SQLite database (or ":memory:" for in-memory testing).
     */
    explicit SqliteJobStore(std::string db_path = "taskmesh.db");
    ~SqliteJobStore() override;

    // Non-copyable, non-movable resource owner
    SqliteJobStore(const SqliteJobStore&) = delete;
    SqliteJobStore& operator=(const SqliteJobStore&) = delete;
    SqliteJobStore(SqliteJobStore&&) = delete;
    SqliteJobStore& operator=(SqliteJobStore&&) = delete;

    void save_job(const Job& job) override;
    [[nodiscard]] std::optional<Job> get_job(const JobId& id) override;
    void update_status(const JobId& id,
                       JobStatus status,
                       std::optional<std::string> error = std::nullopt,
                       std::optional<uint32_t> attempt_count = std::nullopt) override;
    [[nodiscard]] std::vector<Job> list_jobs(std::optional<JobStatus> filter_status = std::nullopt,
                                            size_t limit = 100,
                                            size_t offset = 0) override;
    [[nodiscard]] std::vector<Job> recover_uncompleted_jobs() override;
    bool delete_job(const JobId& id, bool only_terminal = false) override;
    bool reset_job_for_retry(const JobId& id) override;
    [[nodiscard]] std::unordered_map<JobStatus, size_t> count_by_status() override;

private:
    void init_schema();

    std::string db_path_;
    ::sqlite3* db_{nullptr};
    std::mutex db_mutex_;
};

} // namespace taskmesh
