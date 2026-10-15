#include <gtest/gtest.h>

#include "taskmesh/job.hpp"
#include "taskmesh/job_status.hpp"

#include <chrono>
#include <string>

using namespace taskmesh;

// -----------------------------------------------------------------------------
// Job Model Tests
// -----------------------------------------------------------------------------

TEST(JobTest, ValidJobCreationDirect) {
    const auto now = Job::Clock::now();
    const auto scheduled = now + std::chrono::hours(1);

    Job job(
        JobId("job-101"),
        "send_email",
        "{\"recipient\": \"alice@example.com\"}",
        JobPriority::High,
        now,
        scheduled,
        JobStatus::Pending,
        0,
        3,
        std::nullopt
    );

    EXPECT_EQ(job.id().value(), "job-101");
    EXPECT_EQ(job.type(), "send_email");
    EXPECT_EQ(job.payload(), "{\"recipient\": \"alice@example.com\"}");
    EXPECT_EQ(job.priority(), JobPriority::High);
    EXPECT_EQ(job.creation_time(), now);
    ASSERT_TRUE(job.scheduled_time().has_value());
    EXPECT_EQ(*job.scheduled_time(), scheduled);
    EXPECT_EQ(job.status(), JobStatus::Pending);
    EXPECT_EQ(job.attempt_count(), 0u);
    EXPECT_EQ(job.max_attempts(), 3u);
    EXPECT_FALSE(job.error_details().has_value());
}

TEST(JobTest, ValidJobCreationViaBuilder) {
    Job job = Job::Builder()
        .with_id("data-sync-01")
        .with_type("db_replication")
        .with_payload("{\"replica\":\"node-2\"}")
        .with_priority(JobPriority::Critical)
        .with_max_attempts(5)
        .build();

    EXPECT_EQ(job.id().value(), "data-sync-01");
    EXPECT_EQ(job.type(), "db_replication");
    EXPECT_EQ(job.priority(), JobPriority::Critical);
    EXPECT_EQ(job.status(), JobStatus::Pending);
    EXPECT_EQ(job.attempt_count(), 0u);
    EXPECT_EQ(job.max_attempts(), 5u);
}

TEST(JobTest, DefaultValuesAreSensible) {
    Job job = Job::Builder()
        .with_id("default-test-1")
        .with_type("test_job")
        .with_payload("test_payload")
        .build();

    EXPECT_EQ(job.priority(), JobPriority::Normal);
    EXPECT_EQ(job.status(), JobStatus::Pending);
    EXPECT_EQ(job.attempt_count(), 0u);
    EXPECT_EQ(job.max_attempts(), 1u);
    EXPECT_FALSE(job.scheduled_time().has_value());
    EXPECT_FALSE(job.error_details().has_value());
    EXPECT_FALSE(job.is_terminal());
}

TEST(JobTest, InvalidJobIdEmptyThrows) {
    EXPECT_THROW({
        JobId id("");
        static_cast<void>(id);
    }, JobValidationError);

    EXPECT_THROW({
        auto j = Job::Builder()
            .with_id("")
            .with_type("test")
            .with_payload("{}")
            .build();
        static_cast<void>(j);
    }, JobValidationError);
}

TEST(JobTest, InvalidJobIdContainingWhitespaceThrows) {
    EXPECT_THROW({ JobId id("job with space"); static_cast<void>(id); }, JobValidationError);
    EXPECT_THROW({ JobId id(" job"); static_cast<void>(id); }, JobValidationError);
    EXPECT_THROW({ JobId id("job\t1"); static_cast<void>(id); }, JobValidationError);
    EXPECT_THROW({ JobId id("job\n2"); static_cast<void>(id); }, JobValidationError);
}

TEST(JobTest, InvalidJobIdContainingDisallowedPunctuationThrows) {
    EXPECT_THROW({ JobId id("job!1"); static_cast<void>(id); }, JobValidationError);
    EXPECT_THROW({ JobId id("job#hash"); static_cast<void>(id); }, JobValidationError);
    EXPECT_THROW({ JobId id("job$dollar"); static_cast<void>(id); }, JobValidationError);
    EXPECT_THROW({ JobId id("job/slash"); static_cast<void>(id); }, JobValidationError);
    EXPECT_THROW({ JobId id("job\\backslash"); static_cast<void>(id); }, JobValidationError);
    EXPECT_THROW({ JobId id("job*wildcard"); static_cast<void>(id); }, JobValidationError);
}

