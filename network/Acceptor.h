#pragma once

#include "platform.h"
#include "AcceptContext.h"
#include "OverlappedExt.h"
#include <Logger.hpp>
#include <Result.hpp>
#include <SocketError.h>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

namespace highp::network {

using AcceptCallback = std::function<void(AcceptContext&)>;

class Acceptor final {
public:
	using Res = fn::Result<void, err::ESocketError>;

	explicit Acceptor(std::shared_ptr<log::Logger> logger, int preAllocCount = 10);
	~Acceptor();

	Acceptor(const Acceptor&) = delete;
	Acceptor& operator=(const Acceptor&) = delete;
	Acceptor(Acceptor&&) = delete;
	Acceptor& operator=(Acceptor&&) = delete;

	Res Initialize(SocketHandle listenSocket, HANDLE iocpHandle);
	Res PostAccept();
	Res PostAccepts(int count);
	Res OnAcceptComplete(OverlappedExt* overlapped, DWORD bytesTransferred);

	void SetAcceptCallback(AcceptCallback callback);
	void Shutdown();

private:
	Res LoadAcceptExFunctions();
	SocketHandle CreateAcceptSocket();
	OverlappedExt* AcquireOverlapped();
	void ReleaseOverlapped(OverlappedExt* overlapped);

private:
	std::shared_ptr<log::Logger> _logger;
	SocketHandle _listenSocket = InvalidSocket;
	HANDLE _iocpHandle = INVALID_HANDLE_VALUE;

	LPFN_ACCEPTEX _fnAcceptEx = nullptr;
	LPFN_GETACCEPTEXSOCKADDRS _fnGetAcceptExSockAddrs = nullptr;

	std::vector<std::unique_ptr<OverlappedExt>> _overlappedPool;
	std::queue<OverlappedExt*> _availableOverlappeds;
	std::mutex _poolMutex;

	AcceptCallback _acceptCallback;
	int _preAllocCount;
};

}
