#include <gtest/gtest.h>

#include "taskmesh/job.hpp"
#include "taskmesh/job_executor.hpp"

#include <stdexcept>

using namespace taskmesh;

TEST(JobExecutorTest, SuccessfulExecutionUpdatesLifecycleAndAttempts) {
    InMemoryJobExecutor executor;

    Job job = Job::Builder()
        .with_id("exec-job-1")
        .with_type("image_resize")
        .with_payload("{\"scale\": 0.5}")
        .build();

    job.mark_queued();
    EXPECT_EQ(job.attempt_count(), 0u);
    EXPECT_EQ(job.status(), JobStatus::Queued);

    ExecutionResult result = executor.execute(job);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(job.status(), JobStatus::Completed);
    EXPECT_EQ(job.attempt_count(), 1u);
    EXPECT_FALSE(job.error_details().has_value());
    EXPECT_EQ(executor.executed_count(), 1u);
    EXPECT_EQ(executor.successful_count(), 1u);
    EXPECT_EQ(executor.failed_count(), 0u);
}

TEST(JobExecutorTest, CustomHandlerDispatchByType) {
    InMemoryJobExecutor executor;

    executor.register_handler("billing_invoice", [](const Job& job) {
        if (job.payload().empty()) {
            return ExecutionResult::error("Payload missing");
        }
        return ExecutionResult::ok("Invoice generated: " + job.payload());
    });

    Job job = Job::Builder()
        .with_id("invoice-42")
        .with_type("billing_invoice")
        .with_payload("INV-2026-001")
        .build();

    job.mark_queued();
    ExecutionResult result = executor.execute(job);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.message, "Invoice generated: INV-2026-001");
    EXPECT_EQ(job.status(), JobStatus::Completed);
}

TEST(JobExecutorTest, FailedExecutionUpdatesJobToFailed) {
    InMemoryJobExecutor executor;

    executor.register_handler("flake_task", [](const Job&) {
        return ExecutionResult::error("Downstream endpoint connection timeout (504)");
    });

    Job job = Job::Builder()
        .with_id("flake-1")
        .with_type("flake_task")
        .with_max_attempts(3)
        .build();

    job.mark_queued();
    ExecutionResult result = executor.execute(job);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(job.status(), JobStatus::Failed);
    ASSERT_TRUE(job.error_details().has_value());
    EXPECT_EQ(*job.error_details(), "Downstream endpoint connection timeout (504)");
    EXPECT_EQ(job.attempt_count(), 1u);
    EXPECT_TRUE(job.can_retry()); // 1 < 3
    EXPECT_EQ(executor.failed_count(), 1u);
}

TEST(JobExecutorTest, ExceptionInHandlerCaughtGracefully) {
    InMemoryJobExecutor executor;

    executor.register_handler("crashing_task", [](const Job&) -> ExecutionResult {
        throw std::runtime_error("Simulated catastrophic crash inside handler");
    });

    Job job = Job::Builder()
        .with_id("crash-1")
        .with_type("crashing_task")
        .build();

    job.mark_queued();
    ExecutionResult result = executor.execute(job);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(job.status(), JobStatus::Failed);
    ASSERT_TRUE(job.error_details().has_value());
    EXPECT_NE(job.error_details()->find("Simulated catastrophic crash"), std::string::npos);
}

// -----------------------------------------------------------------------------
// Regression Tests for Review Findings H-01 & H-02
// -----------------------------------------------------------------------------

TEST(JobExecutorTest, ExecutionOfCompletedJobReturnsErrorAndPreservesState) {
    InMemoryJobExecutor executor;

    Job completed_job = Job::Builder()
        .with_id("completed-test-1")
        .with_type("cleanup_task")
        .with_status(JobStatus::Completed)
        .with_attempt_count(1)
        .with_max_attempts(1)
        .build();

    ExecutionResult result = executor.execute(completed_job);

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.message.find("status must be Pending or Queued"), std::string::npos);
    EXPECT_EQ(completed_job.status(), JobStatus::Completed);
    EXPECT_EQ(completed_job.attempt_count(), 1u);
    EXPECT_FALSE(completed_job.error_details().has_value());

    // Invariant: Metrics remain consistent
    EXPECT_EQ(executor.executed_count(), 1u);
    EXPECT_EQ(executor.successful_count(), 0u);
    EXPECT_EQ(executor.failed_count(), 1u);
    EXPECT_EQ(executor.executed_count(), executor.successful_count() + executor.failed_count());
}

