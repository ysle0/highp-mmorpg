#include "pch.h"
#include "IoCompletionPort.h"
#include "OverlappedExt.h"

namespace highp::network {

IoCompletionPort::IoCompletionPort(
	std::shared_ptr<log::Logger> logger,
	CompletionHandler handler)
	: _logger(std::move(logger))
	, _completionHandler(std::move(handler))
{
}

IoCompletionPort::~IoCompletionPort() noexcept {
	Shutdown();
}

IoCompletionPort::Res IoCompletionPort::Initialize(int workerThreadCount) {
	_handle = CreateIoCompletionPort(
		INVALID_HANDLE_VALUE,
		nullptr,
		NULL,
		workerThreadCount);

	if (_handle == NULL) {
		return Res::Err(err::EIocpError::CreateIocpFailed);
	}

	_isRunning = true;

	for (int i = 0; i < workerThreadCount; ++i) {
		_workerThreads.emplace_back([this](std::stop_token st) {
			WorkerLoop(st);
		});
	}

	_logger->Info("IoCompletionPort initialized. worker threads: {}", workerThreadCount);
	return Res::Ok();
}

void IoCompletionPort::Shutdown() {
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

	_logger->Info("IoCompletionPort shutdown complete.");
}

IoCompletionPort::Res IoCompletionPort::AssociateSocket(SocketHandle socket, void* completionKey) {
	HANDLE result = CreateIoCompletionPort(
		reinterpret_cast<HANDLE>(socket),
		_handle,
		reinterpret_cast<ULONG_PTR>(completionKey),
		0);

	if (result == NULL || result != _handle) {
		return Res::Err(err::EIocpError::ConnectIocpFailed);
	}

	return Res::Ok();
}

IoCompletionPort::Res IoCompletionPort::PostCompletion(DWORD bytes, void* key, LPOVERLAPPED overlapped) {
	BOOL result = PostQueuedCompletionStatus(
		_handle,
		bytes,
		reinterpret_cast<ULONG_PTR>(key),
		overlapped);

	if (result == FALSE) {
		return Res::Err(err::EIocpError::IocpInternalError);
	}

	return Res::Ok();
}

void IoCompletionPort::SetCompletionHandler(CompletionHandler handler) {
	_completionHandler = std::move(handler);
}

void IoCompletionPort::WorkerLoop(std::stop_token st) {
	while (!st.stop_requested() && _isRunning.load()) {
		DWORD bytesTransferred = 0;
		ULONG_PTR completionKey = 0;
		LPOVERLAPPED overlapped = nullptr;

		BOOL ok = GetQueuedCompletionStatus(
			_handle,
			&bytesTransferred,
			&completionKey,
			&overlapped,
			INFINITE);

		if (completionKey == 0 && overlapped == nullptr) {
			break;
		}

		CompletionEvent event{
			.completionKey = reinterpret_cast<void*>(completionKey),
			.overlapped = overlapped,
			.bytesTransferred = bytesTransferred,
			.success = (ok == TRUE),
			.errorCode = (ok == FALSE) ? GetLastError() : 0,
		};

		if (overlapped != nullptr) {
			auto* ext = reinterpret_cast<OverlappedExt*>(overlapped);
			event.ioType = ext->ioType;
		}

		if (_completionHandler) {
			_completionHandler(event);
		}
	}
}

}