TEST(JobTest, ValidJobIdWithPermittedSymbols) {
    EXPECT_NO_THROW({ JobId id("job_123-abc.def:test"); static_cast<void>(id); });
    JobId id("job_123-abc.def:test");
    EXPECT_EQ(id.value(), "job_123-abc.def:test");
}

TEST(JobTest, InvalidJobIdTooLongThrows) {
    const std::string oversized_id(129, 'a');
    EXPECT_THROW({
        JobId test_id(oversized_id);
        static_cast<void>(test_id);
    }, JobValidationError);

    const std::string max_valid_id(128, 'a');
    EXPECT_NO_THROW({
        JobId test_id(max_valid_id);
        static_cast<void>(test_id);
    });
}

TEST(JobTest, InvalidJobTypeEmptyThrows) {
    EXPECT_THROW({
        auto j = Job::Builder()
            .with_id("valid-id")
            .with_type("")
            .with_payload("{}")
            .build();
        static_cast<void>(j);
    }, JobValidationError);
}

TEST(JobTest, AttemptLimitValidationZeroMaxAttemptsThrows) {
    EXPECT_THROW({
        auto j = Job::Builder()
            .with_id("valid-id")
            .with_type("valid-type")
            .with_payload("{}")
            .with_max_attempts(0)
            .build();
        static_cast<void>(j);
    }, JobValidationError);
}

TEST(JobTest, AttemptLimitValidationAttemptCountExceedingMaxThrows) {
    EXPECT_THROW({
        auto j = Job::Builder()
            .with_id("valid-id")
            .with_type("valid-type")
            .with_payload("{}")
            .with_attempt_count(3)
            .with_max_attempts(2)
            .build();
        static_cast<void>(j);
    }, JobValidationError);
}

TEST(JobTest, IncrementAttemptCountRespectsLimit) {
    Job job = Job::Builder()
        .with_id("retry-job-1")
        .with_type("network_request")
        .with_payload("{}")
        .with_max_attempts(2)
        .build();

    EXPECT_EQ(job.attempt_count(), 0u);
    job.increment_attempt_count();
    EXPECT_EQ(job.attempt_count(), 1u);
    job.increment_attempt_count();
    EXPECT_EQ(job.attempt_count(), 2u);

    // Incrementing past max_attempts must throw
    EXPECT_THROW(job.increment_attempt_count(), JobValidationError);
}

// -----------------------------------------------------------------------------
// Lifecycle State Transition Tests
// -----------------------------------------------------------------------------

TEST(JobTest, ValidLifecycleTransitionsSucceed) {
    Job job = Job::Builder()
        .with_id("lifecycle-job-1")
        .with_type("render_task")
        .with_payload("{}")
        .with_max_attempts(3)
        .build();

    EXPECT_EQ(job.status(), JobStatus::Pending);

    job.mark_queued();
    EXPECT_EQ(job.status(), JobStatus::Queued);

    job.mark_running();
    EXPECT_EQ(job.status(), JobStatus::Running);

    job.mark_completed();
    EXPECT_EQ(job.status(), JobStatus::Completed);
    EXPECT_TRUE(job.is_terminal());
}

TEST(JobTest, FailedJobLifecycleAndRetryCheck) {
    Job job = Job::Builder()
        .with_id("failing-job-1")
        .with_type("payment_charge")
        .with_payload("{}")
        .with_max_attempts(2)
        .build();

    job.mark_queued();
    job.mark_running();
    job.increment_attempt_count();
    job.mark_failed("Card network timeout");

    EXPECT_EQ(job.status(), JobStatus::Failed);
    ASSERT_TRUE(job.error_details().has_value());
    EXPECT_EQ(*job.error_details(), "Card network timeout");
    EXPECT_TRUE(job.can_retry());

    // Failed job can transition back to Queued if retries remain
    job.mark_queued();
    EXPECT_EQ(job.status(), JobStatus::Queued);

    job.mark_running();
    job.increment_attempt_count();
    job.mark_failed("Card network still down");
    EXPECT_FALSE(job.can_retry()); // attempt_count == max_attempts
}

