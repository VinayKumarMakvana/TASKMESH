#include "taskmesh/job_store.hpp"
#include "sqlite3.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace taskmesh {

namespace {

inline int64_t to_epoch_ms(Job::TimePoint tp) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

inline Job::TimePoint from_epoch_ms(int64_t ms) {
    return Job::TimePoint(std::chrono::milliseconds(ms));
}

inline int64_t now_epoch_ms() {
    return to_epoch_ms(Job::Clock::now());
}

} // namespace

SqliteJobStore::SqliteJobStore(std::string db_path)
    : db_path_(std::move(db_path)) {
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Failed to open SQLite database '" + db_path_ + "': " + err);
    }
    init_schema();
}

SqliteJobStore::~SqliteJobStore() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void SqliteJobStore::init_schema() {
    std::lock_guard<std::mutex> lock(db_mutex_);

    char* err_msg = nullptr;
    // Enable WAL mode for high concurrency
    sqlite3_exec(db_, "PRAGMA journal_mode = WAL;", nullptr, nullptr, &err_msg);
    if (err_msg) {
        sqlite3_free(err_msg);
        err_msg = nullptr;
    }

    sqlite3_exec(db_, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, &err_msg);
    if (err_msg) {
        sqlite3_free(err_msg);
        err_msg = nullptr;
    }

    sqlite3_exec(db_, "PRAGMA busy_timeout = 5000;", nullptr, nullptr, &err_msg);
    if (err_msg) {
        sqlite3_free(err_msg);
        err_msg = nullptr;
    }

    const char* schema_sql =
        "CREATE TABLE IF NOT EXISTS jobs ("
        "  id TEXT PRIMARY KEY,"
        "  type TEXT NOT NULL,"
        "  payload TEXT NOT NULL,"
        "  priority INTEGER NOT NULL,"
        "  creation_time INTEGER NOT NULL,"
        "  scheduled_time INTEGER,"
        "  status INTEGER NOT NULL,"
        "  attempt_count INTEGER NOT NULL,"
        "  max_attempts INTEGER NOT NULL,"
        "  error_details TEXT,"
        "  updated_at INTEGER NOT NULL"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_jobs_status_priority ON jobs(status, priority DESC, creation_time ASC);";

    int rc = sqlite3_exec(db_, schema_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string err = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("Failed to initialize SQLite schema: " + err);
    }
}

void SqliteJobStore::save_job(const Job& job) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql =
        "INSERT OR REPLACE INTO jobs ("
        "  id, type, payload, priority, creation_time, scheduled_time, "
        "  status, attempt_count, max_attempts, error_details, updated_at"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(std::string("Prepare save_job failed: ") + sqlite3_errmsg(db_));
    }

    sqlite3_bind_text(stmt, 1, job.id().value().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, job.type().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, job.payload().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, job.priority().value());
    sqlite3_bind_int64(stmt, 5, to_epoch_ms(job.creation_time()));

    if (job.scheduled_time().has_value()) {
        sqlite3_bind_int64(stmt, 6, to_epoch_ms(*job.scheduled_time()));
    } else {
        sqlite3_bind_null(stmt, 6);
    }

    sqlite3_bind_int(stmt, 7, static_cast<int>(job.status()));
    sqlite3_bind_int(stmt, 8, static_cast<int>(job.attempt_count()));
    sqlite3_bind_int(stmt, 9, static_cast<int>(job.max_attempts()));

    if (job.error_details().has_value()) {
        sqlite3_bind_text(stmt, 10, job.error_details()->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 10);
    }

    sqlite3_bind_int64(stmt, 11, now_epoch_ms());

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        throw std::runtime_error(std::string("Failed to save job: ") + sqlite3_errmsg(db_));
    }
}

