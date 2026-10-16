#define _CRT_SECURE_NO_WARNINGS
#include "taskmesh/server.hpp"

#undef CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib/httplib.h"
#include "nlohmann/json.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <sstream>

using json = nlohmann::json;
using namespace std::chrono_literals;

namespace taskmesh {

namespace {

std::string generate_random_id(const std::string& prefix = "job") {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist(100000, 999999);
    const auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return prefix + "-" + std::to_string(ts) + "-" + std::to_string(dist(rng));
}

json job_to_json(const Job& job) {
    const auto epoch_created = std::chrono::duration_cast<std::chrono::milliseconds>(
        job.creation_time().time_since_epoch()).count();

    json j = {
        {"id", job.id().value()},
        {"type", job.type()},
        {"payload", job.payload()},
        {"priority", job.priority().value()},
        {"priority_name", job.priority().to_string()},
        {"status", std::string(to_string(job.status()))},
        {"attempt_count", job.attempt_count()},
        {"max_attempts", job.max_attempts()},
        {"created_at_ms", epoch_created}
    };

    if (job.scheduled_time().has_value()) {
        j["scheduled_at_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            job.scheduled_time()->time_since_epoch()).count();
    }
    if (job.error_details().has_value()) {
        j["error_details"] = *job.error_details();
    }
    return j;
}

JobPriority parse_priority(const json& p_val) {
    if (p_val.is_number()) {
        return JobPriority(p_val.get<int32_t>());
    }
    if (p_val.is_string()) {
        std::string s = p_val.get<std::string>();
        if (s == "Low" || s == "low") return JobPriority::Low;
        if (s == "Normal" || s == "normal") return JobPriority::Normal;
        if (s == "High" || s == "high") return JobPriority::High;
        if (s == "Critical" || s == "critical") return JobPriority::Critical;
    }
    return JobPriority::Normal;
}

} // namespace

// =============================================================================
// RateLimiter Implementation
// =============================================================================

RateLimiter::RateLimiter(size_t max_requests_per_minute)
    : max_rpm_(max_requests_per_minute) {}

bool RateLimiter::allow(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();

    auto it = clients_.find(client_ip);
    if (it == clients_.end()) {
        clients_[client_ip] = {now, 1};
        return true;
    }

    auto& window = it->second;
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - window.window_start);

    if (elapsed >= 60s) {
        window.window_start = now;
        window.count = 1;
        return true;
    }

    if (window.count >= max_rpm_) {
        return false;
    }