TEST(JobExecutorTest, ExecutionOfCancelledJobReturnsErrorAndPreservesState) {
    InMemoryJobExecutor executor;

    Job cancelled_job = Job::Builder()
        .with_id("cancelled-test-1")
        .with_type("cleanup_task")
        .with_status(JobStatus::Cancelled)
        .with_error_details("cancelled_by_operator")
        .with_attempt_count(0)
        .with_max_attempts(3)
        .build();

    ExecutionResult result = executor.execute(cancelled_job);

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.message.find("status must be Pending or Queued"), std::string::npos);
    EXPECT_EQ(cancelled_job.status(), JobStatus::Cancelled);
    EXPECT_EQ(cancelled_job.attempt_count(), 0u);
    ASSERT_TRUE(cancelled_job.error_details().has_value());
    EXPECT_EQ(*cancelled_job.error_details(), "cancelled_by_operator");

    EXPECT_EQ(executor.executed_count(), 1u);
    EXPECT_EQ(executor.failed_count(), 1u);
    EXPECT_EQ(executor.successful_count(), 0u);
}

TEST(JobExecutorTest, ExecutionOfAttemptExhaustedQueuedJobReturnsErrorAndPreservesState) {
    InMemoryJobExecutor executor;

    Job exhausted_job = Job::Builder()
        .with_id("exhausted-queued-1")
        .with_type("transcode_video")
        .with_status(JobStatus::Queued)
        .with_attempt_count(3)
        .with_max_attempts(3)
        .build();

    ExecutionResult result = executor.execute(exhausted_job);

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.message.find("attempt limit reached"), std::string::npos);
    EXPECT_EQ(exhausted_job.status(), JobStatus::Queued);
    EXPECT_EQ(exhausted_job.attempt_count(), 3u); // Must not increment past max_attempts

    EXPECT_EQ(executor.executed_count(), 1u);
    EXPECT_EQ(executor.failed_count(), 1u);
    EXPECT_EQ(executor.successful_count(), 0u);
}

TEST(JobExecutorTest, ExecutionOfAttemptExhaustedPendingJobReturnsErrorAndPreservesState) {
    InMemoryJobExecutor executor;

    Job exhausted_pending = Job::Builder()
        .with_id("exhausted-pending-1")
        .with_type("transcode_video")
        .with_status(JobStatus::Pending)
        .with_attempt_count(1)
        .with_max_attempts(1)
        .build();

    ExecutionResult result = executor.execute(exhausted_pending);

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.message.find("attempt limit reached"), std::string::npos);
    EXPECT_EQ(exhausted_pending.status(), JobStatus::Pending);
    EXPECT_EQ(exhausted_pending.attempt_count(), 1u);

    EXPECT_EQ(executor.executed_count(), 1u);
    EXPECT_EQ(executor.failed_count(), 1u);
    EXPECT_EQ(executor.successful_count(), 0u);
}

TEST(JobExecutorTest, ExecutorMetricsConsistencyInvariantAcrossMultipleOperations) {
    InMemoryJobExecutor executor;

    executor.register_handler("failing_type", [](const Job&) {
        return ExecutionResult::error("Handler error");
    });

    // 1. Successful job
    Job j1 = Job::Builder().with_id("m-ok-1").with_type("default_type").build();
    j1.mark_queued();
    EXPECT_TRUE(executor.execute(j1).success);

    // 2. Successful job
    Job j2 = Job::Builder().with_id("m-ok-2").with_type("default_type").build();
    j2.mark_queued();
    EXPECT_TRUE(executor.execute(j2).success);

    // 3. Handler failure
    Job j3 = Job::Builder().with_id("m-fail-1").with_type("failing_type").build();
    j3.mark_queued();
    EXPECT_FALSE(executor.execute(j3).success);

    // 4. Completed job rejection (H-01)
    Job j4 = Job::Builder().with_id("m-term-1").with_type("default_type").with_status(JobStatus::Completed).build();
    EXPECT_FALSE(executor.execute(j4).success);

    // 5. Cancelled job rejection (H-01)
    Job j5 = Job::Builder().with_id("m-term-2").with_type("default_type").with_status(JobStatus::Cancelled).build();
    EXPECT_FALSE(executor.execute(j5).success);

    // 6. Attempt exhausted rejection (H-02)
    Job j6 = Job::Builder().with_id("m-exhaust-1").with_type("default_type").with_status(JobStatus::Queued).with_attempt_count(2).with_max_attempts(2).build();
    EXPECT_FALSE(executor.execute(j6).success);

    // Strict invariant: total executed equals successful + failed
    EXPECT_EQ(executor.executed_count(), 6u);
    EXPECT_EQ(executor.successful_count(), 2u);
    EXPECT_EQ(executor.failed_count(), 4u);
    EXPECT_EQ(executor.executed_count(), executor.successful_count() + executor.failed_count());
}
