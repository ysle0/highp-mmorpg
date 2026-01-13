#pragma once

#include "platform.h"
#include "EIoType.h"
#include <IocpError.h>
#include <Logger.hpp>
#include <Result.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace highp::network {

struct CompletionEvent {
	EIoType ioType;
	void* completionKey;
	LPOVERLAPPED overlapped;
	DWORD bytesTransferred;
	bool success;
	DWORD errorCode;
};

using CompletionHandler = std::function<void(const CompletionEvent&)>;

class IoCompletionPort final {
public:
	using Res = fn::Result<void, err::EIocpError>;

	explicit IoCompletionPort(std::shared_ptr<log::Logger> logger);
	~IoCompletionPort();

	IoCompletionPort(const IoCompletionPort&) = delete;
	IoCompletionPort& operator=(const IoCompletionPort&) = delete;
	IoCompletionPort(IoCompletionPort&&) = delete;
	IoCompletionPort& operator=(IoCompletionPort&&) = delete;

	Res Initialize(int workerThreadCount);
	void Shutdown();

	Res AssociateSocket(SocketHandle socket, void* completionKey);
	Res PostCompletion(DWORD bytes, void* key, LPOVERLAPPED overlapped);

	void SetCompletionHandler(CompletionHandler handler);

	HANDLE GetHandle() const noexcept { return _handle; }
	bool IsRunning() const noexcept { return _isRunning.load(); }

private:
	void WorkerLoop(std::stop_token st);

private:
	std::shared_ptr<log::Logger> _logger;
	HANDLE _handle = INVALID_HANDLE_VALUE;
	std::vector<std::jthread> _workerThreads;
	std::atomic<bool> _isRunning{ false };
	CompletionHandler _completionHandler;
};

}
