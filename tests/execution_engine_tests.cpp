#include "taskmesh/execution_engine.hpp"
#include "taskmesh/job_executor.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <latch>
#include <thread>
#include <vector>

using namespace taskmesh;
using namespace std::chrono_literals;

namespace {

// Real legacy subclass implementing only Phase 1 pure virtual execute(Job&)
class LegacyPhase1Executor : public IJobExecutor {
public:
    ExecutionResult execute(Job& job) override {
        call_count.fetch_add(1, std::memory_order_relaxed);
        (void)job;
        return ExecutionResult::ok("Legacy execution succeeded");
    }

    std::atomic<size_t> call_count{0};
};

} // namespace

TEST(ExecutionEngineTest, MultiWorkerProcessesJobsSuccessfully) {
    InMemoryJobExecutor executor;
    std::atomic<int> counter{0};
    executor.register_handler("increment", [&counter](const Job&, const JobExecutionContext&) {
        counter.fetch_add(1, std::memory_order_relaxed);
        return ExecutionResult::ok();
    });

    ExecutionEngine engine(executor, {.num_workers = 4, .worker_poll_timeout = 20ms});

    constexpr int kJobCount = 20;
    for (int i = 0; i < kJobCount; ++i) {
        Job job = Job::Builder().with_id("inc-" + std::to_string(i)).with_type("increment").build();
        EXPECT_TRUE(engine.submit(std::move(job)));
    }

    engine.shutdown(ShutdownMode::GracefulDrain);
    EXPECT_TRUE(engine.wait_until_stopped(2000ms));
    EXPECT_EQ(counter.load(), kJobCount);

    const auto metrics = engine.get_metrics();
    EXPECT_TRUE(metrics.check_invariant());
    EXPECT_EQ(metrics.submitted, kJobCount);
    EXPECT_EQ(metrics.completed, kJobCount);
    EXPECT_EQ(metrics.queued, 0);
    EXPECT_EQ(metrics.running, 0);
    EXPECT_EQ(metrics.failed, 0);
    EXPECT_EQ(metrics.cancelled, 0);
}

TEST(ExecutionEngineTest, LegacySubclassExecutorWorksWithoutModification) {
    LegacyPhase1Executor legacy_executor;
    ExecutionEngine engine(legacy_executor, {.num_workers = 2, .worker_poll_timeout = 20ms});

    constexpr int kJobCount = 10;
    for (int i = 0; i < kJobCount; ++i) {
        Job job = Job::Builder().with_id("legacy-" + std::to_string(i)).with_type("legacy").build();
        EXPECT_TRUE(engine.submit(std::move(job)));
    }

    engine.shutdown(ShutdownMode::GracefulDrain);
    EXPECT_TRUE(engine.wait_until_stopped(2000ms));
    EXPECT_EQ(legacy_executor.call_count.load(), kJobCount);

    const auto metrics = engine.get_metrics();
    EXPECT_TRUE(metrics.check_invariant());
    EXPECT_EQ(metrics.completed, kJobCount);
}

TEST(ExecutionEngineTest, PreExecutionCancellationSkipsHandlerAndMarksCancelled) {
    InMemoryJobExecutor executor;
    std::atomic<bool> handler_called{false};
    executor.register_handler("guarded", [&handler_called](const Job&, const JobExecutionContext&) {
        handler_called = true;
        return ExecutionResult::ok();
    });

    // Start with 1 worker
    ExecutionEngine engine(executor, {.num_workers = 1, .worker_poll_timeout = 20ms});

    // Submit a blocking job to occupy worker
    std::latch worker_busy(1);
    std::latch allow_finish(1);
    executor.register_handler("blocker", [&worker_busy, &allow_finish](const Job&, const JobExecutionContext&) {
        worker_busy.count_down();
        allow_finish.wait();
        return ExecutionResult::ok();
    });

    EXPECT_TRUE(engine.submit(Job::Builder().with_id("job-blocker").with_type("blocker").build()));
    worker_busy.wait(); // Worker is now busy with blocker

    // Submit a queued job
    EXPECT_TRUE(engine.submit(Job::Builder().with_id("job-to-cancel").with_type("guarded").build()));

    // Cancel the queued job while worker is still blocked on blocker
    EXPECT_TRUE(engine.cancel_job(JobId("job-to-cancel")));

    // Now unblock worker
    allow_finish.count_down();

    engine.shutdown(ShutdownMode::GracefulDrain);
    EXPECT_TRUE(engine.wait_until_stopped(2000ms));

    // Handler for "job-to-cancel" must never have been called!
    EXPECT_FALSE(handler_called.load());

    const auto metrics = engine.get_metrics();
    EXPECT_TRUE(metrics.check_invariant());
    EXPECT_EQ(metrics.submitted, 2);
    EXPECT_EQ(metrics.completed, 1);
    EXPECT_EQ(metrics.cancelled, 1);
}

