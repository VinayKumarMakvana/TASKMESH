#include <gtest/gtest.h>

#include "taskmesh/job.hpp"
#include "taskmesh/priority_scheduler.hpp"

#include <random>
#include <string>
#include <vector>

using namespace taskmesh;

// -----------------------------------------------------------------------------
// Priority Scheduler Tests
// -----------------------------------------------------------------------------

TEST(PrioritySchedulerTest, EmptyQueueBehaviour) {
    PriorityScheduler scheduler;

    EXPECT_TRUE(scheduler.empty());
    EXPECT_EQ(scheduler.size(), 0u);
    EXPECT_EQ(scheduler.dequeue(), std::nullopt);
    EXPECT_EQ(scheduler.peek(), std::nullopt);
    EXPECT_EQ(scheduler.peek_ptr(), nullptr);
}

TEST(PrioritySchedulerTest, HigherPriorityComesFirst) {
    PriorityScheduler scheduler;

    // Enqueue in ascending priority order: Low -> Normal -> High -> Critical
    scheduler.enqueue(Job::Builder().with_id("job-low").with_type("test").with_priority(JobPriority::Low).build());
    scheduler.enqueue(Job::Builder().with_id("job-norm").with_type("test").with_priority(JobPriority::Normal).build());
    scheduler.enqueue(Job::Builder().with_id("job-high").with_type("test").with_priority(JobPriority::High).build());
    scheduler.enqueue(Job::Builder().with_id("job-crit").with_type("test").with_priority(JobPriority::Critical).build());

    EXPECT_EQ(scheduler.size(), 4u);
    EXPECT_FALSE(scheduler.empty());

    // Expected dequeue order: Critical -> High -> Normal -> Low
    auto j1 = scheduler.dequeue();
    ASSERT_TRUE(j1.has_value());
    EXPECT_EQ(j1->id().value(), "job-crit");
    EXPECT_EQ(j1->priority(), JobPriority::Critical);

    auto j2 = scheduler.dequeue();
    ASSERT_TRUE(j2.has_value());
    EXPECT_EQ(j2->id().value(), "job-high");

    auto j3 = scheduler.dequeue();
    ASSERT_TRUE(j3.has_value());
    EXPECT_EQ(j3->id().value(), "job-norm");

    auto j4 = scheduler.dequeue();
    ASSERT_TRUE(j4.has_value());
    EXPECT_EQ(j4->id().value(), "job-low");

    EXPECT_TRUE(scheduler.empty());
    EXPECT_EQ(scheduler.size(), 0u);
}

TEST(PrioritySchedulerTest, EqualPriorityFollowsStrictFIFO) {
    PriorityScheduler scheduler;

    // All jobs share the exact same priority (Normal)
    scheduler.enqueue(Job::Builder().with_id("fifo-1").with_type("task").with_priority(JobPriority::Normal).build());
    scheduler.enqueue(Job::Builder().with_id("fifo-2").with_type("task").with_priority(JobPriority::Normal).build());
    scheduler.enqueue(Job::Builder().with_id("fifo-3").with_type("task").with_priority(JobPriority::Normal).build());
    scheduler.enqueue(Job::Builder().with_id("fifo-4").with_type("task").with_priority(JobPriority::Normal).build());

    // Must dequeue in exact arrival order
    EXPECT_EQ(scheduler.dequeue()->id().value(), "fifo-1");
    EXPECT_EQ(scheduler.dequeue()->id().value(), "fifo-2");
    EXPECT_EQ(scheduler.dequeue()->id().value(), "fifo-3");
    EXPECT_EQ(scheduler.dequeue()->id().value(), "fifo-4");

    EXPECT_TRUE(scheduler.empty());
}

