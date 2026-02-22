#include "pch.h"
#include "io/windows/IocpIoMultiplexer.h"
#include <chrono>

using namespace std::chrono_literals;

namespace highp::net::internal {
    IocpIoMultiplexer::IocpIoMultiplexer(
        std::shared_ptr<log::Logger> logger,
        CompletionHandler handler
    ) : _logger(logger),
        _completionHandler(std::move(handler)) {
    }

    IocpIoMultiplexer::~IocpIoMultiplexer() noexcept { Shutdown(); }

    IocpIoMultiplexer::Res IocpIoMultiplexer::Initialize(int workerThreadCount) {
        _handle = CreateIoCompletionPort(
            INVALID_HANDLE_VALUE,
            nullptr,
            NULL,
            workerThreadCount);
        if (_handle == nullptr) {
            return Res::Err(err::ENetworkError::IocpCreateFailed);
        }

        _isRunning = true;

        for (int i = 0; i < workerThreadCount; ++i) {
            _workerThreads.emplace_back([this](std::stop_token st) { WorkerLoop(st); });
        }

        _logger->Info("IocpIoMultiplexer initialized. worker threads: {}",
                      workerThreadCount);
        return Res::Ok();
    }

    void IocpIoMultiplexer::Shutdown() {
        if (!_isRunning.exchange(false)) {
            return;
        }

        for (auto& worker : _workerThreads) {
            worker.request_stop();
            PostQueuedCompletionStatus(_handle, 0, 0, nullptr);
        }

        _workerThreads.clear();

        if (_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(_handle);
            _handle = INVALID_HANDLE_VALUE;
        }

        _logger->Info("IocpIoMultiplexer shutdown complete.");
    }

    IocpIoMultiplexer::Res IocpIoMultiplexer::PostCompletion(
        DWORD bytes,
        ICompletionTarget* key,
        LPOVERLAPPED overlapped
    ) {
        BOOL result = PostQueuedCompletionStatus(
            _handle,
            bytes,
            key->AsCompletionKey(),
            overlapped);

        if (result == FALSE) {
            return Res::Err(err::ENetworkError::IocpInternalError);
        }

        return Res::Ok();
    }

    void IocpIoMultiplexer::SetCompletionHandler(CompletionHandler handler) {
        _completionHandler = std::move(handler);
    }

    HANDLE IocpIoMultiplexer::GetHandle() const noexcept { return _handle; }

    bool IocpIoMultiplexer::IsRunning() const noexcept { return _isRunning.load(); }

    void IocpIoMultiplexer::WorkerLoop(std::stop_token st) const {
        while (!st.stop_requested() && _isRunning.load()) {
            DWORD bytesTransferred = 0;
            ULONG_PTR completionKey = 0;
            LPOVERLAPPED overlapped = nullptr;

            const BOOL ok = GetQueuedCompletionStatus(
                _handle,
                &bytesTransferred,
                &completionKey,
                &overlapped,
                INFINITE);
            if (completionKey == 0 && overlapped == nullptr) {
                break;
            }

            if (ok == FALSE) {
                const DWORD err = GetLastError();
                if (bool isIoPendingCanceled = err == ERROR_OPERATION_ABORTED) {
                    constexpr auto workerShutdownGracePeriodMs = std::chrono::milliseconds(
                        Const::Io::workerIoPendingCancelGracePeriodMs);

                    _logger->Info(
                        "[IocpIoMultiplexer::WorkerLoop] GetQueuedCompletionStatus canceled. I/O pending canceled by CancelIoEx().. exit WorkerLoop In {}ms",
                        workerShutdownGracePeriodMs);

                    std::this_thread::sleep_for(workerShutdownGracePeriodMs);

                    break;
                }
            }

            CompletionEvent event{
                .target = ICompletionTarget::FromCompletionKey(completionKey),
                .overlapped = overlapped,
                .bytesTransferred = bytesTransferred,
                .success = ok == TRUE,
                .errorCode = ok == FALSE ? GetLastError() : 0,
            };

            if (overlapped != nullptr) {
                const auto* ext = reinterpret_cast<OverlappedBase*>(overlapped);
                event.ioType = ext->ioType;
            }

            if (_completionHandler) {
                _completionHandler(event);
            }
        }
    }
} // namespace highp::net::internal
