#include "taskmesh/execution_engine.hpp"
#include "taskmesh/job.hpp"
#include "taskmesh/job_executor.hpp"
#include "taskmesh/priority_scheduler.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

void print_separator(char ch = '=', int width = 78) {
    std::cout << std::string(width, ch) << "\n";
}

void print_banner() {
    print_separator();
    std::cout << "  TASKMESH — Concurrent Priority Job Engine (Phase 1 & 2 CLI)\n";
    std::cout << "  Engine: C++20 | Monotonic FIFO Priority Scheduling | Multithreaded Workers\n";
    print_separator();
}

} // namespace

int main() {
    print_banner();

    taskmesh::PriorityScheduler scheduler;
    taskmesh::InMemoryJobExecutor executor;

    // Register specialized handlers
    executor.register_handler("payment_processing", [](const taskmesh::Job& job) {
        std::cout << "      [Worker] Executing financial transaction payload: " << job.payload() << "\n";
        return taskmesh::ExecutionResult::ok("Funds settled and transaction ledger updated.");
    });

    executor.register_handler("security_audit", [](const taskmesh::Job& job) {
        std::cout << "      [Worker] Scanning critical infrastructure: " << job.payload() << "\n";
        return taskmesh::ExecutionResult::ok("Vulnerability scan clean; no CVEs found.");
    });

    executor.register_handler("long_computation", [](const taskmesh::Job& job, const taskmesh::JobExecutionContext& ctx) {
        std::cout << "      [Worker] Starting cancellable computation for: " << job.id().value() << "\n";
        for (int i = 0; i < 20; ++i) {
            if (ctx.is_cancellation_requested()) {
                std::cout << "      [Worker] Cooperative cancellation acknowledged for: " << job.id().value() << "\n";
                return taskmesh::ExecutionResult::cancelled("Operation cancelled via cooperative token.");
            }
            std::this_thread::sleep_for(15ms);
        }
        return taskmesh::ExecutionResult::ok("Computation complete.");
    });

    std::cout << "\n[1] DEMONSTRATING DOMAIN VALIDATION AND ERROR HANDLING:\n";
    print_separator('-');

    try {
        std::cout << "  Attempting to create job with invalid ID 'job/invalid#1': ";
        taskmesh::Job invalid_id_job = taskmesh::Job::Builder()
            .with_id("job/invalid#1")
            .with_type("test")
            .with_payload("{}")
            .build();
        std::cout << "UNEXPECTED SUCCESS\n";
    } catch (const taskmesh::JobValidationError& ex) {
        std::cout << "REJECTED (as expected)\n    Reason: " << ex.what() << "\n";
    }

    try {
        std::cout << "  Attempting to create job with max_attempts = 0: ";
        taskmesh::Job invalid_attempts_job = taskmesh::Job::Builder()
            .with_id("valid-id-01")
            .with_type("test")
            .with_payload("{}")
            .with_max_attempts(0)
            .build();
        std::cout << "UNEXPECTED SUCCESS\n";
    } catch (const taskmesh::JobValidationError& ex) {
        std::cout << "REJECTED (as expected)\n    Reason: " << ex.what() << "\n";
    }

    std::cout << "\n[2] ENQUEUING REPRESENTATIVE JOBS (Interleaved Priorities):\n";
    print_separator('-');

    struct JobSpec {
        std::string id;
        std::string type;
        std::string payload;
        taskmesh::JobPriority priority;
        std::string description;
    };

    const std::vector<JobSpec> jobs_to_enqueue = {
        {"notif-email-01",  "email_dispatch",      "{\"to\":\"user1@corp.internal\"}",      taskmesh::JobPriority::Normal,   "Normal Priority #1"},
        {"telemetry-99",    "telemetry_archive",   "{\"range\":\"2026-10-01_10-15\"}",       taskmesh::JobPriority::Low,      "Low Priority"},
        {"pay-proc-101",    "payment_processing",  "{\"amount\":12500.00,\"ccy\":\"USD\"}", taskmesh::JobPriority::High,     "High Priority"},
        {"report-gen-02",   "report_generation",   "{\"report\":\"quarterly_sales\"}",      taskmesh::JobPriority::Normal,   "Normal Priority #2"},
        {"sec-audit-001",   "security_audit",      "{\"target\":\"auth_subsystem\"}",        taskmesh::JobPriority::Critical, "Critical Priority"},
        {"cache-sync-03",   "cache_warmup",        "{\"region\":\"us-east\"}",               taskmesh::JobPriority::Normal,   "Normal Priority #3"}
    };

    for (const auto& spec : jobs_to_enqueue) {
        taskmesh::Job job = taskmesh::Job::Builder()
            .with_id(spec.id)
            .with_type(spec.type)
            .with_payload(spec.payload)
            .with_priority(spec.priority)
            .build();

        scheduler.enqueue(std::move(job));
        std::cout << "  Enqueued: [" << std::setw(13) << std::left << spec.priority.to_string() << "] "
                  << "ID: " << std::setw(16) << spec.id 
                  << " (" << spec.description << ")\n";
    }

    std::cout << "\n  Queue size after enqueue: " << scheduler.size() << " jobs pending.\n";

    std::cout << "\n[3] SEQUENTIAL SCHEDULER EXECUTION (Observed Dequeue Order):\n";
    print_separator('-');

    size_t execution_step = 1;
    while (!scheduler.empty()) {
        auto job_opt = scheduler.dequeue();
        if (!job_opt) {
            break;
        }

        taskmesh::Job& current_job = *job_opt;
        std::cout << "  Step " << execution_step++ << ": Dequeued "
                  << "[" << std::setw(13) << std::left << current_job.priority().to_string() << "] "
                  << "ID: " << std::setw(16) << current_job.id().value()
                  << " Type: " << current_job.type() << "\n";

        taskmesh::ExecutionResult result = executor.execute(current_job);
        std::cout << "      Result: " << (result.success ? "SUCCESS" : "FAILED")
                  << " (" << result.message << ")\n";
        std::cout << "      Final Job Status: " << to_string(current_job.status()) 
                  << ", Attempts: " << current_job.attempt_count() << "\n";
    }

    std::cout << "\n[4] CONCURRENT EXECUTION ENGINE (Multithreaded Worker Pool + Cancellation):\n";
    print_separator('-');

    {
        taskmesh::ExecutionEngine engine(executor, {.num_workers = 3, .worker_poll_timeout = 20ms});
        std::cout << "  Started ExecutionEngine with " << engine.worker_count() << " worker threads.\n";

        // Submit jobs
        engine.submit(taskmesh::Job::Builder().with_id("async-audit-1").with_type("security_audit").with_payload("{\"target\":\"api_gateway\"}").with_priority(taskmesh::JobPriority::Critical).build());
        engine.submit(taskmesh::Job::Builder().with_id("async-pay-1").with_type("payment_processing").with_payload("{\"amount\":99.99}").with_priority(taskmesh::JobPriority::High).build());
        engine.submit(taskmesh::Job::Builder().with_id("async-comp-1").with_type("long_computation").with_payload("{\"iter\":1000}").with_priority(taskmesh::JobPriority::Normal).build());
        engine.submit(taskmesh::Job::Builder().with_id("async-comp-cancel").with_type("long_computation").with_payload("{\"iter\":5000}").with_priority(taskmesh::JobPriority::Low).build());

        // Cancel one job cooperatively
        std::this_thread::sleep_for(25ms);
        bool cancelled = engine.cancel_job(taskmesh::JobId("async-comp-cancel"));
        std::cout << "  Requested cancellation of 'async-comp-cancel': " << (cancelled ? "SUCCESS" : "FAILED") << "\n";

        engine.shutdown(taskmesh::ShutdownMode::GracefulDrain);
        bool stopped = engine.wait_until_stopped(3000ms);
        std::cout << "  Engine shutdown and stopped cleanly: " << (stopped ? "YES" : "NO") << "\n";

        auto metrics = engine.get_metrics();
        std::cout << "\n  Engine Metrics Invariant Check: " << (metrics.check_invariant() ? "VALID" : "INVALID") << "\n";
        std::cout << "    Submitted : " << metrics.submitted << "\n";
        std::cout << "    Completed : " << metrics.completed << "\n";
        std::cout << "    Cancelled : " << metrics.cancelled << "\n";
        std::cout << "    Failed    : " << metrics.failed << "\n";
        std::cout << "    Queued    : " << metrics.queued << "\n";
        std::cout << "    Running   : " << metrics.running << "\n";
    }

    print_separator();
    std::cout << "  TaskMesh CLI Demonstration Completed Successfully.\n";
    print_separator();

    return 0;
}