TEST(PrioritySchedulerTest, MultiplePrioritiesMixedTogether) {
    PriorityScheduler scheduler;

    // Interleaved sequence:
    // Normal 1, Critical 1, Low 1, High 1, Normal 2, Critical 2, High 2
    scheduler.enqueue(Job::Builder().with_id("norm-1").with_type("t").with_priority(JobPriority::Normal).build());
    scheduler.enqueue(Job::Builder().with_id("crit-1").with_type("t").with_priority(JobPriority::Critical).build());
    scheduler.enqueue(Job::Builder().with_id("low-1").with_type("t").with_priority(JobPriority::Low).build());
    scheduler.enqueue(Job::Builder().with_id("high-1").with_type("t").with_priority(JobPriority::High).build());
    scheduler.enqueue(Job::Builder().with_id("norm-2").with_type("t").with_priority(JobPriority::Normal).build());
    scheduler.enqueue(Job::Builder().with_id("crit-2").with_type("t").with_priority(JobPriority::Critical).build());
    scheduler.enqueue(Job::Builder().with_id("high-2").with_type("t").with_priority(JobPriority::High).build());

    std::vector<std::string> expected_order = {
        "crit-1", // Critical FIFO 1
        "crit-2", // Critical FIFO 2
        "high-1", // High FIFO 1
        "high-2", // High FIFO 2
        "norm-1", // Normal FIFO 1
        "norm-2", // Normal FIFO 2
        "low-1"   // Low
    };

    for (const auto& expected_id : expected_order) {
        auto job = scheduler.dequeue();
        ASSERT_TRUE(job.has_value());
        EXPECT_EQ(job->id().value(), expected_id);
    }

    EXPECT_TRUE(scheduler.empty());
}

TEST(PrioritySchedulerTest, PeekDoesNotRemoveJob) {
    PriorityScheduler scheduler;

    scheduler.enqueue(Job::Builder().with_id("p1").with_type("t").with_priority(JobPriority::Low).build());
    scheduler.enqueue(Job::Builder().with_id("p2").with_type("t").with_priority(JobPriority::High).build());

    EXPECT_EQ(scheduler.size(), 2u);

    // Peek copy
    auto peek1 = scheduler.peek();
    ASSERT_TRUE(peek1.has_value());
    EXPECT_EQ(peek1->id().value(), "p2");
    EXPECT_EQ(scheduler.size(), 2u);

    // Peek pointer
    const Job* peek_ptr = scheduler.peek_ptr();
    ASSERT_NE(peek_ptr, nullptr);
    EXPECT_EQ(peek_ptr->id().value(), "p2");

    // Dequeue must return the exact same item
    auto dequeued = scheduler.dequeue();
    ASSERT_TRUE(dequeued.has_value());
    EXPECT_EQ(dequeued->id().value(), "p2");
    EXPECT_EQ(scheduler.size(), 1u);
}

TEST(PrioritySchedulerTest, InvalidJobRejectionDoesNotCorruptQueue) {
    PriorityScheduler scheduler;

    // Enqueue one valid job
    scheduler.enqueue(Job::Builder().with_id("valid-1").with_type("t").build());
    EXPECT_EQ(scheduler.size(), 1u);

    // 1. Terminal Completed job
    Job completed_job = Job::Builder()
        .with_id("completed-job")
        .with_type("t")
        .with_status(JobStatus::Completed)
        .build();
    EXPECT_THROW(scheduler.enqueue(completed_job), JobValidationError);

    // 2. Terminal Cancelled job
    Job cancelled_job = Job::Builder()
        .with_id("cancelled-job")
        .with_type("t")
        .with_status(JobStatus::Cancelled)
        .build();
    EXPECT_THROW(scheduler.enqueue(cancelled_job), JobValidationError);

    // 3. Already Running job
    Job running_job = Job::Builder()
        .with_id("running-job")
        .with_type("t")
        .with_status(JobStatus::Running)
        .build();
    EXPECT_THROW(scheduler.enqueue(running_job), JobValidationError);

    // Verify queue size and contents remained untouched
    EXPECT_EQ(scheduler.size(), 1u);
    EXPECT_EQ(scheduler.dequeue()->id().value(), "valid-1");
    EXPECT_TRUE(scheduler.empty());
}

TEST(PrioritySchedulerTest, RepeatedEnqueueDequeueInterleaved) {
    PriorityScheduler scheduler;

    // Round 1: push 3, pop 1
    scheduler.enqueue(Job::Builder().with_id("r1-1").with_type("t").with_priority(JobPriority::Normal).build());
    scheduler.enqueue(Job::Builder().with_id("r1-2").with_type("t").with_priority(JobPriority::High).build());
    scheduler.enqueue(Job::Builder().with_id("r1-3").with_type("t").with_priority(JobPriority::Low).build());
    EXPECT_EQ(scheduler.dequeue()->id().value(), "r1-2"); // High popped

    // Round 2: push 2 (Critical and High), pop 2
    scheduler.enqueue(Job::Builder().with_id("r2-1").with_type("t").with_priority(JobPriority::Critical).build());
    scheduler.enqueue(Job::Builder().with_id("r2-2").with_type("t").with_priority(JobPriority::High).build());
    EXPECT_EQ(scheduler.dequeue()->id().value(), "r2-1"); // Critical popped
    EXPECT_EQ(scheduler.dequeue()->id().value(), "r2-2"); // High popped

    // Round 3: remaining should be r1-1 (Normal), r1-3 (Low)
    EXPECT_EQ(scheduler.dequeue()->id().value(), "r1-1");
    EXPECT_EQ(scheduler.dequeue()->id().value(), "r1-3");
    EXPECT_TRUE(scheduler.empty());
}