std::optional<Job> SqliteJobStore::get_job(const JobId& id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql =
        "SELECT id, type, payload, priority, creation_time, scheduled_time, "
        "       status, attempt_count, max_attempts, error_details "
        "FROM jobs WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(std::string("Prepare get_job failed: ") + sqlite3_errmsg(db_));
    }

    sqlite3_bind_text(stmt, 1, id.value().c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char* j_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* j_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* j_payload = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        int j_priority = sqlite3_column_int(stmt, 3);
        int64_t j_creation = sqlite3_column_int64(stmt, 4);
        bool has_scheduled = sqlite3_column_type(stmt, 5) != SQLITE_NULL;
        int64_t j_scheduled = has_scheduled ? sqlite3_column_int64(stmt, 5) : 0;
        int j_status = sqlite3_column_int(stmt, 6);
        int j_attempts = sqlite3_column_int(stmt, 7);
        int j_max_attempts = sqlite3_column_int(stmt, 8);
        const char* j_err = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));

        Job::Builder builder;
        builder.with_id(j_id ? j_id : "")
               .with_type(j_type ? j_type : "")
               .with_payload(j_payload ? j_payload : "")
               .with_priority(JobPriority(j_priority))
               .with_creation_time(from_epoch_ms(j_creation))
               .with_status(static_cast<JobStatus>(j_status))
               .with_attempt_count(static_cast<uint32_t>(j_attempts))
               .with_max_attempts(static_cast<uint32_t>(j_max_attempts));

        if (has_scheduled) {
            builder.with_scheduled_time(from_epoch_ms(j_scheduled));
        }
        if (j_err) {
            builder.with_error_details(j_err);
        }

        sqlite3_finalize(stmt);
        return builder.build();
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

void SqliteJobStore::update_status(const JobId& id,
                                   JobStatus status,
                                   std::optional<std::string> error,
                                   std::optional<uint32_t> attempt_count) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    std::string sql;
    if (attempt_count.has_value()) {
        sql = "UPDATE jobs SET status = ?, error_details = ?, attempt_count = ?, updated_at = ? WHERE id = ?;";
    } else {
        sql = "UPDATE jobs SET status = ?, error_details = ?, updated_at = ? WHERE id = ?;";
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(std::string("Prepare update_status failed: ") + sqlite3_errmsg(db_));
    }

    sqlite3_bind_int(stmt, 1, static_cast<int>(status));
    if (error.has_value()) {
        sqlite3_bind_text(stmt, 2, error->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 2);
    }

    if (attempt_count.has_value()) {
        sqlite3_bind_int(stmt, 3, static_cast<int>(*attempt_count));
        sqlite3_bind_int64(stmt, 4, now_epoch_ms());
        sqlite3_bind_text(stmt, 5, id.value().c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_int64(stmt, 3, now_epoch_ms());
        sqlite3_bind_text(stmt, 4, id.value().c_str(), -1, SQLITE_TRANSIENT);
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        throw std::runtime_error(std::string("Failed to update job status: ") + sqlite3_errmsg(db_));
    }
}

std::vector<Job> SqliteJobStore::list_jobs(std::optional<JobStatus> filter_status, size_t limit, size_t offset) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    std::string sql =
        "SELECT id, type, payload, priority, creation_time, scheduled_time, "
        "       status, attempt_count, max_attempts, error_details "
        "FROM jobs ";

    if (filter_status.has_value()) {
        sql += "WHERE status = ? ";
    }
    sql += "ORDER BY priority DESC, creation_time ASC LIMIT ? OFFSET ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(std::string("Prepare list_jobs failed: ") + sqlite3_errmsg(db_));
    }

    int param_idx = 1;
    if (filter_status.has_value()) {
        sqlite3_bind_int(stmt, param_idx++, static_cast<int>(*filter_status));
    }
    sqlite3_bind_int64(stmt, param_idx++, static_cast<int64_t>(limit));
    sqlite3_bind_int64(stmt, param_idx++, static_cast<int64_t>(offset));

    std::vector<Job> jobs;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* j_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* j_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* j_payload = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        int j_priority = sqlite3_column_int(stmt, 3);
        int64_t j_creation = sqlite3_column_int64(stmt, 4);
        bool has_scheduled = sqlite3_column_type(stmt, 5) != SQLITE_NULL;
        int64_t j_scheduled = has_scheduled ? sqlite3_column_int64(stmt, 5) : 0;
        int j_status = sqlite3_column_int(stmt, 6);
        int j_attempts = sqlite3_column_int(stmt, 7);
        int j_max_attempts = sqlite3_column_int(stmt, 8);
        const char* j_err = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));

        Job::Builder builder;
        builder.with_id(j_id ? j_id : "")
               .with_type(j_type ? j_type : "")
               .with_payload(j_payload ? j_payload : "")
               .with_priority(JobPriority(j_priority))
               .with_creation_time(from_epoch_ms(j_creation))
               .with_status(static_cast<JobStatus>(j_status))
               .with_attempt_count(static_cast<uint32_t>(j_attempts))
               .with_max_attempts(static_cast<uint32_t>(j_max_attempts));

        if (has_scheduled) {
            builder.with_scheduled_time(from_epoch_ms(j_scheduled));
        }
        if (j_err) {
            builder.with_error_details(j_err);
        }

        jobs.push_back(builder.build());
    }

    sqlite3_finalize(stmt);
    return jobs;
}