    window.count++;
    return true;
}

// =============================================================================
// TaskMeshServer Implementation
// =============================================================================

TaskMeshServer::TaskMeshServer(ServerConfig config)
    : config_(std::move(config)),
      store_(std::make_unique<SqliteJobStore>(config_.db_path)),
      engine_(executor_, {.num_workers = config_.num_workers, .worker_poll_timeout = 25ms}),
      rate_limiter_(config_.rate_limit_rpm),
      start_time_(std::chrono::steady_clock::now()) {

    // Resolve API key from environment variable if not specified in config
    if (config_.api_key.empty()) {
        const char* env_key = std::getenv("TASKMESH_API_KEY");
        if (env_key) {
            config_.api_key = env_key;
        }
    }

    // Connect execution engine lifecycle transitions directly to persistent SQLite store
    engine_.set_lifecycle_callback([this](const Job& job, JobStatus new_status, const std::string& msg) {
        if (store_) {
            try {
                store_->update_status(job.id(), new_status, msg.empty() ? std::nullopt : std::make_optional(msg), job.attempt_count());
            } catch (...) {}
        }
    });

    register_worker_handlers();
    recover_and_enqueue_persisted_jobs();
    setup_routes();
}

TaskMeshServer::~TaskMeshServer() {
    stop();
}

void TaskMeshServer::register_worker_handlers() {
    // 1. Email task handler with cooperative cancellation
    executor_.register_handler("email", [](const Job&, const JobExecutionContext& ctx) -> ExecutionResult {
        for (int i = 0; i < 5; ++i) {
            if (ctx.is_cancellation_requested()) {
                return ExecutionResult::cancelled("Cancelled during email dispatch");
            }
            std::this_thread::sleep_for(15ms);
        }
        return ExecutionResult::ok("Email dispatched successfully");
    });

    // 2. Data sync handler
    executor_.register_handler("data_sync", [](const Job&, const JobExecutionContext& ctx) -> ExecutionResult {
        for (int i = 0; i < 6; ++i) {
            if (ctx.is_cancellation_requested()) {
                return ExecutionResult::cancelled("Cancelled during data sync");
            }
            std::this_thread::sleep_for(15ms);
        }
        return ExecutionResult::ok("Data sync settled");
    });

    // 3. Flaky task: transient failure on first attempt, recovers on retry
    executor_.register_handler("flaky", [](const Job& job, const JobExecutionContext& ctx) -> ExecutionResult {
        if (ctx.is_cancellation_requested()) return ExecutionResult::cancelled();
        if (job.attempt_count() < 2) {
            return ExecutionResult::error("Temporary upstream timeout (transient error)");
        }
        return ExecutionResult::ok("Recovered and succeeded on retry attempt");
    });

    // 4. Failing task: simulates permanent failure for error testing and manual retry verification
    executor_.register_handler("failing", [](const Job&, const JobExecutionContext& ctx) -> ExecutionResult {
        if (ctx.is_cancellation_requested()) return ExecutionResult::cancelled();
        return ExecutionResult::error("Fatal error: Unreachable target host connection refused");
    });

    // 5. Compute task: multi-stage computation
    executor_.register_handler("compute", [](const Job&, const JobExecutionContext& ctx) -> ExecutionResult {
        for (int i = 0; i < 8; ++i) {
            if (ctx.is_cancellation_requested()) return ExecutionResult::cancelled("Compute cancelled by user");
            std::this_thread::sleep_for(20ms);
        }
        return ExecutionResult::ok("Matrix transformation completed successfully");
    });

    // 6. Fallback default handler for any custom job type
    executor_.set_default_handler([](const Job&, const JobExecutionContext& ctx) -> ExecutionResult {
        if (ctx.is_cancellation_requested()) {
            return ExecutionResult::cancelled("Cancelled before processing");
        }
        std::this_thread::sleep_for(20ms);
        return ExecutionResult::ok("Executed via default task runner");
    });
}

void TaskMeshServer::recover_and_enqueue_persisted_jobs() {
    auto uncompleted = store_->recover_uncompleted_jobs();
    for (auto& job : uncompleted) {
        engine_.submit(std::move(job));
    }
}

bool TaskMeshServer::check_auth(const std::string& auth_header, const std::string& api_key_header) const {
    if (config_.api_key.empty()) {
        return true; // No auth required if API key not configured
    }

    if (!api_key_header.empty() && api_key_header == config_.api_key) {
        return true;
    }

    if (!auth_header.empty()) {
        if (auth_header.rfind("Bearer ", 0) == 0) {
            std::string token = auth_header.substr(7);
            if (token == config_.api_key) {
                return true;
            }
        }
    }

    return false;
}

void TaskMeshServer::setup_routes() {
    http_server_ = std::make_unique<httplib::Server>();

    // Global Pre-routing Handler: Rate Limiting & CORS
    http_server_->set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key");
        res.set_header("X-Content-Type-Options", "nosniff");
        res.set_header("X-Frame-Options", "DENY");

        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }

        if (!rate_limiter_.allow(req.remote_addr)) {
            res.status = 429;
            res.set_content(json{{"error", "Rate limit exceeded. Try again in 60 seconds."}}.dump(), "application/json");
            return httplib::Server::HandlerResponse::Handled;
        }