TEST(PrioritySchedulerTest, ClearResetsStateAndCounter) {
    PriorityScheduler scheduler;

    scheduler.enqueue(Job::Builder().with_id("c1").with_type("t").build());
    scheduler.enqueue(Job::Builder().with_id("c2").with_type("t").build());
    EXPECT_EQ(scheduler.size(), 2u);
    EXPECT_EQ(scheduler.current_sequence_number(), 2u);

    scheduler.clear();
    EXPECT_EQ(scheduler.size(), 0u);
    EXPECT_TRUE(scheduler.empty());
    EXPECT_EQ(scheduler.current_sequence_number(), 0u);
}

// -----------------------------------------------------------------------------
// Deterministic Ordering Invariant & Stress Test
// -----------------------------------------------------------------------------

TEST(PrioritySchedulerTest, DeterministicStressBatchOrderingInvariant) {
    PriorityScheduler scheduler;

    constexpr size_t kBatchSize = 5000;
    // Deterministic Mersenne Twister RNG with fixed seed for reproducible test runs
    std::mt19937 rng(1337);
    std::uniform_int_distribution<int32_t> priority_dist(1, 10);

    struct ExpectedEntry {
        std::string id;
        int32_t priority;
        size_t enqueue_idx;
    };

    std::vector<ExpectedEntry> enqueued_entries;
    enqueued_entries.reserve(kBatchSize);

    for (size_t i = 0; i < kBatchSize; ++i) {
        const int32_t prio = priority_dist(rng);
        const std::string id = "stress-job-" + std::to_string(i);

        scheduler.enqueue(Job::Builder()
            .with_id(id)
            .with_type("benchmark")
            .with_priority(JobPriority(prio))
            .build());

        enqueued_entries.push_back({id, prio, i});
    }

    EXPECT_EQ(scheduler.size(), kBatchSize);

    // Drain the entire queue and verify the ordering invariant:
    // For every pair (A, B) where A is dequeued immediately before B:
    // 1. A.priority >= B.priority
    // 2. If A.priority == B.priority, A must have arrived before B (FIFO).

    std::optional<Job> prev_job = std::nullopt;
    size_t dequeued_count = 0;

    // Quick lookup table to find original enqueue index by id
    std::unordered_map<std::string, size_t> id_to_arrival;
    id_to_arrival.reserve(kBatchSize);
    for (const auto& entry : enqueued_entries) {
        id_to_arrival[entry.id] = entry.enqueue_idx;
    }

    while (!scheduler.empty()) {
        auto current_opt = scheduler.dequeue();
        ASSERT_TRUE(current_opt.has_value());
        const Job& current = *current_opt;
        ++dequeued_count;

        if (prev_job.has_value()) {
            const Job& prev = *prev_job;
            // Invariant 1: Higher priority must precede lower priority
            EXPECT_GE(prev.priority().value(), current.priority().value())
                << "Ordering violation: " << prev.id().value() << " (prio " << prev.priority().value()
                << ") dequeued before " << current.id().value() << " (prio " << current.priority().value() << ")";

            // Invariant 2: Equal priority must preserve FIFO
            if (prev.priority().value() == current.priority().value()) {
                const size_t prev_arrival = id_to_arrival.at(prev.id().value());
                const size_t curr_arrival = id_to_arrival.at(current.id().value());
                EXPECT_LT(prev_arrival, curr_arrival)
                    << "FIFO violation at priority " << current.priority().value()
                    << ": " << prev.id().value() << " (arrival " << prev_arrival << ") "
                    << "vs " << current.id().value() << " (arrival " << curr_arrival << ")";
            }
        }

        prev_job = current;
    }

    EXPECT_EQ(dequeued_count, kBatchSize);
    EXPECT_TRUE(scheduler.empty());
}
