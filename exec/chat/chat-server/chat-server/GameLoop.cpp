#include "GameLoop.h"

#include "SelfHandlerRegistry.h"
#include "scope/Defer.h"

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
        *_dispatcher,
        _logger,
        _roomManager,
        _userManager);
}

GameLoop::~GameLoop() noexcept {
    if (!_hasStopped.load()) {
        Stop();
    }
}

void GameLoop::Connect(const std::shared_ptr<highp::net::Client>& client) {
    if (_hasStopped.load()) {
        _logger->Warn("[GameLoop::Connect] already stopped.");
        return;
    }

    if (_sessionManager == nullptr) {
        return;
    }

    const auto session = _sessionManager->CreateSession(client);
    _logger->Info("[GameLoop::Connect] session #{} connected on socket #{}",
                  session->GetId(), client->socket);
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

    // TODO: 게임 로직 (방/zone 정지)

    // Tick() 의 sleep_until 은 즉시 깨울 수 없어 tickMs 정도 기다릴 수 있음.
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

void GameLoop::Update(std::stop_token st) const {
    _logger->Info("[LogicThread] Update started. ServerTick={}ms", _tickMs.load());
    DEFER([this] {
        // 종료 전 잔여 커맨드 처리
        _dispatcher->Tick();
        _logger->Info("[LogicThread] stopped.");
        });

    while (!st.stop_requested()) {
        // 시작 전 목표 시각을 고정.
        const auto targetingNextTick =
            std::chrono::steady_clock::now() +
            std::chrono::milliseconds(_tickMs.load());

        _dispatcher->Tick();

        // 이미 targetingNextTick 이  sleep 할 필요없이 바로 다음 tick 으로 넘어감.
        const bool alreadyPassedNextTick = targetingNextTick < std::chrono::steady_clock::now();
        if (alreadyPassedNextTick) {
            std::this_thread::yield();
            continue;
        }

        // _dispatcher->Tick() 이후 남은 tick 만큼만 sleep!
        std::this_thread::sleep_until(targetingNextTick);
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
