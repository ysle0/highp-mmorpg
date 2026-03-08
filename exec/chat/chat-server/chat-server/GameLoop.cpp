#include "GameLoop.h"

#include "scope/Defer.h"

GameLoop::GameLoop(
    std::shared_ptr<log::Logger> logger,
    net::NetworkCfg networkConfig
) : _logger(std::move(logger)),
    _dispatcher(logger) {
    _tickMs.store(networkConfig.server.tickMs);
}

GameLoop::~GameLoop() noexcept {
    if (!_hasStopped.load()) {
        Stop();
    }
}

void GameLoop::Start() {
    _logger->Debug("[GameLoop] starting...");

    _logicThread = thread::LogicThread::Exec([this](std::stop_token st) {
        Update(std::move(st));
    });
}

void GameLoop::Stop() {
    _logger->Debug("[GameLoop] stopping...");

    // TODO: 게임 로직 (방/zone 정지)
    if (_logicThread.joinable()) {
        _logicThread.request_stop();
        // Tick() 의 sleep_until 은 즉시 깨울 수 없어 tickMs 정도 기다릴 수 있음.
        _logicThread.join();
    }
}

void GameLoop::Receive(
    std::shared_ptr<net::Client> shared,
    std::span<const char> span
) {
    if (_hasStopped.load()) {
        return;
    }

    _dispatcher.Receive(shared, span);
}

void GameLoop::Update(std::stop_token st) {
    _logger->Info("[LogicThread] Update started. ServerTick={}ms", _tickMs.load());
    scope::Defer defer([this] {
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
