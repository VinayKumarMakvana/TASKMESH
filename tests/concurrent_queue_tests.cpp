#include "taskmesh/concurrent_priority_queue.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace taskmesh;
using namespace std::chrono_literals;

TEST(ConcurrentPriorityQueueTest, BasicPushAndTryPop) {
    ConcurrentPriorityQueue queue;
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);

    Job job = Job::Builder().with_id("job-1").with_type("test").with_priority(JobPriority::Normal).build();
    queue.push(std::move(job));

    EXPECT_FALSE(queue.empty());
    EXPECT_EQ(queue.size(), 1);

    auto popped = queue.try_pop();
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(popped->id().value(), "job-1");
    EXPECT_TRUE(queue.empty());
}

TEST(ConcurrentPriorityQueueTest, PopWaitTimeoutOnEmptyQueueReturnsNullopt) {
    ConcurrentPriorityQueue queue;
    const auto start = std::chrono::steady_clock::now();
    auto result = queue.pop_wait(50ms);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(result.has_value());
    EXPECT_GE(elapsed, 40ms);
}

TEST(ConcurrentPriorityQueueTest, PopWaitWakesImmediatelyOnPush) {
    ConcurrentPriorityQueue queue;
    std::atomic<bool> producer_done{false};

    std::jthread producer([&queue, &producer_done]() {
        std::this_thread::sleep_for(40ms);
        producer_done = true;
        queue.push(Job::Builder().with_id("wake-job").with_type("wake").with_priority(JobPriority::High).build());
    });

    const auto start = std::chrono::steady_clock::now();
    auto result = queue.pop_wait(500ms);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->id().value(), "wake-job");
    EXPECT_TRUE(producer_done);
    EXPECT_LT(elapsed, 400ms);
}

TEST(ConcurrentPriorityQueueTest, GracefulDrainAllowsRemainingJobsToDrain) {
    ConcurrentPriorityQueue queue;
    queue.push(Job::Builder().with_id("d-1").with_type("test").with_priority(JobPriority::Normal).build());
    queue.push(Job::Builder().with_id("d-2").with_type("test").with_priority(JobPriority::High).build());

    queue.close(true); // Graceful drain
    EXPECT_TRUE(queue.is_draining());
    EXPECT_THROW(queue.push(Job::Builder().with_id("d-3").with_type("test").build()), std::runtime_error);

    // High priority popped first
    auto p1 = queue.pop_wait(100ms);
    ASSERT_TRUE(p1.has_value());
    EXPECT_EQ(p1->id().value(), "d-2");

    auto p2 = queue.pop_wait(100ms);
    ASSERT_TRUE(p2.has_value());
    EXPECT_EQ(p2->id().value(), "d-1");

    // Once empty, pop_wait returns nullopt immediately
    auto p3 = queue.pop_wait(50ms);
    EXPECT_FALSE(p3.has_value());
}

TEST(ConcurrentPriorityQueueTest, ImmediateStopDrainAllExtractsAllJobs) {
    ConcurrentPriorityQueue queue;
    queue.push(Job::Builder().with_id("s-1").with_type("test").with_priority(JobPriority::Normal).build());
    queue.push(Job::Builder().with_id("s-2").with_type("test").with_priority(JobPriority::Critical).build());
    queue.push(Job::Builder().with_id("s-3").with_type("test").with_priority(JobPriority::Low).build());

    queue.close(false); // Immediate stop
    EXPECT_TRUE(queue.is_closed());

    auto drained = queue.drain_all();
    EXPECT_EQ(drained.size(), 3);
    EXPECT_TRUE(queue.empty());
    // Critical priority came out first
    EXPECT_EQ(drained[0].id().value(), "s-2");
}

TEST(ConcurrentPriorityQueueTest, MultiThreadedProducerConsumerStress) {
    ConcurrentPriorityQueue queue;
    constexpr size_t kNumProducers = 4;
    constexpr size_t kNumConsumers = 4;
    constexpr size_t kJobsPerProducer = 250;
    constexpr size_t kTotalJobs = kNumProducers * kJobsPerProducer;

    std::atomic<size_t> consumed_count{0};
    std::vector<std::jthread> consumers;
    consumers.reserve(kNumConsumers);

    for (size_t c = 0; c < kNumConsumers; ++c) {
        consumers.emplace_back([&queue, &consumed_count]() {
            while (true) {
                auto job_opt = queue.pop_wait(50ms);
                if (job_opt.has_value()) {
                    consumed_count.fetch_add(1, std::memory_order_relaxed);
                } else if (queue.is_draining() && queue.empty()) {
                    break;
                }
            }
        });
    }

    std::vector<std::jthread> producers;
    producers.reserve(kNumProducers);
    for (size_t p = 0; p < kNumProducers; ++p) {
        producers.emplace_back([&queue, p]() {
            for (size_t i = 0; i < kJobsPerProducer; ++i) {
                const auto priority = static_cast<JobPriority>(i % 5);
                queue.push(Job::Builder()
                               .with_id("stress-p" + std::to_string(p) + "-j" + std::to_string(i))
                               .with_type("stress")
                               .with_priority(priority)
                               .build());
            }
        });
    }

    // Join producers
    producers.clear();

    // Signal drain and wait for consumers
    queue.close(true);
    consumers.clear();

    EXPECT_EQ(consumed_count.load(), kTotalJobs);
    EXPECT_TRUE(queue.empty());
}
