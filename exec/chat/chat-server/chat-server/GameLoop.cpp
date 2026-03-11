#include "GameLoop.h"

#include "SelfHandlerRegistry.h"
#include "scope/Defer.h"

GameLoop::GameLoop(
    std::shared_ptr<highp::log::Logger> logger,
    highp::net::NetworkCfg networkConfig
) : _logger(std::move(logger)),
    _dispatcher(_logger) {
    _tickMs.store(networkConfig.server.tickMs);
    SelfHandlerRegistry::Instance().RegisterAll(_dispatcher, _logger);
}

GameLoop::~GameLoop() noexcept {
    if (!_hasStopped.load()) {
        Stop();
    }
}

void GameLoop::Start() {
    _logger->Debug("[GameLoop] starting...");

    _logicThread = highp::thread::LogicThread::Exec([this](std::stop_token st) {
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

    if (_logicThread.joinable()) {
        _logicThread.request_stop();
        // Tick() 의 sleep_until 은 즉시 깨울 수 없어 tickMs 정도 기다릴 수 있음.
        _logicThread.join();
    }
}

void GameLoop::Receive(std::shared_ptr<highp::net::Client> client, std::span<const char> data) {
    if (_hasStopped.load()) {
        _logger->Warn("[GameLoop::Receive] already stopped.");
        return;
    }

    _dispatcher.Receive(std::move(client), data);
}

void GameLoop::Update(std::stop_token st) {
    _logger->Info("[LogicThread] Update started. ServerTick={}ms", _tickMs.load());
    highp::scope::Defer defer([this] {
        // 종료 전 잔여 커맨드 처리
        _dispatcher.Tick();
        _logger->Info("[LogicThread] stopped.");
    });

    while (!st.stop_requested()) {
        const auto nextTick =
            std::chrono::steady_clock::now() +
            std::chrono::milliseconds(_tickMs.load());

        if (nextTick < std::chrono::steady_clock::now()) {
            std::this_thread::yield();
            continue;
        }

        _dispatcher.Tick();
        std::this_thread::sleep_until(nextTick);
    }
}
