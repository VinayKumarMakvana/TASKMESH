#include "taskmesh/job_store.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

using namespace taskmesh;

class SqliteJobStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path_ = "test_taskmesh_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".db";
        store_ = std::make_unique<SqliteJobStore>(test_db_path_);
    }

    void TearDown() override {
        store_.reset();
        std::error_code ec;
        std::filesystem::remove(test_db_path_, ec);
        std::filesystem::remove(test_db_path_ + "-wal", ec);
        std::filesystem::remove(test_db_path_ + "-shm", ec);
    }

    std::string test_db_path_;
    std::unique_ptr<SqliteJobStore> store_;
};

TEST_F(SqliteJobStoreTest, SaveAndRetrieveJobRoundtrip) {
    const auto now = Job::Clock::now();
    Job job = Job::Builder()
        .with_id("job-persist-01")
        .with_type("database_backup")
        .with_payload("{\"db\":\"users_prod\"}")
        .with_priority(JobPriority::High)
        .with_creation_time(now)
        .with_status(JobStatus::Pending)
        .with_attempt_count(0)
        .with_max_attempts(3)
        .build();

    store_->save_job(job);

    auto retrieved = store_->get_job(JobId("job-persist-01"));
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->id().value(), "job-persist-01");
    EXPECT_EQ(retrieved->type(), "database_backup");
    EXPECT_EQ(retrieved->payload(), "{\"db\":\"users_prod\"}");
    EXPECT_EQ(retrieved->priority().value(), JobPriority::High.value());
    EXPECT_EQ(retrieved->status(), JobStatus::Pending);
    EXPECT_EQ(retrieved->max_attempts(), 3u);
}

TEST_F(SqliteJobStoreTest, UpdateStatusAndErrorDetails) {
    Job job = Job::Builder()
        .with_id("job-update-01")
        .with_type("send_sms")
        .with_payload("{\"to\":\"+123456789\"}")
        .build();

    store_->save_job(job);

    store_->update_status(JobId("job-update-01"), JobStatus::Running, std::nullopt, 1);
    auto running_job = store_->get_job(JobId("job-update-01"));
    ASSERT_TRUE(running_job.has_value());
    EXPECT_EQ(running_job->status(), JobStatus::Running);
    EXPECT_EQ(running_job->attempt_count(), 1u);

    store_->update_status(JobId("job-update-01"), JobStatus::Failed, "Gateway timeout 504", 1);
    auto failed_job = store_->get_job(JobId("job-update-01"));
    ASSERT_TRUE(failed_job.has_value());
    EXPECT_EQ(failed_job->status(), JobStatus::Failed);
    ASSERT_TRUE(failed_job->error_details().has_value());
    EXPECT_EQ(*failed_job->error_details(), "Gateway timeout 504");
}

TEST_F(SqliteJobStoreTest, ListJobsWithFilteringAndPagination) {
    for (int i = 0; i < 10; ++i) {
        auto status = (i % 2 == 0) ? JobStatus::Queued : JobStatus::Completed;
        Job job = Job::Builder()
            .with_id("list-job-" + std::to_string(i))
            .with_type("item")
            .with_status(status)
            .build();
        store_->save_job(job);
    }

    auto queued_jobs = store_->list_jobs(JobStatus::Queued, 10, 0);
    EXPECT_EQ(queued_jobs.size(), 5);

    auto all_jobs = store_->list_jobs(std::nullopt, 5, 0);
    EXPECT_EQ(all_jobs.size(), 5);

    auto counts = store_->count_by_status();
    EXPECT_EQ(counts[JobStatus::Queued], 5);
    EXPECT_EQ(counts[JobStatus::Completed], 5);
}

TEST_F(SqliteJobStoreTest, CrashRecoveryRequeuesInterruptedRetryableRunningJob) {
    // 1. Simulate a job running when the process crashed
    Job crashed_job = Job::Builder()
        .with_id("crashed-job-01")
        .with_type("long_render")
        .with_status(JobStatus::Running)
        .with_attempt_count(1)
        .with_max_attempts(3)
        .build();
    store_->save_job(crashed_job);

    // Also a normal queued job
    Job queued_job = Job::Builder()
        .with_id("queued-job-01")
        .with_type("normal_task")
        .with_status(JobStatus::Queued)
        .build();
    store_->save_job(queued_job);

    // 2. Simulate process restart: close store and reopen from disk
    store_.reset();
    store_ = std::make_unique<SqliteJobStore>(test_db_path_);

    // 3. Execute recovery
    auto runnable_jobs = store_->recover_uncompleted_jobs();
    EXPECT_EQ(runnable_jobs.size(), 2);

    auto recovered_crashed = store_->get_job(JobId("crashed-job-01"));
    ASSERT_TRUE(recovered_crashed.has_value());
    EXPECT_EQ(recovered_crashed->status(), JobStatus::Queued);
    ASSERT_TRUE(recovered_crashed->error_details().has_value());
    EXPECT_NE(recovered_crashed->error_details()->find("Recovered after process restart"), std::string::npos);
}

TEST_F(SqliteJobStoreTest, CrashRecoveryFailsExhaustedRunningJob) {
    // A running job that has already consumed all its attempts
    Job exhausted_job = Job::Builder()
        .with_id("exhausted-job-01")
        .with_type("expensive_compute")
        .with_status(JobStatus::Running)
        .with_attempt_count(3)
        .with_max_attempts(3)
        .build();
    store_->save_job(exhausted_job);

    // Restart process
    store_.reset();
    store_ = std::make_unique<SqliteJobStore>(test_db_path_);

    auto runnable_jobs = store_->recover_uncompleted_jobs();
    EXPECT_EQ(runnable_jobs.size(), 0);

    auto post_recovery = store_->get_job(JobId("exhausted-job-01"));
    ASSERT_TRUE(post_recovery.has_value());
    EXPECT_EQ(post_recovery->status(), JobStatus::Failed);
}

TEST_F(SqliteJobStoreTest, DeleteJobRemovesRecord) {
    Job job = Job::Builder().with_id("to-delete-01").with_type("temp").build();
    store_->save_job(job);
    EXPECT_TRUE(store_->get_job(JobId("to-delete-01")).has_value());

    EXPECT_TRUE(store_->delete_job(JobId("to-delete-01")));
    EXPECT_FALSE(store_->get_job(JobId("to-delete-01")).has_value());
    EXPECT_FALSE(store_->delete_job(JobId("to-delete-01"))); // Already deleted
}
