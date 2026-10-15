#include "taskmesh/server.hpp"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
taskmesh::TaskMeshServer* g_server = nullptr;

void signal_handler(int signum) {
    std::cout << "\n[TaskMesh] Signal " << signum << " received, initiating graceful shutdown...\n";
    if (g_server) {
        g_server->stop();
    }
}
} // namespace

int main(int argc, char* argv[]) {
    taskmesh::ServerConfig config;

    // 1. Environment variable defaults
    const char* env_port = std::getenv("PORT");
    if (env_port) {
        try { config.port = std::stoi(env_port); } catch (...) {}
    }
    const char* env_workers = std::getenv("TASKMESH_WORKERS");
    if (env_workers) {
        try { config.num_workers = std::stoul(env_workers); } catch (...) {}
    }
    const char* env_db = std::getenv("TASKMESH_DB");
    if (env_db) {
        config.db_path = env_db;
    }
    const char* env_key = std::getenv("TASKMESH_API_KEY");
    if (env_key) {
        config.api_key = env_key;
    }

    // 2. Command-line overrides
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            config.port = std::stoi(argv[++i]);
        } else if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];
        } else if (arg == "--workers" && i + 1 < argc) {
            config.num_workers = std::stoul(argv[++i]);
        } else if (arg == "--db" && i + 1 < argc) {
            config.db_path = argv[++i];
        } else if (arg == "--api-key" && i + 1 < argc) {
            config.api_key = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "TaskMesh Server — Production Job Engine Host\n"
                      << "Usage: " << argv[0] << " [options]\n\n"
                      << "Options:\n"
                      << "  --port <port>       HTTP port to listen on (default: 8080 or $PORT)\n"
                      << "  --host <host>       Network interface to bind (default: 0.0.0.0)\n"
                      << "  --workers <count>   Worker thread pool size (default: 4 or $TASKMESH_WORKERS)\n"
                      << "  --db <path>         Path to persistent SQLite database (default: taskmesh.db)\n"
                      << "  --api-key <key>     API key for authenticated requests (default: $TASKMESH_API_KEY)\n"
                      << "  --help, -h          Show this help message\n";
            return 0;
        }
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "==============================================================================\n";
    std::cout << "  TASKMESH — Background Job Processing Service (C++20 Engine)\n";
    std::cout << "==============================================================================\n";
    std::cout << "  Host / Interface : " << config.host << "\n";
    std::cout << "  Port             : " << config.port << "\n";
    std::cout << "  Worker Threads   : " << config.num_workers << "\n";
    std::cout << "  Durable Storage  : " << config.db_path << " (SQLite WAL Mode)\n";
    std::cout << "  Authentication   : " << (config.api_key.empty() ? "Disabled (Public API)" : "Enabled (Bearer/X-API-Key)") << "\n";
    std::cout << "  Web Dashboard    : http://" << (config.host == "0.0.0.0" ? "localhost" : config.host) << ":" << config.port << "/\n";
    std::cout << "  Health Check     : http://" << (config.host == "0.0.0.0" ? "localhost" : config.host) << ":" << config.port << "/health\n";
    std::cout << "==============================================================================\n";

    try {
        taskmesh::TaskMeshServer server(config);
        g_server = &server;
        server.start();
    } catch (const std::exception& ex) {
        std::cerr << "[TaskMesh] Fatal server error: " << ex.what() << "\n";
        return 1;
    }

    std::cout << "[TaskMesh] Server stopped cleanly. Goodbye.\n";
    return 0;
}
