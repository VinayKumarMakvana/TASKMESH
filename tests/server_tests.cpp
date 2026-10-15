#include "taskmesh/server.hpp"

#undef CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib/httplib.h"
#include "nlohmann/json.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>

using json = nlohmann::json;
using namespace taskmesh;
using namespace std::chrono_literals;

class TaskMeshServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_port_ = 18080;
        test_db_ = "test_server_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".db";

        ServerConfig config;
        config.host = "127.0.0.1";
        config.port = test_port_;
        config.db_path = test_db_;
        config.api_key = "test-secret-token";
        config.num_workers = 2;

        server_ = std::make_unique<TaskMeshServer>(config);

        server_thread_ = std::jthread([this]() {
            server_->start();
        });

        // Wait for server to bind
        std::this_thread::sleep_for(150ms);
        client_ = std::make_unique<httplib::Client>("127.0.0.1", test_port_);
        client_->set_connection_timeout(2, 0);
        client_->set_read_timeout(3, 0);
    }

    void TearDown() override {
        if (server_) {
            server_->stop();
        }
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        std::error_code ec;
        std::filesystem::remove(test_db_, ec);
        std::filesystem::remove(test_db_ + "-wal", ec);
        std::filesystem::remove(test_db_ + "-shm", ec);
    }

    int test_port_{18080};
    std::string test_db_;
    std::unique_ptr<TaskMeshServer> server_;
    std::jthread server_thread_;
    std::unique_ptr<httplib::Client> client_;
};

TEST_F(TaskMeshServerTest, HealthEndpointReturns200AndJson) {
    auto res = client_->Get("/health");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);

    auto j = json::parse(res->body);
    EXPECT_EQ(j["status"], "healthy");
    EXPECT_EQ(j["service"], "TaskMesh Engine");
}

TEST_F(TaskMeshServerTest, ReadyEndpointReturns200AndDatabaseConnected) {
    auto res = client_->Get("/ready");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);

    auto j = json::parse(res->body);
    EXPECT_EQ(j["status"], "ready");
    EXPECT_EQ(j["database"], "connected");
}

TEST_F(TaskMeshServerTest, MetricsEndpointReturnsAccurateSnapshot) {
    auto res = client_->Get("/api/v1/metrics");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);

    auto j = json::parse(res->body);
    EXPECT_TRUE(j["invariant_valid"].get<bool>());
    EXPECT_EQ(j["queued"].get<uint64_t>(), 0);
}

TEST_F(TaskMeshServerTest, AuthenticationRejectsMissingOrInvalidToken) {
    // 1. Missing Token -> 401
    json job_req = {{"type", "email"}, {"payload", "test"}};
    auto res = client_->Post("/api/v1/jobs", job_req.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 401);

    // 2. Invalid Token -> 401
    httplib::Headers bad_headers = {{"Authorization", "Bearer invalid-key"}};
    auto res_bad = client_->Post("/api/v1/jobs", bad_headers, job_req.dump(), "application/json");
    ASSERT_TRUE(res_bad);
    EXPECT_EQ(res_bad->status, 401);

    // 3. Valid Token -> 201
    httplib::Headers valid_headers = {{"Authorization", "Bearer test-secret-token"}};
    auto res_good = client_->Post("/api/v1/jobs", valid_headers, job_req.dump(), "application/json");
    ASSERT_TRUE(res_good);
    EXPECT_EQ(res_good->status, 201);
}

TEST_F(TaskMeshServerTest, ValidationRejectsInvalidJobRequests) {
    httplib::Headers headers = {{"X-API-Key", "test-secret-token"}};

    // Missing type
    json bad_1 = {{"payload", "{}"}};
    auto res_1 = client_->Post("/api/v1/jobs", headers, bad_1.dump(), "application/json");
    ASSERT_TRUE(res_1);
    EXPECT_EQ(res_1->status, 400);

    // Invalid Job ID characters
    json bad_2 = {{"id", "bad/id#1"}, {"type", "email"}};
    auto res_2 = client_->Post("/api/v1/jobs", headers, bad_2.dump(), "application/json");
    ASSERT_TRUE(res_2);
    EXPECT_EQ(res_2->status, 400);
}

TEST_F(TaskMeshServerTest, SubmitRetrieveAndCancelJobLifecycle) {
    httplib::Headers headers = {{"Authorization", "Bearer test-secret-token"}};

    // 1. Submit Job
    json job_req = {
        {"id", "test-api-job-1"},
        {"type", "data_sync"},
        {"payload", "{\"sync\":true}"},
        {"priority", "High"},
        {"max_attempts", 3}
    };

    auto post_res = client_->Post("/api/v1/jobs", headers, job_req.dump(), "application/json");
    ASSERT_TRUE(post_res);
    EXPECT_EQ(post_res->status, 201);

    // 2. Retrieve Job by ID
    auto get_res = client_->Get("/api/v1/jobs/test-api-job-1");
    ASSERT_TRUE(get_res);
    EXPECT_EQ(get_res->status, 200);

    auto j = json::parse(get_res->body);
    EXPECT_EQ(j["id"], "test-api-job-1");
    EXPECT_EQ(j["type"], "data_sync");
    EXPECT_EQ(j["priority_name"], "High(100)");

    // 3. Cancel Job
    auto cancel_res = client_->Post("/api/v1/jobs/test-api-job-1/cancel", headers, "", "text/plain");
    ASSERT_TRUE(cancel_res);
    EXPECT_TRUE(cancel_res->status == 200 || cancel_res->status == 409); // 200 if cancelled, 409 if already completed
}

TEST_F(TaskMeshServerTest, DashboardHtmlServedAtRoot) {
    auto res = client_->Get("/");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    EXPECT_NE(res->body.find("TaskMesh"), std::string::npos);
    EXPECT_NE(res->body.find("<!DOCTYPE html>"), std::string::npos);
}