        return httplib::Server::HandlerResponse::Unhandled;
    });

    // 1. Health & Readiness Endpoints
    http_server_->Get("/health", [this](const httplib::Request&, httplib::Response& res) {
        const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time_).count();

        json body = {
            {"status", "healthy"},
            {"service", "TaskMesh Engine"},
            {"uptime_seconds", uptime},
            {"workers_active", engine_.worker_count()}
        };
        res.set_content(body.dump(), "application/json");
    });

    http_server_->Get("/ready", [this](const httplib::Request&, httplib::Response& res) {
        json body = {
            {"status", "ready"},
            {"database", "connected"},
            {"workers_count", engine_.worker_count()},
            {"queue_depth", engine_.queue_size()}
        };
        res.set_content(body.dump(), "application/json");
    });

    // 2. Metrics & Stats Endpoints
    http_server_->Get("/api/v1/metrics", [this](const httplib::Request&, httplib::Response& res) {
        auto m = engine_.get_metrics();
        json body = {
            {"submitted", m.submitted},
            {"queued", m.queued},
            {"running", m.running},
            {"completed", m.completed},
            {"failed", m.failed},
            {"cancelled", m.cancelled},
            {"invariant_valid", m.check_invariant()}
        };
        res.set_content(body.dump(), "application/json");
    });

    http_server_->Get("/api/v1/stats", [this](const httplib::Request&, httplib::Response& res) {
        auto counts = store_->count_by_status();
        json status_counts = {
            {"Pending", counts[JobStatus::Pending]},
            {"Queued", counts[JobStatus::Queued]},
            {"Running", counts[JobStatus::Running]},
            {"Completed", counts[JobStatus::Completed]},
            {"Failed", counts[JobStatus::Failed]},
            {"Cancelled", counts[JobStatus::Cancelled]}
        };
        res.set_content(status_counts.dump(), "application/json");
    });

    // 3. Submit Job (POST /api/v1/jobs)
    http_server_->Post("/api/v1/jobs", [this](const httplib::Request& req, httplib::Response& res) {
        if (!check_auth(req.get_header_value("Authorization"), req.get_header_value("X-API-Key"))) {
            res.status = 401;
            res.set_content(json{{"error", "Unauthorized: invalid or missing API key."}}.dump(), "application/json");
            return;
        }

        json req_body;
        try {
            req_body = json::parse(req.body);
        } catch (const std::exception& ex) {
            res.status = 400;
            res.set_content(json{{"error", std::string("Malformed JSON body: ") + ex.what()}}.dump(), "application/json");
            return;
        }

        // Validate type
        if (!req_body.contains("type") || !req_body["type"].is_string() || req_body["type"].get<std::string>().empty()) {
            res.status = 400;
            res.set_content(json{{"error", "Validation failed: 'type' must be a non-empty string."}}.dump(), "application/json");
            return;
        }

        const std::string job_type = req_body["type"].get<std::string>();

        // ID generation or validation
        std::string job_id_str = req_body.value("id", "");
        if (job_id_str.empty()) {
            job_id_str = generate_random_id(job_type);
        }

        try {
            JobId::validate(job_id_str);
        } catch (const std::exception& ex) {
            res.status = 400;
            res.set_content(json{{"error", std::string("Invalid job ID: ") + ex.what()}}.dump(), "application/json");
            return;
        }

        // Duplicate ID check
        if (store_->get_job(JobId(job_id_str)).has_value()) {
            res.status = 409;
            res.set_content(json{{"error", "Conflict: Job with ID '" + job_id_str + "' already exists."}}.dump(), "application/json");
            return;
        }

        // Payload
        std::string payload_str = "{}";
        if (req_body.contains("payload")) {
            if (req_body["payload"].is_string()) {
                payload_str = req_body["payload"].get<std::string>();
            } else {
                payload_str = req_body["payload"].dump();
            }
        }

        // Priority
        JobPriority priority = JobPriority::Normal;
        if (req_body.contains("priority")) {
            priority = parse_priority(req_body["priority"]);
        }

        // Max Attempts
        uint32_t max_attempts = req_body.value("max_attempts", 3u);
        if (max_attempts == 0) max_attempts = 1;

        // Build Job
        Job job = Job::Builder()
            .with_id(job_id_str)
            .with_type(job_type)
            .with_payload(payload_str)
            .with_priority(priority)
            .with_max_attempts(max_attempts)
            .with_status(JobStatus::Queued)
            .build();

        // 1. Persist to SQLite
        try {
            store_->save_job(job);
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"error", std::string("Database persistence error: ") + ex.what()}}.dump(), "application/json");
            return;
        }

        // 2. Schedule into Execution Engine
        bool submitted = engine_.submit(job);
        if (!submitted) {
            store_->update_status(job.id(), JobStatus::Failed, "Engine rejected job submission (stopping/closed)");
            res.status = 503;
            res.set_content(json{{"error", "Execution engine is shutting down or full."}}.dump(), "application/json");
            return;
        }

        res.status = 201;
        json res_json = {
            {"message", "Job scheduled successfully"},
            {"job", job_to_json(job)}
        };
        res.set_content(res_json.dump(), "application/json");
    });

    // 4. List Jobs (GET /api/v1/jobs)
    http_server_->Get("/api/v1/jobs", [this](const httplib::Request& req, httplib::Response& res) {
        std::optional<JobStatus> status_filter = std::nullopt;
        if (req.has_param("status")) {
            status_filter = job_status_from_string(req.get_param_value("status"));
        }

        size_t limit = 50;
        if (req.has_param("limit")) {
            try {
                limit = std::stoull(req.get_param_value("limit"));
                if (limit > 200) limit = 200;
            } catch (...) {}
        }

        size_t offset = 0;
        if (req.has_param("offset")) {
            try {
                offset = std::stoull(req.get_param_value("offset"));
            } catch (...) {}
        }

        auto jobs = store_->list_jobs(status_filter, limit, offset);
        json jobs_arr = json::array();
        for (const auto& j : jobs) {
            jobs_arr.push_back(job_to_json(j));
        }

        json body = {
            {"count", jobs.size()},
            {"limit", limit},
            {"offset", offset},
            {"jobs", jobs_arr}
        };
        res.set_content(body.dump(), "application/json");
    });

    // 5. Get Job Details (GET /api/v1/jobs/:id)
    http_server_->Get(R"(/api/v1/jobs/([a-zA-Z0-9_\-\.:]+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string job_id_str = req.matches[1];
        auto job = store_->get_job(JobId(job_id_str));
        if (!job.has_value()) {
            res.status = 404;
            res.set_content(json{{"error", "Job not found"}}.dump(), "application/json");
            return;
        }

        res.set_content(job_to_json(*job).dump(), "application/json");
    });

    // 6. Cancel Job (POST /api/v1/jobs/:id/cancel)
    http_server_->Post(R"(/api/v1/jobs/([a-zA-Z0-9_\-\.:]+)/cancel)", [this](const httplib::Request& req, httplib::Response& res) {
        if (!check_auth(req.get_header_value("Authorization"), req.get_header_value("X-API-Key"))) {
            res.status = 401;
            res.set_content(json{{"error", "Unauthorized"}}.dump(), "application/json");
            return;
        }

        std::string job_id_str = req.matches[1];
        JobId id(job_id_str);

        auto existing = store_->get_job(id);
        if (!existing.has_value()) {
            res.status = 404;
            res.set_content(json{{"error", "Job not found"}}.dump(), "application/json");
            return;
        }

        if (existing->is_terminal()) {
            res.status = 409;
            res.set_content(json{{"error", "Job is already terminal and cannot be cancelled."}}.dump(), "application/json");
            return;
        }

        bool engine_cancelled = engine_.cancel_job(id);
        store_->update_status(id, JobStatus::Cancelled, "Cancelled by user via HTTP API");

        json body = {
            {"message", "Cancellation signal sent"},
            {"job_id", job_id_str},
            {"engine_acknowledged", engine_cancelled}
        };
        res.set_content(body.dump(), "application/json");
    });

    // 7. Retry Job (POST /api/v1/jobs/:id/retry)
    http_server_->Post(R"(/api/v1/jobs/([a-zA-Z0-9_\-\.:]+)/retry)", [this](const httplib::Request& req, httplib::Response& res) {
        if (!check_auth(req.get_header_value("Authorization"), req.get_header_value("X-API-Key"))) {
            res.status = 401;
            res.set_content(json{{"error", "Unauthorized"}}.dump(), "application/json");
            return;
        }

        std::string job_id_str = req.matches[1];
        JobId id(job_id_str);

        auto existing = store_->get_job(id);
        if (!existing.has_value()) {
            res.status = 404;
            res.set_content(json{{"error", "Job not found"}}.dump(), "application/json");
            return;
        }

        if (existing->status() != JobStatus::Failed && existing->status() != JobStatus::Cancelled) {
            res.status = 409;
            res.set_content(json{{"error", "Only Failed or Cancelled jobs can be retried."}}.dump(), "application/json");
            return;
        }

        if (engine_.is_job_active(id)) {
            res.status = 409;
            res.set_content(json{{"error", "Cannot retry a job that is currently active or queued in the engine."}}.dump(), "application/json");
            return;
        }

        // Atomically transition SQLite record from Failed/Cancelled to Queued (with 0 attempts)
        if (!store_->reset_job_for_retry(id)) {
            res.status = 409;
            res.set_content(json{{"error", "Only Failed or Cancelled jobs can be retried."}}.dump(), "application/json");
            return;
        }

        Job retry_job = Job::Builder()
            .with_id(existing->id().value())
            .with_type(existing->type())
            .with_payload(existing->payload())
            .with_priority(existing->priority())
            .with_attempt_count(0)
            .with_max_attempts(existing->max_attempts() > 1 ? existing->max_attempts() : 3)
            .with_status(JobStatus::Queued)
            .build();

        bool submitted = engine_.submit(retry_job);
        if (!submitted) {
            store_->update_status(id, JobStatus::Failed, "Engine rejected retry submission (stopping or duplicate)", 0);
            res.status = 503;
            res.set_content(json{{"error", "Execution engine is shutting down or stopped."}}.dump(), "application/json");
            return;
        }

        res.status = 200;
        json body = {
            {"message", "Job successfully re-queued for execution"},
            {"job", job_to_json(retry_job)}
        };
        res.set_content(body.dump(), "application/json");
    });

    // 8. Delete Job (DELETE /api/v1/jobs/:id)
    http_server_->Delete(R"(/api/v1/jobs/([a-zA-Z0-9_\-\.:]+))", [this](const httplib::Request& req, httplib::Response& res) {
        if (!check_auth(req.get_header_value("Authorization"), req.get_header_value("X-API-Key"))) {
            res.status = 401;
            res.set_content(json{{"error", "Unauthorized"}}.dump(), "application/json");
            return;
        }

        std::string job_id_str = req.matches[1];
        JobId id(job_id_str);

        if (engine_.is_job_active(id)) {
            res.status = 409;
            res.set_content(json{{"error", "Cannot delete an active job (Queued or Running). Cancel the job first."}}.dump(), "application/json");
            return;
        }

        bool deleted = store_->delete_job(id, /*only_terminal=*/true);
        if (!deleted) {
            auto check = store_->get_job(id);
            if (check.has_value() && (check->status() == JobStatus::Running || check->status() == JobStatus::Queued)) {
                res.status = 409;
                res.set_content(json{{"error", "Cannot delete an active job (Queued or Running). Cancel the job first."}}.dump(), "application/json");
            } else {
                res.status = 404;
                res.set_content(json{{"error", "Job not found or already deleted"}}.dump(), "application/json");
            }
            return;
        }

        res.status = 200;
        res.set_content(json{{"message", "Job deleted successfully"}, {"id", job_id_str}}.dump(), "application/json");
    });

    // 8. Serve Web Dashboard UI (GET / and GET /dashboard)
    if (config_.enable_dashboard) {
        auto serve_dashboard = [](const httplib::Request&, httplib::Response& res) {
            res.set_content(get_dashboard_html(), "text/html; charset=utf-8");
        };
        http_server_->Get("/", serve_dashboard);
        http_server_->Get("/dashboard", serve_dashboard);
    }
}

void TaskMeshServer::start() {
    running_ = true;
    bound_port_ = config_.port;
    std::cout << "[TaskMesh] HTTP REST API & Web Dashboard starting on " << config_.host << ":" << config_.port << "...\n";
    http_server_->listen(config_.host.c_str(), config_.port);
    running_ = false;
}

void TaskMeshServer::stop() {
    if (http_server_ && http_server_->is_running()) {
        http_server_->stop();
    }
    engine_.shutdown(ShutdownMode::GracefulDrain);
    engine_.wait_until_stopped(2000ms);
    running_ = false;
}

bool TaskMeshServer::is_running() const noexcept {
    return running_;
}

int TaskMeshServer::bound_port() const noexcept {
    return bound_port_;
}

} // namespace taskmesh
