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