TEST(JobTest, IllegalLifecycleTransitionsThrow) {
    Job pending_job = Job::Builder()
        .with_id("illegal-trans-1")
        .with_type("test")
        .with_payload("{}")
        .build();

    // Cannot jump straight from Pending to Completed
    EXPECT_THROW(pending_job.mark_completed(), JobValidationError);

    // Completed is terminal and cannot transition anywhere
    Job completed_job = Job::Builder()
        .with_id("completed-1")
        .with_type("test")
        .with_payload("{}")
        .with_status(JobStatus::Completed)
        .build();

    EXPECT_THROW(completed_job.mark_running(), JobValidationError);
    EXPECT_THROW(completed_job.mark_queued(), JobValidationError);
    EXPECT_THROW(completed_job.mark_cancelled(), JobValidationError);

    // Cancelled is terminal
    Job cancelled_job = Job::Builder()
        .with_id("cancelled-1")
        .with_type("test")
        .with_payload("{}")
        .with_status(JobStatus::Cancelled)
        .build();

    EXPECT_THROW(cancelled_job.mark_running(), JobValidationError);
    EXPECT_THROW(cancelled_job.mark_queued(), JobValidationError);
}

// -----------------------------------------------------------------------------
// Priority Comparison & Value Semantics Tests
// -----------------------------------------------------------------------------

TEST(JobTest, JobPriorityStrictOrdering) {
    EXPECT_LT(JobPriority::Low, JobPriority::Normal);
    EXPECT_LT(JobPriority::Normal, JobPriority::High);
    EXPECT_LT(JobPriority::High, JobPriority::Critical);

    EXPECT_GT(JobPriority::Critical, JobPriority::High);
    EXPECT_GT(JobPriority::High, JobPriority::Normal);
    EXPECT_GT(JobPriority::Normal, JobPriority::Low);

    EXPECT_EQ(JobPriority::Normal, JobPriority(50));
    EXPECT_EQ(JobPriority::High, JobPriority(100));

    // Custom priority levels
    JobPriority custom_mid(75);
    EXPECT_GT(custom_mid, JobPriority::Normal);
    EXPECT_LT(custom_mid, JobPriority::High);
}

TEST(JobTest, ValueSemanticsCopyAndMove) {
    Job original = Job::Builder()
        .with_id("value-test-1")
        .with_type("compute_hash")
        .with_payload("data_chunk_42")
        .with_priority(JobPriority::High)
        .build();

    // Copy constructor
    Job copy = original;
    EXPECT_EQ(copy, original);

    // Move constructor
    Job moved = std::move(copy);
    EXPECT_EQ(moved, original);
}

TEST(JobTest, JobStatusStringRoundtrip) {
    EXPECT_EQ(to_string(JobStatus::Pending), "Pending");
    EXPECT_EQ(to_string(JobStatus::Queued), "Queued");
    EXPECT_EQ(to_string(JobStatus::Running), "Running");
    EXPECT_EQ(to_string(JobStatus::Completed), "Completed");
    EXPECT_EQ(to_string(JobStatus::Failed), "Failed");
    EXPECT_EQ(to_string(JobStatus::Cancelled), "Cancelled");

    EXPECT_EQ(job_status_from_string("Pending"), JobStatus::Pending);
    EXPECT_EQ(job_status_from_string("queued"), JobStatus::Queued);
    EXPECT_EQ(job_status_from_string("Running"), JobStatus::Running);
    EXPECT_EQ(job_status_from_string("completed"), JobStatus::Completed);
    EXPECT_EQ(job_status_from_string("Failed"), JobStatus::Failed);
    EXPECT_EQ(job_status_from_string("cancelled"), JobStatus::Cancelled);
    EXPECT_EQ(job_status_from_string("invalid_status"), std::nullopt);
}
