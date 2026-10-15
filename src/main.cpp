#include "taskmesh/job.hpp"
#include "taskmesh/job_executor.hpp"
#include "taskmesh/priority_scheduler.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

void print_separator(char ch = '=', int width = 78) {
    std::cout << std::string(width, ch) << "\n";
}

void print_banner() {
    print_separator();
    std::cout << "  TASKMESH — Priority Job Scheduling Engine (Phase 1 CLI Demonstration)\n";
    std::cout << "  Engine: C++20 | Monotonic FIFO Tie-Breaking | Modular Architecture\n";
    print_separator();
}

} // namespace

int main() {
    print_banner();

    taskmesh::PriorityScheduler scheduler;
    taskmesh::InMemoryJobExecutor executor;

    // Register a specialized handler for demonstration
    executor.register_handler("payment_processing", [](const taskmesh::Job& job) {
        std::cout << "      [Worker] Executing financial transaction payload: " << job.payload() << "\n";
        return taskmesh::ExecutionResult::ok("Funds settled and transaction ledger updated.");
    });

    executor.register_handler("security_audit", [](const taskmesh::Job& job) {
        std::cout << "      [Worker] Scanning critical infrastructure: " << job.payload() << "\n";
        return taskmesh::ExecutionResult::ok("Vulnerability scan clean; no CVEs found.");
    });

    std::cout << "\n[1] DEMONSTRATING DOMAIN VALIDATION AND ERROR HANDLING:\n";
    print_separator('-');

    // 1. Invalid Job Identifier
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

    // 2. Invalid Max Attempts
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

    // 3. Enqueuing job with terminal status
    try {
        std::cout << "  Attempting to enqueue already completed job: ";
        taskmesh::Job completed_job = taskmesh::Job::Builder()
            .with_id("completed-job-01")
            .with_type("archive")
            .with_payload("{}")
            .with_status(taskmesh::JobStatus::Completed)
            .build();
        scheduler.enqueue(std::move(completed_job));
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

    // Notice the arrival order: Normal 1, Low, High, Normal 2, Critical, Normal 3
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

    std::cout << "\n[3] EXECUTING JOBS (Observed Dequeue Order):\n";
    print_separator('-');
    std::cout << "  Expected Order:\n"
              << "    1. Critical (sec-audit-001)\n"
              << "    2. High     (pay-proc-101)\n"
              << "    3. Normal 1 (notif-email-01)  <-- FIFO Equal Priority Check\n"
              << "    4. Normal 2 (report-gen-02)   <-- FIFO Equal Priority Check\n"
              << "    5. Normal 3 (cache-sync-03)   <-- FIFO Equal Priority Check\n"
              << "    6. Low      (telemetry-99)\n";
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

    std::cout << "\n[4] EMPTY QUEUE SAFETY CHECK:\n";
    print_separator('-');
    auto empty_dequeue = scheduler.dequeue();
    if (!empty_dequeue.has_value()) {
        std::cout << "  Safely dequeued from empty queue -> std::nullopt received as expected.\n";
    } else {
        std::cerr << "  ERROR: Dequeued item from empty queue!\n";
    }

    std::cout << "\n[5] EXECUTION SUMMARY:\n";
    print_separator('-');
    std::cout << "  Total Executed  : " << executor.executed_count() << "\n";
    std::cout << "  Successful Jobs : " << executor.successful_count() << "\n";
    std::cout << "  Failed Jobs     : " << executor.failed_count() << "\n";
    std::cout << "  Remaining Queue : " << scheduler.size() << "\n";
    print_separator();
    std::cout << "  TaskMesh Phase 1 Demonstration Completed Successfully.\n";
    print_separator();

    return 0;
}