std::vector<Job> SqliteJobStore::recover_uncompleted_jobs() {
    std::lock_guard<std::mutex> lock(db_mutex_);

    // 1. Transaction to fix orphaned Running jobs
    sqlite3_exec(db_, "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, nullptr);

    // Any job left in Running (status=2) was interrupted by a crash:
    // If attempts < max_attempts, re-queue it (status=1).
    // If attempts >= max_attempts, mark it Failed (status=4).
    const char* recover_running_sql =
        "UPDATE jobs "
        "SET status = CASE WHEN attempt_count < max_attempts THEN 1 ELSE 4 END, "
        "    error_details = 'Recovered after process restart', "
        "    updated_at = ? "
        "WHERE status = 2;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, recover_running_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, now_epoch_ms());
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);

    // 2. Select all jobs in Queued (status=1) to resume processing
    const char* select_queued_sql =
        "SELECT id, type, payload, priority, creation_time, scheduled_time, "
        "       status, attempt_count, max_attempts, error_details "
        "FROM jobs WHERE status = 1 "
        "ORDER BY priority DESC, creation_time ASC;";

    std::vector<Job> queued_jobs;
    if (sqlite3_prepare_v2(db_, select_queued_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* j_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* j_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* j_payload = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            int j_priority = sqlite3_column_int(stmt, 3);
            int64_t j_creation = sqlite3_column_int64(stmt, 4);
            bool has_scheduled = sqlite3_column_type(stmt, 5) != SQLITE_NULL;
            int64_t j_scheduled = has_scheduled ? sqlite3_column_int64(stmt, 5) : 0;
            int j_status = sqlite3_column_int(stmt, 6);
            int j_attempts = sqlite3_column_int(stmt, 7);
            int j_max_attempts = sqlite3_column_int(stmt, 8);
            const char* j_err = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));

            Job::Builder builder;
            builder.with_id(j_id ? j_id : "")
                   .with_type(j_type ? j_type : "")
                   .with_payload(j_payload ? j_payload : "")
                   .with_priority(JobPriority(j_priority))
                   .with_creation_time(from_epoch_ms(j_creation))
                   .with_status(static_cast<JobStatus>(j_status))
                   .with_attempt_count(static_cast<uint32_t>(j_attempts))
                   .with_max_attempts(static_cast<uint32_t>(j_max_attempts));

            if (has_scheduled) {
                builder.with_scheduled_time(from_epoch_ms(j_scheduled));
            }
            if (j_err) {
                builder.with_error_details(j_err);
            }

            queued_jobs.push_back(builder.build());
        }
        sqlite3_finalize(stmt);
    }

    return queued_jobs;
}

bool SqliteJobStore::delete_job(const JobId& id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM jobs WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, id.value().c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(db_);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) && (changes > 0);
}

std::unordered_map<JobStatus, size_t> SqliteJobStore::count_by_status() {
    std::lock_guard<std::mutex> lock(db_mutex_);

    std::unordered_map<JobStatus, size_t> counts;
    const char* sql = "SELECT status, COUNT(*) FROM jobs GROUP BY status;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int status_int = sqlite3_column_int(stmt, 0);
            int64_t count = sqlite3_column_int64(stmt, 1);
            counts[static_cast<JobStatus>(status_int)] = static_cast<size_t>(count);
        }
        sqlite3_finalize(stmt);
    }

    return counts;
}

} // namespace taskmesh
