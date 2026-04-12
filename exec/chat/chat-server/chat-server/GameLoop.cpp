#include "GameLoop.h"

#include "SelfHandlerRegistry.h"
#include "scope/Defer.h"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

GameLoop::GameLoop(
    std::shared_ptr<highp::log::Logger> logger,
    std::unique_ptr<highp::net::PacketDispatcher> packetDispatcher,
    std::shared_ptr<RoomManager> roomManager,
    std::shared_ptr<SessionManager> sessionManager,
    std::shared_ptr<UserManager> userManager,
    const highp::net::NetworkCfg& networkConfig
) : _logger(std::move(logger)),
    _dispatcher(std::move(packetDispatcher)),
    _roomManager(std::move(roomManager)),
    _sessionManager(std::move(sessionManager)),
    _userManager(std::move(userManager)) {
    _tickMs.store(networkConfig.server.tickMs);
    SelfHandlerRegistry::Instance().RegisterAll(
        _dispatcher,
        _logger,
        _roomManager,
        _userManager);
}

GameLoop::~GameLoop() noexcept {
    if (!_hasStopped.load()) {
        Stop();
    }
}

void GameLoop::UseCallbacks(EventCallbacks callbacks) noexcept {
    _callbacks = std::move(callbacks);
    _dispatcher->UseCallbacks(_callbacks.dispatcher);
}

void GameLoop::Connect(const std::shared_ptr<highp::net::Client>& client) {
    if (_hasStopped.load()) {
        _logger->Warn("[GameLoop::Connect] already stopped.");
        return;
    }

    if (_sessionManager == nullptr) {
        return;
    }

    const std::shared_ptr<Session> session = _sessionManager->CreateSession(client);
    _logger->Info("[GameLoop::Connect] session #{} connected on socket #{}",
                  session->GetId(),
                  client->socket);
}

void GameLoop::Start() {
    _logger->Debug("[GameLoop] starting...");

    _logicThread.Exec([this](std::stop_token st) {
        Update(std::move(st));
    });
}

void GameLoop::Stop() {
    if (_hasStopped.load()) {
        _logger->Warn("[GameLoop::Stop] already stopped.");
        return;
    }

    _logger->Debug("[GameLoop::Stop] stopping...");
    _hasStopped.store(true);

    _logicThread.Exit();
}

void GameLoop::Receive(
    const std::shared_ptr<highp::net::Client>& client,
    std::span<const char> data
) const {
    if (_hasStopped.load()) {
        _logger->Warn("[GameLoop::Receive] already stopped.");
        return;
    }

    _dispatcher->Receive(client, data);
}

void GameLoop::Update(std::stop_token st) {
    _logger->Info("[LogicThread] Update started. ServerTick={}ms", _tickMs.load());

    if (_callbacks.onLogicThreadStarted) {
        _callbacks.onLogicThreadStarted(GetCurrentThreadId());
    }

    highp::scope::Defer defer([this] {
        _dispatcher->Tick();
        _logger->Info("[LogicThread] stopped.");
    });

    while (!st.stop_requested()) {
        const auto tickStart = std::chrono::steady_clock::now();
        const auto targetNextTick = tickStart + std::chrono::milliseconds(_tickMs.load());

        _dispatcher->Tick();

        const auto tickEnd = std::chrono::steady_clock::now();
        if (_callbacks.onTickDuration) {
            _callbacks.onTickDuration(tickEnd - tickStart);
        }
        if (_callbacks.onTickLag) {
            _callbacks.onTickLag(
                tickEnd > targetNextTick
                    ? std::chrono::duration_cast<std::chrono::nanoseconds>(tickEnd - targetNextTick)
                    : 0ns);
        }

        if (targetNextTick < tickEnd) {
            std::this_thread::yield();
            continue;
        }

        std::this_thread::sleep_until(targetNextTick);
    }
}

void GameLoop::Disconnect(
    const std::shared_ptr<highp::net::Client>& client
) {
    if (_roomManager != nullptr) {
        _roomManager->KickDisconnected(client);
    }

    if (_sessionManager != nullptr) {
        _sessionManager->RemoveByClient(client);
    }
}
