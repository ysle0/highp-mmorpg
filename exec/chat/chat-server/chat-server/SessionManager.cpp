#include "SessionManager.h"

SessionManager::SessionManager(std::shared_ptr<highp::log::Logger> logger)
    : _logger(std::move(logger)) {
}

std::shared_ptr<Session> SessionManager::CreateSession(const std::shared_ptr<highp::net::Client>& client) {
    const uint64_t sessionId = _sessionCounter.fetch_add(1);
    auto session = std::make_shared<Session>(_logger, client, sessionId);

    {
        std::scoped_lock lock{_mtx};
        _sessions[sessionId] = session;
    }

    return session;
}

std::shared_ptr<Session> SessionManager::GetSession(uint64_t sessionId) const {
    std::scoped_lock lock{_mtx};
    const auto it = _sessions.find(sessionId);
    if (it == _sessions.end()) {
        return nullptr;
    }

    return it->second;
}

std::shared_ptr<Session> SessionManager::GetSessionByClient(const std::shared_ptr<highp::net::Client>& client) const {
    std::scoped_lock lock{_mtx};
    for (const auto& [id, s] : _sessions) {
        if (s->IsSameSession(client)) {
            return s;
        }
    }
    return nullptr;
}

bool SessionManager::RemoveSession(uint64_t sessionId) {
    std::scoped_lock lock{_mtx};
    return _sessions.erase(sessionId) > 0;
}

bool SessionManager::RemoveByClient(const std::shared_ptr<highp::net::Client>& client) {
    std::scoped_lock lock{_mtx};
    const auto oldSize = _sessions.size();
    std::erase_if(_sessions, [&client](const auto& entry) {
        return entry.second->IsSameSession(client);
    });
    return oldSize != _sessions.size();
}
