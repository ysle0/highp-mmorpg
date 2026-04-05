#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "logger/Logger.hpp"
#include "Session.h"

class SessionManager final {
public:
    explicit SessionManager(std::shared_ptr<highp::log::Logger> logger);

public:
    [[nodiscard]] std::shared_ptr<Session> CreateSession(const std::shared_ptr<highp::net::Client>& client);

    [[nodiscard]] std::shared_ptr<Session> GetSession(uint64_t sessionId) const;
    [[nodiscard]] std::shared_ptr<Session> GetSessionByClient(const std::shared_ptr<highp::net::Client>& client) const;

    bool RemoveSession(uint64_t sessionId);
    bool RemoveByClient(const std::shared_ptr<highp::net::Client>& client);

private:
    std::shared_ptr<highp::log::Logger> _logger;
    mutable std::mutex _mtx;
    std::atomic<uint64_t> _sessionCounter{0};
    std::unordered_map<uint64_t, std::shared_ptr<Session>> _sessions;
};
