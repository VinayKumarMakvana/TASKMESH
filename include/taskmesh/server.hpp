#pragma once

#include "taskmesh/execution_engine.hpp"
#include "taskmesh/job_executor.hpp"
#include "taskmesh/job_store.hpp"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace httplib {
class Server;
}

namespace taskmesh {

/**
 * @brief Server configuration parameters.
 */
struct ServerConfig {
    std::string host{"0.0.0.0"};
    int port{8080};
    std::string api_key{""};       //!< Optional API key for authentication. If empty, checks TASKMESH_API_KEY env.
    size_t rate_limit_rpm{300};    //!< Max requests per minute per IP.
    std::string db_path{"taskmesh.db"};
    size_t num_workers{4};
    bool enable_dashboard{true};
};

/**
 * @brief Thread-safe rate limiter tracking client requests per minute.
 */
class RateLimiter {
public:
    explicit RateLimiter(size_t max_requests_per_minute = 300);

    [[nodiscard]] bool allow(const std::string& client_ip);

private:
    struct ClientWindow {
        std::chrono::steady_clock::time_point window_start;
        size_t count{0};
    };

    size_t max_rpm_;
    std::mutex mutex_;
    std::unordered_map<std::string, ClientWindow> clients_;
};

/**
 * @brief Production HTTP REST API server and dashboard provider.
 *
 * Exposes REST endpoints for job scheduling, status, metrics, and health,
 * backed by the C++20 ExecutionEngine and persistent SqliteJobStore.
 */
class TaskMeshServer {
public:
    explicit TaskMeshServer(ServerConfig config = {});
    ~TaskMeshServer();

    // Non-copyable, non-movable server instance
    TaskMeshServer(const TaskMeshServer&) = delete;
    TaskMeshServer& operator=(const TaskMeshServer&) = delete;
    TaskMeshServer(TaskMeshServer&&) = delete;
    TaskMeshServer& operator=(TaskMeshServer&&) = delete;

    /**
     * @brief Starts the HTTP server (blocks calling thread until stop() is called).
     */
    void start();

    /**
     * @brief Stops the HTTP server and gracefully shuts down the execution engine.
     */
    void stop();

    [[nodiscard]] bool is_running() const noexcept;
    [[nodiscard]] int bound_port() const noexcept;
    [[nodiscard]] ExecutionEngine& engine() noexcept { return engine_; }
    [[nodiscard]] IJobStore& store() noexcept { return *store_; }

private:
    void setup_routes();
    void register_worker_handlers();
    void recover_and_enqueue_persisted_jobs();

    [[nodiscard]] bool check_auth(const std::string& auth_header, const std::string& api_key_header) const;
    [[nodiscard]] static std::string get_dashboard_html();

    ServerConfig config_;
    std::unique_ptr<IJobStore> store_;
    InMemoryJobExecutor executor_;
    ExecutionEngine engine_;
    RateLimiter rate_limiter_;
    std::unique_ptr<httplib::Server> http_server_;
    std::chrono::steady_clock::time_point start_time_;
    bool running_{false};
    int bound_port_{0};
};

} // namespace taskmesh
