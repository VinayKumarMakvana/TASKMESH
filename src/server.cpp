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

    register_worker_handlers();
    recover_and_enqueue_persisted_jobs();
    setup_routes();
}

TaskMeshServer::~TaskMeshServer() {
    stop();
}

void TaskMeshServer::register_worker_handlers() {
    // 1. Email task handler
    executor_.register_handler("email", [this](const Job& job, const JobExecutionContext& ctx) {
        for (int i = 0; i < 5; ++i) {
            if (ctx.is_cancellation_requested()) {
                store_->update_status(job.id(), JobStatus::Cancelled, "Cancelled during email dispatch");
                return ExecutionResult::cancelled("Cancelled during email dispatch");
            }
            std::this_thread::sleep_for(15ms);
        }
        store_->update_status(job.id(), JobStatus::Completed);
        return ExecutionResult::ok("Email dispatched successfully");
    });

    // 2. Data processing handler
    executor_.register_handler("data_sync", [this](const Job& job, const JobExecutionContext& ctx) {
        for (int i = 0; i < 8; ++i) {
            if (ctx.is_cancellation_requested()) {
                store_->update_status(job.id(), JobStatus::Cancelled, "Cancelled during data sync");
                return ExecutionResult::cancelled("Cancelled during data sync");
            }
            std::this_thread::sleep_for(15ms);
        }
        store_->update_status(job.id(), JobStatus::Completed);
        return ExecutionResult::ok("Data sync settled");
    });

    // 3. Fallback default handler for any custom job type
    executor_.set_default_handler([this](const Job& job, const JobExecutionContext& ctx) {
        if (ctx.is_cancellation_requested()) {
            store_->update_status(job.id(), JobStatus::Cancelled, "Cancelled before processing");
            return ExecutionResult::cancelled();
        }
        std::this_thread::sleep_for(20ms);
        store_->update_status(job.id(), JobStatus::Completed);
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

    // 7. Delete Job (DELETE /api/v1/jobs/:id)
    http_server_->Delete(R"(/api/v1/jobs/([a-zA-Z0-9_\-\.:]+))", [this](const httplib::Request& req, httplib::Response& res) {
        if (!check_auth(req.get_header_value("Authorization"), req.get_header_value("X-API-Key"))) {
            res.status = 401;
            res.set_content(json{{"error", "Unauthorized"}}.dump(), "application/json");
            return;
        }

        std::string job_id_str = req.matches[1];
        bool deleted = store_->delete_job(JobId(job_id_str));
        if (!deleted) {
            res.status = 404;
            res.set_content(json{{"error", "Job not found or already deleted"}}.dump(), "application/json");
            return;
        }

        res.set_content(json{{"message", "Job deleted successfully"}}.dump(), "application/json");
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

std::string TaskMeshServer::get_dashboard_html() {
    return R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>TaskMesh — Concurrent Priority Job Engine</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500;600&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg-dark: #0a0d14;
      --bg-card: rgba(18, 24, 38, 0.75);
      --bg-card-hover: rgba(26, 34, 52, 0.85);
      --border-color: rgba(255, 255, 255, 0.08);
      --border-focus: #3b82f6;
      --text-main: #f8fafc;
      --text-muted: #94a3b8;
      --primary: #3b82f6;
      --primary-glow: rgba(59, 130, 246, 0.35);
      --success: #10b981;
      --warning: #f59e0b;
      --danger: #ef4444;
      --purple: #8b5cf6;
      --critical: #ec4899;
      --font-sans: 'Inter', -apple-system, sans-serif;
      --font-mono: 'JetBrains Mono', monospace;
    }

    * { box-sizing: border-box; margin: 0; padding: 0; }

    body {
      background-color: var(--bg-dark);
      background-image: 
        radial-gradient(circle at 15% 15%, rgba(59, 130, 246, 0.12) 0%, transparent 45%),
        radial-gradient(circle at 85% 85%, rgba(139, 92, 246, 0.10) 0%, transparent 45%);
      background-attachment: fixed;
      color: var(--text-main);
      font-family: var(--font-sans);
      min-height: 100vh;
      display: flex;
      flex-direction: column;
    }

    header {
      background: rgba(10, 13, 20, 0.8);
      backdrop-filter: blur(16px);
      border-bottom: 1px solid var(--border-color);
      position: sticky;
      top: 0;
      z-index: 50;
      padding: 1rem 2rem;
      display: flex;
      align-items: center;
      justify-content: space-between;
    }

    .brand {
      display: flex;
      align-items: center;
      gap: 0.75rem;
      text-decoration: none;
      color: inherit;
    }

    .brand-icon {
      width: 36px;
      height: 36px;
      background: linear-gradient(135deg, #3b82f6, #8b5cf6);
      border-radius: 8px;
      display: flex;
      align-items: center;
      justify-content: center;
      box-shadow: 0 0 20px var(--primary-glow);
    }

    .brand-icon svg { width: 22px; height: 22px; fill: white; }
    .brand-title { font-size: 1.25rem; font-weight: 700; letter-spacing: -0.02em; }
    .brand-badge {
      font-size: 0.7rem;
      font-weight: 600;
      padding: 0.2rem 0.5rem;
      border-radius: 9999px;
      background: rgba(59, 130, 246, 0.15);
      border: 1px solid rgba(59, 130, 246, 0.3);
      color: #60a5fa;
      letter-spacing: 0.05em;
      text-transform: uppercase;
    }

    .header-actions {
      display: flex;
      align-items: center;
      gap: 1rem;
    }

    .status-indicator {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      font-size: 0.85rem;
      color: var(--text-muted);
      background: rgba(255, 255, 255, 0.03);
      padding: 0.4rem 0.8rem;
      border-radius: 6px;
      border: 1px solid var(--border-color);
    }

    .pulse-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background-color: var(--success);
      box-shadow: 0 0 10px var(--success);
      animation: pulse 2s infinite;
    }

    @keyframes pulse {
      0%, 100% { opacity: 1; transform: scale(1); }
      50% { opacity: 0.5; transform: scale(1.2); }
    }

    .btn {
      display: inline-flex;
      align-items: center;
      gap: 0.5rem;
      padding: 0.5rem 1rem;
      border-radius: 6px;
      font-size: 0.875rem;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.2s ease;
      border: none;
      text-decoration: none;
    }

    .btn-primary {
      background: linear-gradient(135deg, #3b82f6, #2563eb);
      color: white;
      box-shadow: 0 4px 14px var(--primary-glow);
    }
    .btn-primary:hover {
      transform: translateY(-1px);
      box-shadow: 0 6px 20px var(--primary-glow);
    }

    .btn-secondary {
      background: rgba(255, 255, 255, 0.05);
      color: var(--text-main);
      border: 1px solid var(--border-color);
    }
    .btn-secondary:hover {
      background: rgba(255, 255, 255, 0.1);
    }

    main {
      flex: 1;
      max-width: 1380px;
      width: 100%;
      margin: 0 auto;
      padding: 2rem;
      display: flex;
      flex-direction: column;
      gap: 2rem;
    }

    /* Metric Gauges Grid */
    .metrics-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
      gap: 1.25rem;
    }

    .metric-card {
      background: var(--bg-card);
      backdrop-filter: blur(12px);
      border: 1px solid var(--border-color);
      border-radius: 12px;
      padding: 1.25rem;
      transition: transform 0.2s ease, border-color 0.2s ease;
      position: relative;
      overflow: hidden;
    }

    .metric-card:hover {
      transform: translateY(-2px);
      border-color: rgba(255, 255, 255, 0.2);
    }

    .metric-card::before {
      content: '';
      position: absolute;
      top: 0; left: 0; right: 0;
      height: 3px;
    }

    .card-submitted::before { background: var(--primary); }
    .card-queued::before { background: var(--warning); }
    .card-running::before { background: var(--purple); }
    .card-completed::before { background: var(--success); }
    .card-failed::before { background: var(--danger); }
    .card-cancelled::before { background: var(--text-muted); }

    .metric-label {
      font-size: 0.8rem;
      font-weight: 500;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      color: var(--text-muted);
      margin-bottom: 0.5rem;
    }

    .metric-value {
      font-size: 2rem;
      font-weight: 700;
      font-family: var(--font-mono);
      line-height: 1;
    }

    /* Action & Filter Toolbar */
    .toolbar {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      justify-content: space-between;
      gap: 1rem;
      background: var(--bg-card);
      backdrop-filter: blur(12px);
      border: 1px solid var(--border-color);
      padding: 1rem 1.5rem;
      border-radius: 12px;
    }

    .filter-group {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      flex-wrap: wrap;
    }

    .filter-pill {
      background: transparent;
      border: 1px solid var(--border-color);
      color: var(--text-muted);
      padding: 0.35rem 0.85rem;
      border-radius: 9999px;
      font-size: 0.8rem;
      font-weight: 500;
      cursor: pointer;
      transition: all 0.15s ease;
    }

    .filter-pill.active, .filter-pill:hover {
      background: var(--primary);
      color: white;
      border-color: var(--primary);
      box-shadow: 0 0 12px var(--primary-glow);
    }

    /* Jobs Data Table */
    .table-container {
      background: var(--bg-card);
      backdrop-filter: blur(12px);
      border: 1px solid var(--border-color);
      border-radius: 12px;
      overflow-x: auto;
    }

    table {
      width: 100%;
      border-collapse: collapse;
      text-align: left;
      font-size: 0.875rem;
    }

    th {
      background: rgba(255, 255, 255, 0.02);
      color: var(--text-muted);
      font-weight: 600;
      font-size: 0.75rem;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      padding: 1rem 1.25rem;
      border-bottom: 1px solid var(--border-color);
    }

    td {
      padding: 1rem 1.25rem;
      border-bottom: 1px solid rgba(255, 255, 255, 0.04);
      vertical-align: middle;
    }

    tr:hover td {
      background: rgba(255, 255, 255, 0.02);
    }

    .priority-badge {
      display: inline-block;
      font-size: 0.75rem;
      font-weight: 600;
      padding: 0.2rem 0.6rem;
      border-radius: 4px;
      font-family: var(--font-mono);
    }

    .priority-Low { background: rgba(148, 163, 184, 0.15); color: #94a3b8; }
    .priority-Normal { background: rgba(59, 130, 246, 0.15); color: #60a5fa; }
    .priority-High { background: rgba(245, 158, 11, 0.15); color: #fbbf24; }
    .priority-Critical { background: rgba(236, 72, 153, 0.15); color: #f472b6; }

    .status-badge {
      display: inline-flex;
      align-items: center;
      gap: 0.4rem;
      font-size: 0.75rem;
      font-weight: 600;
      padding: 0.25rem 0.65rem;
      border-radius: 9999px;
    }

    .status-Queued { background: rgba(245, 158, 11, 0.15); color: #fbbf24; }
    .status-Running { background: rgba(139, 92, 246, 0.15); color: #c084fc; }
    .status-Completed { background: rgba(16, 185, 129, 0.15); color: #34d399; }
    .status-Failed { background: rgba(239, 68, 68, 0.15); color: #f87171; }
    .status-Cancelled { background: rgba(148, 163, 184, 0.15); color: #94a3b8; }

    .code-text {
      font-family: var(--font-mono);
      font-size: 0.8rem;
      background: rgba(0, 0, 0, 0.25);
      padding: 0.2rem 0.4rem;
      border-radius: 4px;
      max-width: 250px;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
      display: inline-block;
    }

    /* Modal Form */
    .modal-overlay {
      position: fixed;
      top: 0; left: 0; right: 0; bottom: 0;
      background: rgba(0, 0, 0, 0.75);
      backdrop-filter: blur(8px);
      display: none;
      align-items: center;
      justify-content: center;
      z-index: 100;
    }

    .modal-overlay.active { display: flex; }

    .modal-box {
      background: #111726;
      border: 1px solid var(--border-color);
      border-radius: 16px;
      max-width: 520px;
      width: 90%;
      padding: 2rem;
      box-shadow: 0 20px 50px rgba(0, 0, 0, 0.5);
    }

    .modal-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 1.5rem;
    }

    .modal-title { font-size: 1.25rem; font-weight: 700; }
    .modal-close {
      background: transparent;
      border: none;
      color: var(--text-muted);
      cursor: pointer;
      font-size: 1.25rem;
    }

    .form-group {
      margin-bottom: 1.25rem;
      display: flex;
      flex-direction: column;
      gap: 0.4rem;
    }

    label { font-size: 0.8rem; font-weight: 600; color: var(--text-muted); }

    input, select, textarea {
      background: rgba(255, 255, 255, 0.04);
      border: 1px solid var(--border-color);
      border-radius: 6px;
      padding: 0.6rem 0.8rem;
      color: var(--text-main);
      font-family: inherit;
      font-size: 0.875rem;
    }

    input:focus, select:focus, textarea:focus {
      outline: none;
      border-color: var(--primary);
      box-shadow: 0 0 10px var(--primary-glow);
    }

    textarea { resize: vertical; min-height: 80px; font-family: var(--font-mono); }
  </style>
</head>
<body>

  <header>
    <a href="/" class="brand">
      <div class="brand-icon">
        <svg viewBox="0 0 24 24"><path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5"/></svg>
      </div>
      <div>
        <div class="brand-title">TaskMesh <span class="brand-badge">Engine v0.2.0</span></div>
      </div>
    </a>

    <div class="header-actions">
      <div class="status-indicator">
        <span class="pulse-dot"></span>
        <span id="engine-status">Engine Healthy</span>
      </div>
      <button class="btn btn-secondary" onclick="fetchData()">
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21.5 2v6h-6M21.34 15.57a10 10 0 1 1-.57-8.38l5.67-5.67"/></svg>
        Refresh
      </button>
      <button class="btn btn-primary" onclick="openModal()">
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"></line><line x1="5" y1="12" x2="19" y2="12"></line></svg>
        Submit Job
      </button>
    </div>
  </header>

  <main>
    <!-- Metric Gauges -->
    <div class="metrics-grid">
      <div class="metric-card card-submitted">
        <div class="metric-label">Submitted</div>
        <div class="metric-value" id="val-submitted">0</div>
      </div>
      <div class="metric-card card-queued">
        <div class="metric-label">Queued</div>
        <div class="metric-value" id="val-queued">0</div>
      </div>
      <div class="metric-card card-running">
        <div class="metric-label">Running</div>
        <div class="metric-value" id="val-running">0</div>
      </div>
      <div class="metric-card card-completed">
        <div class="metric-label">Completed</div>
        <div class="metric-value" id="val-completed">0</div>
      </div>
      <div class="metric-card card-failed">
        <div class="metric-label">Failed</div>
        <div class="metric-value" id="val-failed">0</div>
      </div>
      <div class="metric-card card-cancelled">
        <div class="metric-label">Cancelled</div>
        <div class="metric-value" id="val-cancelled">0</div>
      </div>
    </div>

    <!-- Toolbar & Filters -->
    <div class="toolbar">
      <div class="filter-group">
        <button class="filter-pill active" onclick="setFilter(null, this)">All Jobs</button>
        <button class="filter-pill" onclick="setFilter('Queued', this)">Queued</button>
        <button class="filter-pill" onclick="setFilter('Running', this)">Running</button>
        <button class="filter-pill" onclick="setFilter('Completed', this)">Completed</button>
        <button class="filter-pill" onclick="setFilter('Failed', this)">Failed</button>
        <button class="filter-pill" onclick="setFilter('Cancelled', this)">Cancelled</button>
      </div>

      <div style="display:flex; align-items:center; gap:0.75rem;">
        <span style="font-size:0.8rem; color:var(--text-muted);">Auto-refresh (2s):</span>
        <input type="checkbox" id="auto-refresh-toggle" checked>
      </div>
    </div>

    <!-- Data Table -->
    <div class="table-container">
      <table>
        <thead>
          <tr>
            <th>Priority</th>
            <th>Job ID</th>
            <th>Type</th>
            <th>Payload</th>
            <th>Status</th>
            <th>Attempts</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody id="jobs-table-body">
          <tr>
            <td colspan="7" style="text-align:center; color:var(--text-muted); padding:3rem;">Loading jobs from TaskMesh engine...</td>
          </tr>
        </tbody>
      </table>
    </div>
  </main>

  <!-- Submit Job Modal -->
  <div class="modal-overlay" id="submit-modal">
    <div class="modal-box">
      <div class="modal-header">
        <div class="modal-title">Schedule New Job</div>
        <button class="modal-close" onclick="closeModal()">&times;</button>
      </div>
      <form id="job-form" onsubmit="submitJob(event)">
        <div class="form-group">
          <label>Job Identifier (Optional)</label>
          <input type="text" id="form-job-id" placeholder="e.g. email-dispatch-42 (auto-generated if empty)">
        </div>
        <div class="form-group">
          <label>Job Type *</label>
          <select id="form-job-type" required>
            <option value="email">email (Simulated Email Dispatch)</option>
            <option value="data_sync">data_sync (Database Replication)</option>
            <option value="compute">compute (Long-Running Computation)</option>
            <option value="custom">custom (Generic Task)</option>
          </select>
        </div>
        <div class="form-group">
          <label>Scheduling Priority *</label>
          <select id="form-priority" required>
            <option value="50">Normal (50)</option>
            <option value="100">High (100)</option>
            <option value="200">Critical (200)</option>
            <option value="10">Low (10)</option>
          </select>
        </div>
        <div class="form-group">
          <label>Max Retry Attempts</label>
          <input type="number" id="form-max-attempts" value="3" min="1" max="10">
        </div>
        <div class="form-group">
          <label>Payload (JSON or Text)</label>
          <textarea id="form-payload">{"recipient": "user@example.com", "action": "deliver"}</textarea>
        </div>
        <div style="display:flex; justify-content:flex-end; gap:0.75rem; margin-top:1.5rem;">
          <button type="button" class="btn btn-secondary" onclick="closeModal()">Cancel</button>
          <button type="submit" class="btn btn-primary">Schedule Job</button>
        </div>
      </form>
    </div>
  </div>

  <script>
    let currentFilter = null;

    function openModal() { document.getElementById('submit-modal').classList.add('active'); }
    function closeModal() { document.getElementById('submit-modal').classList.remove('active'); }

    function setFilter(status, el) {
      currentFilter = status;
      document.querySelectorAll('.filter-pill').forEach(b => b.classList.remove('active'));
      el.classList.add('active');
      fetchData();
    }

    async function fetchData() {
      try {
        // Fetch Metrics
        const mRes = await fetch('/api/v1/metrics');
        if (mRes.ok) {
          const m = await mRes.json();
          document.getElementById('val-submitted').innerText = m.submitted;
          document.getElementById('val-queued').innerText = m.queued;
          document.getElementById('val-running').innerText = m.running;
          document.getElementById('val-completed').innerText = m.completed;
          document.getElementById('val-failed').innerText = m.failed;
          document.getElementById('val-cancelled').innerText = m.cancelled;
        }

        // Fetch Jobs
        let url = '/api/v1/jobs?limit=50';
        if (currentFilter) url += '&status=' + encodeURIComponent(currentFilter);
        const jRes = await fetch(url);
        if (jRes.ok) {
          const data = await jRes.json();
          renderJobs(data.jobs || []);
        }
      } catch (err) {
        console.error('Error fetching TaskMesh state:', err);
      }
    }

    function renderJobs(jobs) {
      const tbody = document.getElementById('jobs-table-body');
      if (jobs.length === 0) {
        tbody.innerHTML = '<tr><td colspan="7" style="text-align:center; color:var(--text-muted); padding:3rem;">No jobs match the current view.</td></tr>';
        return;
      }

      tbody.innerHTML = jobs.map(j => {
        const canCancel = (j.status === 'Queued' || j.status === 'Running');
        const prioName = j.priority_name ? j.priority_name.split('(')[0] : 'Normal';
        return `
          <tr>
            <td><span class="priority-badge priority-${prioName}">${j.priority_name}</span></td>
            <td><strong>${escapeHtml(j.id)}</strong></td>
            <td>${escapeHtml(j.type)}</td>
            <td><span class="code-text" title="${escapeHtml(j.payload)}">${escapeHtml(j.payload)}</span></td>
            <td><span class="status-badge status-${j.status}">${j.status}</span></td>
            <td>${j.attempt_count} / ${j.max_attempts}</td>
            <td>
              ${canCancel ? `<button class="btn btn-secondary" style="padding:0.25rem 0.6rem; font-size:0.75rem;" onclick="cancelJob('${escapeHtml(j.id)}')">Cancel</button>` : `<span style="color:var(--text-muted); font-size:0.75rem;">—</span>`}
            </td>
          </tr>
        `;
      }).join('');
    }

    async function submitJob(e) {
      e.preventDefault();
      const id = document.getElementById('form-job-id').value.trim();
      const type = document.getElementById('form-job-type').value;
      const priority = parseInt(document.getElementById('form-priority').value, 10);
      const max_attempts = parseInt(document.getElementById('form-max-attempts').value, 10);
      const payload = document.getElementById('form-payload').value.trim();

      const body = { type, priority, max_attempts, payload };
      if (id) body.id = id;

      try {
        const res = await fetch('/api/v1/jobs', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(body)
        });
        if (res.ok) {
          closeModal();
          fetchData();
        } else {
          const err = await res.json();
          alert('Submission error: ' + (err.error || 'Failed'));
        }
      } catch (err) {
        alert('Request failed: ' + err.message);
      }
    }

    async function cancelJob(id) {
      if (!confirm(`Request cooperative cancellation for job '${id}'?`)) return;
      try {
        const res = await fetch(`/api/v1/jobs/${encodeURIComponent(id)}/cancel`, { method: 'POST' });
        if (res.ok) {
          fetchData();
        } else {
          const err = await res.json();
          alert('Cancel error: ' + (err.error || 'Failed'));
        }
      } catch (err) {
        alert('Request failed: ' + err.message);
      }
    }

    function escapeHtml(str) {
      if (!str) return '';
      return String(str).replace(/[&<>"']/g, s => ({ '&':'&amp;', '<':'&lt;', '>':'&gt;', '"':'&quot;', "'":'&#39;' })[s]);
    }

    // Auto-refresh interval
    setInterval(() => {
      if (document.getElementById('auto-refresh-toggle').checked) {
        fetchData();
      }
    }, 2000);

    fetchData();
  </script>
</body>
</html>)rawhtml";
}

} // namespace taskmesh
