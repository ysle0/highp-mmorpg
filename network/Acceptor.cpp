#include "pch.h"
#include "Acceptor.h"

namespace highp::network {

Acceptor::Acceptor(
	std::shared_ptr<log::Logger> logger,
	int preAllocCount,
	AcceptCallback onAfterAccept)
	: _logger(logger)
	, _overlappedPool(preAllocCount)
	, _acceptCallback(std::move(onAfterAccept))
{
}

Acceptor::~Acceptor() {
	Shutdown();
}

Acceptor::Res Acceptor::Initialize(SocketHandle listenSocket, HANDLE iocpHandle) {
	_listenSocket = listenSocket;
	_iocpHandle = iocpHandle;

	// Listen 소켓을 IOCP에 연결 (AcceptEx 완료 통지를 받기 위해 필수)
	HANDLE result = CreateIoCompletionPort(
		reinterpret_cast<HANDLE>(_listenSocket),
		_iocpHandle,
		0,  // Listen 소켓은 completionKey 불필요
		0);
	if (result == NULL || result != _iocpHandle) {
		_logger->Error("Failed to associate listen socket with IOCP. error: {}", GetLastError());
		return Res::Err(err::ESocketError::CreateSocketFailed);
	}

	if (auto res = LoadAcceptExFunctions(); res.HasErr()) {
		return res;
	}

	_logger->Info("Acceptor initialized. listen socket associated with IOCP. pre-allocated overlappeds: {}", _overlappedPool.AvailableCount());
	return Res::Ok();
}

Acceptor::Res Acceptor::LoadAcceptExFunctions() {
	GUID guidAcceptEx = WSAID_ACCEPTEX;
	GUID guidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;
	DWORD bytes = 0;

	int result = WSAIoctl(
		_listenSocket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guidAcceptEx,
		sizeof(guidAcceptEx),
		&_fnAcceptEx,
		sizeof(_fnAcceptEx),
		&bytes,
		nullptr,
		nullptr);

	if (result == SOCKET_ERROR) {
		_logger->Error("Failed to load AcceptEx function. error: {}", WSAGetLastError());
		return Res::Err(err::ESocketError::PostAcceptFailed);
	}

	result = WSAIoctl(
		_listenSocket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guidGetAcceptExSockAddrs,
		sizeof(guidGetAcceptExSockAddrs),
		&_fnGetAcceptExSockAddrs,
		sizeof(_fnGetAcceptExSockAddrs),
		&bytes,
		nullptr,
		nullptr);

	if (result == SOCKET_ERROR) {
		_logger->Error("Failed to load GetAcceptExSockaddrs function. error: {}", WSAGetLastError());
		return Res::Err(err::ESocketError::PostAcceptFailed);
	}

	_logger->Info("AcceptEx functions loaded successfully.");
	return Res::Ok();
}

SocketHandle Acceptor::CreateAcceptSocket() {
	SocketHandle socket = WSASocketW(
		AF_INET,
		SOCK_STREAM,
		IPPROTO_TCP,
		nullptr,
		0,
		WSA_FLAG_OVERLAPPED);

	return socket;
}

OverlappedExt* Acceptor::AcquireOverlapped() {
	return _overlappedPool.Acquire();
}

void Acceptor::ReleaseOverlapped(OverlappedExt* overlapped) {
	_overlappedPool.Release(overlapped);
}

Acceptor::Res Acceptor::PostAccept() {
	OverlappedExt* overlapped = AcquireOverlapped();

	SocketHandle acceptSocket = CreateAcceptSocket();
	if (acceptSocket == InvalidSocket) {
		ReleaseOverlapped(overlapped);
		_logger->Error("Failed to create accept socket. error: {}", WSAGetLastError());
		return Res::Err(err::ESocketError::CreateSocketFailed);
	}

	ZeroMemory(&overlapped->overlapped, sizeof(WSAOVERLAPPED));
	overlapped->ioType = EIoType::Accept;
	overlapped->clientSocket = acceptSocket;

	DWORD bytesReceived = 0;
	constexpr DWORD addrLen = sizeof(SOCKADDR_IN) + 16;

	// overlapped->overlapped이 첫 번째 멤버이므로
	// OverlappedExt* 를 직접 LPOVERLAPPED로 캐스팅 가능
	BOOL result = _fnAcceptEx(
		_listenSocket,
		acceptSocket,
		overlapped->sendBuffer,
		0,
		addrLen,
		addrLen,
		&bytesReceived,
		reinterpret_cast<LPOVERLAPPED>(overlapped));

	if (result == FALSE) {
		DWORD err = WSAGetLastError();
		if (err != ERROR_IO_PENDING) {
			closesocket(acceptSocket);
			ReleaseOverlapped(overlapped);
			_logger->Error("AcceptEx failed. error: {}", err);
			return Res::Err(err::ESocketError::PostAcceptFailed);
		}
	}

	return Res::Ok();
}

Acceptor::Res Acceptor::PostAccepts(int count) {
	for (int i = 0; i < count; ++i) {
		if (auto res = PostAccept(); res.HasErr()) {
			return res;
		}
	}
	_logger->Info("Posted {} AcceptEx requests.", count);
	return Res::Ok();
}

Acceptor::Res Acceptor::OnAcceptComplete(OverlappedExt* overlapped, DWORD bytesTransferred) {
	if (overlapped == nullptr || overlapped->ioType != EIoType::Accept) {
		return Res::Err(err::ESocketError::AcceptFailed);
	}

	int result = setsockopt(
		overlapped->clientSocket,
		SOL_SOCKET,
		SO_UPDATE_ACCEPT_CONTEXT,
		reinterpret_cast<char*>(&_listenSocket),
		sizeof(_listenSocket));

	if (result == SOCKET_ERROR) {
		_logger->Error("SO_UPDATE_ACCEPT_CONTEXT failed. error: {}", WSAGetLastError());
		closesocket(overlapped->clientSocket);
		ReleaseOverlapped(overlapped);
		return Res::Err(err::ESocketError::AcceptFailed);
	}

	SOCKADDR_IN* localAddr = nullptr;
	SOCKADDR_IN* remoteAddr = nullptr;
	int localAddrLen = 0;
	int remoteAddrLen = 0;
	constexpr DWORD addrLen = sizeof(SOCKADDR_IN) + 16;

	_fnGetAcceptExSockAddrs(
		overlapped->sendBuffer,
		0,
		addrLen,
		addrLen,
		reinterpret_cast<SOCKADDR**>(&localAddr),
		&localAddrLen,
		reinterpret_cast<SOCKADDR**>(&remoteAddr),
		&remoteAddrLen);

	if (_acceptCallback) {
		AcceptContext ctx;
		ctx.acceptSocket = overlapped->clientSocket;
		ctx.listenSocket = _listenSocket;
		if (localAddr) ctx.localAddr = *localAddr;
		if (remoteAddr) ctx.remoteAddr = *remoteAddr;

		_acceptCallback(ctx);
	}

	ReleaseOverlapped(overlapped);

	if (auto res = PostAccept(); res.HasErr()) {
		_logger->Error("Failed to re-post AcceptEx after completion.");
	}

	return Res::Ok();
}

void Acceptor::SetAcceptCallback(AcceptCallback callback) {
	_acceptCallback = std::move(callback);
}

void Acceptor::Shutdown() {
	_overlappedPool.Clear();
	_fnAcceptEx = nullptr;
	_fnGetAcceptExSockAddrs = nullptr;
	_logger->Info("Acceptor shutdown complete.");
}

}