TEST(ExecutionEngineTest, CooperativeCancellationDuringExecution) {
    InMemoryJobExecutor executor;
    std::latch handler_running(1);

    executor.register_handler("cancellable", [&handler_running](const Job&, const JobExecutionContext& ctx) {
        handler_running.count_down();
        // Wait for cancellation signal
        for (int i = 0; i < 50; ++i) {
            if (ctx.is_cancellation_requested()) {
                return ExecutionResult::cancelled("Cooperative cancellation acknowledged.");
            }
            std::this_thread::sleep_for(10ms);
        }
        return ExecutionResult::ok("Completed without cancellation");
    });

    ExecutionEngine engine(executor, {.num_workers = 1, .worker_poll_timeout = 20ms});
    EXPECT_TRUE(engine.submit(Job::Builder().with_id("job-coop").with_type("cancellable").build()));

    handler_running.wait(); // Handler is actively running

    // Request cancellation
    EXPECT_TRUE(engine.cancel_job(JobId("job-coop")));

    engine.shutdown(ShutdownMode::GracefulDrain);
    EXPECT_TRUE(engine.wait_until_stopped(2000ms));

    const auto metrics = engine.get_metrics();
    EXPECT_TRUE(metrics.check_invariant());
    EXPECT_EQ(metrics.cancelled, 1);
    EXPECT_EQ(metrics.completed, 0);
}

TEST(ExecutionEngineTest, CancellationAfterTerminalStateReturnsFalse) {
    InMemoryJobExecutor executor;
    executor.register_handler("fast", [](const Job&, const JobExecutionContext&) {
        return ExecutionResult::ok();
    });

    ExecutionEngine engine(executor, {.num_workers = 1, .worker_poll_timeout = 20ms});
    EXPECT_TRUE(engine.submit(Job::Builder().with_id("fast-job").with_type("fast").build()));

    engine.shutdown(ShutdownMode::GracefulDrain);
    EXPECT_TRUE(engine.wait_until_stopped(2000ms));

    // Fast job has completed and is terminal
    EXPECT_FALSE(engine.cancel_job(JobId("fast-job")));

    // Unknown job cancellation also returns false
    EXPECT_FALSE(engine.cancel_job(JobId("non-existent-job")));
}

TEST(ExecutionEngineTest, ImmediateStopDrainsQueuedJobsAndCancelsRunning) {
    InMemoryJobExecutor executor;
    std::latch worker_started(1);
    std::latch worker_can_exit(1);

    executor.register_handler("slow", [&worker_started, &worker_can_exit](const Job&, const JobExecutionContext& ctx) {
        worker_started.count_down();
        while (!ctx.is_cancellation_requested()) {
            std::this_thread::sleep_for(10ms);
        }
        worker_can_exit.count_down();
        return ExecutionResult::cancelled("Immediate stop signal observed.");
    });

    ExecutionEngine engine(executor, {.num_workers = 1, .worker_poll_timeout = 20ms});

    // 1 running job
    EXPECT_TRUE(engine.submit(Job::Builder().with_id("slow-1").with_type("slow").build()));
    // 3 queued jobs
    EXPECT_TRUE(engine.submit(Job::Builder().with_id("q-1").with_type("slow").build()));
    EXPECT_TRUE(engine.submit(Job::Builder().with_id("q-2").with_type("slow").build()));
    EXPECT_TRUE(engine.submit(Job::Builder().with_id("q-3").with_type("slow").build()));

    worker_started.wait();

    // Call ImmediateStop
    engine.shutdown(ShutdownMode::ImmediateStop);
    worker_can_exit.wait();

    EXPECT_TRUE(engine.wait_until_stopped(2000ms));

    const auto metrics = engine.get_metrics();
    EXPECT_TRUE(metrics.check_invariant());
    EXPECT_EQ(metrics.submitted, 4);
    EXPECT_EQ(metrics.cancelled, 4); // 1 running + 3 queued
    EXPECT_EQ(metrics.completed, 0);
    EXPECT_EQ(metrics.queued, 0);
    EXPECT_EQ(metrics.running, 0);
}

