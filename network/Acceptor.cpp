#include "pch.h"
#include "Acceptor.h"

namespace highp::network {

Acceptor::Acceptor(
	std::shared_ptr<log::Logger> logger,
	AcceptCallback onAfterAccept
)
	: _logger(std::move(logger))
	, _overlappedPool()
	, _acceptCallback(std::move(onAfterAccept)) {}

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
		return Res::Err(err::ENetworkError::SocketCreateFailed);
	}

	// 0) 원본 C++17 if with initializer 로 2줄 -> 1줄로 줄이기!
	//if (auto res = LoadAcceptExFunctions(); res.HasErr()) {
	//	return res;
	//}

	// 1) bool 연산자 오버로딩 -> .HasErr() 를 줄일 수 있음.
	//if (auto res = LoadAcceptExFunctions(); !res) {
	//	return res;
	//}
	//
	// 만약 return 으로 err propagation 이 필요없으면
	//if (LoadAcceptExFunctions()) {
	//}

	// 2) macro GUARD(expr)
	GUARD(LoadAcceptExFunctions());

	_logger->Info("Acceptor initialized. listen socket associated with IOCP. pre-allocated overlappeds: {}",
		_overlappedPool.GetAvailableCount());
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
		return Res::Err(err::ENetworkError::SocketPostAcceptFailed);
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
		return Res::Err(err::ENetworkError::SocketPostAcceptFailed);
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

Acceptor::Res Acceptor::PostAccepts(int count) {
	for (int i = 0; i < count; ++i) {
		GUARD(PostAccept());
	}
	_logger->Info("Posted {} AcceptEx requests.", count);
	return Res::Ok();
}

Acceptor::Res Acceptor::PostAccept() {
	SocketHandle acceptSocket = CreateAcceptSocket();
	if (acceptSocket == InvalidSocket) {
		//ReleaseOverlapped(overlapped);
		_logger->Error("Failed to create accept socket. error: {}", WSAGetLastError());
		return Res::Err(err::ENetworkError::SocketAcceptFailed);
	}

	OverlappedExt* overlapped = _overlappedPool.Acquire();
	ZeroMemory(&overlapped->overlapped, sizeof(WSAOVERLAPPED));
	overlapped->ioType = EIoType::Accept;
	overlapped->clientSocket = acceptSocket;

	// overlappedExt 추적 시작.
	{
		std::lock_guard lock(_ioPendingOverlappedMtx);
		_ioPendingOverlappeds.insert(overlapped);
	}


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
			_overlappedPool.Release(overlapped);
			_logger->Error("AcceptEx failed. error: {}", err);
			return Res::Err(err::ENetworkError::SocketPostAcceptFailed);
		}
	}

	return Res::Ok();
}

Acceptor::Res Acceptor::OnAcceptComplete(OverlappedExt* overlapped, DWORD bytesTransferred) {
	if (overlapped == nullptr || overlapped->ioType != EIoType::Accept) {
		return Res::Err(err::ENetworkError::SocketAcceptFailed);
	}

	{
		std::lock_guard lock(_ioPendingOverlappedMtx);
		_ioPendingOverlappeds.erase(overlapped);
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
		_overlappedPool.Release(overlapped);
		return Res::Err(err::ENetworkError::SocketAcceptFailed);
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
		AcceptContext ctx{
			.acceptSocket = overlapped->clientSocket,
			.listenSocket = _listenSocket,
			.localAddr = localAddr ? *localAddr : sockaddr_in{},
			.remoteAddr = remoteAddr ? *remoteAddr : sockaddr_in{}
		};

		_acceptCallback(ctx);
	}

	_overlappedPool.Release(overlapped);

	// 3) 매크로 처리 + 후속 실행함수 추가
	GUARD_EFFECT(PostAccept(), [this]() {
		_logger->Error("Failed to re-post AcceptEx after completion.");
	});

	return Res::Ok();
}

void Acceptor::SetAcceptCallback(AcceptCallback callback) {
	_acceptCallback = std::move(callback);
}

void Acceptor::Shutdown() {
	{
		std::lock_guard lock(_ioPendingOverlappedMtx);
		for (OverlappedExt* o : _ioPendingOverlappeds) {
			CancelIoEx(
				reinterpret_cast<HANDLE>(_listenSocket),
				&o->overlapped
			);
		}
	}

	_fnAcceptEx = nullptr;
	_fnGetAcceptExSockAddrs = nullptr;


	_logger->Info("Acceptor shutdown complete.");
}

}