TEST(ExecutionEngineTest, ExceptionInHandlerIsolatedAndAccountedAsFailed) {
    InMemoryJobExecutor executor;
    executor.register_handler("exploding", [](const Job&, const JobExecutionContext&) -> ExecutionResult {
        throw std::runtime_error("Simulated catastrophic failure in handler");
    });
    executor.register_handler("healthy", [](const Job&, const JobExecutionContext&) {
        return ExecutionResult::ok();
    });

    ExecutionEngine engine(executor, {.num_workers = 2, .worker_poll_timeout = 20ms});

    EXPECT_TRUE(engine.submit(Job::Builder().with_id("bad-1").with_type("exploding").build()));
    EXPECT_TRUE(engine.submit(Job::Builder().with_id("good-1").with_type("healthy").build()));

    engine.shutdown(ShutdownMode::GracefulDrain);
    EXPECT_TRUE(engine.wait_until_stopped(2000ms));

    const auto metrics = engine.get_metrics();
    EXPECT_TRUE(metrics.check_invariant());
    EXPECT_EQ(metrics.submitted, 2);
    EXPECT_EQ(metrics.failed, 1);
    EXPECT_EQ(metrics.completed, 1);
}

TEST(ExecutionEngineTest, ConcurrentLoadMetricsInvariantCheck) {
    InMemoryJobExecutor executor;
    executor.register_handler("fast", [](const Job&, const JobExecutionContext&) {
        return ExecutionResult::ok();
    });
    executor.register_handler("failing", [](const Job&, const JobExecutionContext&) {
        return ExecutionResult::error("Deliberate failure");
    });

    ExecutionEngine engine(executor, {.num_workers = 4, .worker_poll_timeout = 20ms});

    constexpr int kTotal = 200;
    std::atomic<bool> submitting{true};

    // Submitter thread
    std::jthread producer([&engine, &submitting]() {
        for (int i = 0; i < kTotal; ++i) {
            std::string type = (i % 5 == 0) ? "failing" : "fast";
            engine.submit(Job::Builder()
                              .with_id("load-" + std::to_string(i))
                              .with_type(type)
                              .with_priority(static_cast<JobPriority>(i % 5))
                              .build());
        }
        submitting = false;
    });

    // Observer thread continuously verifying the invariant while jobs are running
    std::jthread observer([&engine, &submitting]() {
        while (submitting.load() || !engine.is_stopped()) {
            const auto m = engine.get_metrics();
            EXPECT_TRUE(m.check_invariant());
            std::this_thread::sleep_for(2ms);
            if (engine.is_stopped()) break;
        }
    });

    producer.join();
    engine.shutdown(ShutdownMode::GracefulDrain);
    EXPECT_TRUE(engine.wait_until_stopped(3000ms));
    observer.join();

    const auto final_m = engine.get_metrics();
    EXPECT_TRUE(final_m.check_invariant());
    EXPECT_EQ(final_m.submitted, kTotal);
    EXPECT_EQ(final_m.completed, 160);
    EXPECT_EQ(final_m.failed, 40);
    EXPECT_EQ(final_m.queued, 0);
    EXPECT_EQ(final_m.running, 0);
}
