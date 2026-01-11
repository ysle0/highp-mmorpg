#include <algorithm>
#include <atomic>
#include <IocpError.h>
#include <iterator>
#include <Logger.hpp>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "Client.h"
#include "Config.h"
#include "Const.h"
#include "EchoServer.h"
#include "EIoType.h"
#include "IoContext.h"

namespace highp::echo_srv {
using namespace highp::err;
using namespace highp::log;

EchoServer::~EchoServer() {
	WSACleanup();
}

EchoServer::EchoServer(std::shared_ptr<Logger> loggerImpl)
	: _logger(std::move(loggerImpl))
	, _config(RuntimeCfg::WithDefaults()) {}

EchoServer::EchoServer(std::shared_ptr<Logger> loggerImpl, RuntimeCfg config)
	: _logger(std::move(loggerImpl))
	, _config(std::move(config)) {}

IocpResult EchoServer::Start() {
	// 1. WSAStartup
	WSADATA data;
	if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
		_logger->Error("{}: {}",
			EIocpErrorHelper::ToString(EIocpError::WsaStartupFailed),
			WSAGetLastError());
		return IocpResult::Err(EIocpError::WsaStartupFailed);
	}

	// 2. Create TCP socket
	_listenerSocket = WSASocket(
		AF_INET,
		SOCK_STREAM,
		IPPROTO_TCP,
		nullptr,
		NULL,
		WSA_FLAG_OVERLAPPED);
	if (_listenerSocket == INVALID_SOCKET) {
		_logger->Error("{}: {}",
			EIocpErrorHelper::ToString(EIocpError::CreateSocketFailed),
			WSAGetLastError());
		WSACleanup();
		return IocpResult::Err(EIocpError::CreateSocketFailed);
	}

	// 3. Bind
	SOCKADDR_IN addr{
		.sin_family = AF_INET,
		.sin_port = htons(static_cast<u_short>(_config.port)),
	};
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(_listenerSocket, (SOCKADDR*)&addr, sizeof(SOCKADDR_IN)) != 0) {
		_logger->Error("{}: {}",
			EIocpErrorHelper::ToString(EIocpError::BindFailed),
			WSAGetLastError());
		closesocket(_listenerSocket);
		WSACleanup();
		return IocpResult::Err(EIocpError::BindFailed);
	}

	// 4. Listen
	if (listen(_listenerSocket, _config.backlog) != 0) {
		_logger->Error("{}: {}",
			EIocpErrorHelper::ToString(EIocpError::ListenFailed),
			WSAGetLastError());
		closesocket(_listenerSocket);
		WSACleanup();
		return IocpResult::Err(EIocpError::ListenFailed);
	}

	// 5. Create a new IOCP handle
	_iocpHandle = CreateIoCompletionPort(
		INVALID_HANDLE_VALUE,
		nullptr,
		NULL,
		_config.workerThreadCount);
	if (_iocpHandle == NULL) {
		_logger->Error("{}: {}",
			EIocpErrorHelper::ToString(EIocpError::CreateIocpFailed),
			WSAGetLastError());
		closesocket(_listenerSocket);
		WSACleanup();
		return IocpResult::Err(EIocpError::CreateIocpFailed);
	}

	// 6. Populate clients
	for (auto i = 0; i < _config.maxClients; i++) {
		_clients.emplace_back(std::make_shared<Client>());
	}

	// 7. Create worker threads
	for (auto i = 0; i < _config.workerThreadCount; i++) {
		_workerThreads.emplace_back([this](std::stop_token st) {
			WorkerLoop(st);
		});
	}
	_logger->Info("worker threads created. count: {}", _config.workerThreadCount);

	// 8. Create accepter thread
	_accepterThread = std::jthread([this](std::stop_token st) {
		AccepterLoop(st);
	});
	_logger->Info("accepter thread created.");

	_logger->Info("server started on port {}.", _config.port);
	return IocpResult::Ok;
}

void EchoServer::Stop() {
	// 1. Worker 스레드들에게 종료 신호 전송
	for (auto& worker : _workerThreads) {
		worker.request_stop();
	}
	// GQCS에서 블로킹 중인 워커들을 깨우기 위해 더미 패킷 전송
	for (size_t i = 0; i < _workerThreads.size(); i++) {
		PostQueuedCompletionStatus(_iocpHandle, 0, 0, nullptr);
	}
	// jthread 소멸자에서 자동 join
	_workerThreads.clear();

	// 2. Accepter 스레드 종료
	// accept()가 블로킹 중이므로 소켓을 먼저 닫아서 깨움
	closesocket(_listenerSocket);
	_accepterThread.request_stop();

	// 3. IOCP 핸들 정리
	CloseHandle(_iocpHandle);
}

IocpResult EchoServer::Recv(std::shared_ptr<Client> client) {
	ZeroMemory(&client->recvOverlapped.overlapped, sizeof(WSAOVERLAPPED));
	client->recvOverlapped.wsaBuffer.len = Const::socketBufferSize;
	client->recvOverlapped.wsaBuffer.buf = client->recvOverlapped.buffer;
	client->recvOverlapped.type = EIoType::Recv;

	DWORD flag = 0;
	DWORD recvNumBytes = 0;

	const int result = WSARecv(
		client->socket,
		&(client->recvOverlapped.wsaBuffer),
		1,
		&recvNumBytes,
		&flag,
		(LPWSAOVERLAPPED)(&client->recvOverlapped),
		NULL);
	if (result == SOCKET_ERROR &&
		(WSAGetLastError() != ERROR_IO_PENDING)) {
		_logger->Error("{}: {}",
			EIocpErrorHelper::ToString(EIocpError::RecvFailed),
			WSAGetLastError());
		return IocpResult::Err(EIocpError::RecvFailed);
	}

	return IocpResult::Ok;
}

IocpResult EchoServer::Send(
	std::shared_ptr<Client> client,
	std::string_view message,
	ULONG messageLen
) {
	ZeroMemory(&client->sendOverlapped.overlapped, sizeof(WSAOVERLAPPED));
	DWORD recvNumBytes = 0;
	CopyMemory(client->sendOverlapped.buffer, message.data(), messageLen);

	client->sendOverlapped.wsaBuffer.len = messageLen;
	client->sendOverlapped.wsaBuffer.buf = client->sendOverlapped.buffer;
	client->sendOverlapped.type = EIoType::Send;

	const int result = WSASend(
		client->socket,
		&client->sendOverlapped.wsaBuffer,
		1,
		&recvNumBytes,
		0,
		(LPWSAOVERLAPPED)(&client->sendOverlapped),
		NULL);
	if (result == SOCKET_ERROR &&
		WSAGetLastError() != ERROR_IO_PENDING) {
		_logger->Error("{}: {}",
			EIocpErrorHelper::ToString(EIocpError::SendFailed),
			WSAGetLastError());
		return IocpResult::Err(EIocpError::SendFailed);
	}

	return IocpResult::Ok;
}

void EchoServer::WorkerLoop(std::stop_token st) {
	while (!st.stop_requested()) {
		DWORD bytesTransferred = 0;
		Client* client = nullptr;
		LPOVERLAPPED overlapped = nullptr;

		// Completion Packet 추가 시 까지 sleep...
		const BOOL ok = GetQueuedCompletionStatus(
			_iocpHandle,
			&bytesTransferred,
			(PULONG_PTR)&client,
			&overlapped,
			INFINITE);

		// I/O Completion 발생시..
		// 1. Kernel 이 completion packet 을 IOCP FIFO 에 추가.
		// 2. 대기 중인 스레드 중 1개만 깨어남.
		// 3. 깨어난 스레드가 packet 을 받아 처리.
		// 4. 처리가 끝나면 다시 GetQueuedCompletionStatus 호출 (while 루프로 다시 호출).

		// Completion Packet 을 받아서 현재 스레드는 꺠어난 상태. user-mode 로 context 전환.
		if (ok == FALSE) {
			const bool isIoFailed = overlapped != nullptr;
			const DWORD err = GetLastError();
			if (isIoFailed) {
				_logger->Error("{}: {}",
					EIocpErrorHelper::ToString(EIocpError::IoFailed),
					err);
				continue;
			}

			// IOCP internal error handling.
			_logger->Error("{}: {}",
				EIocpErrorHelper::ToString(EIocpError::IocpInternalError),
				err);

			if (err == WAIT_TIMEOUT) {
				continue;
			}

			// CRITICAL ERROR! shutdown server.
			break;
		}

		// Stop()에서 보낸 종료 신호 처리
		if (client == nullptr && overlapped == nullptr) {
			break;
		}

		// 클라이언트 graceful disconnect 처리
		if (bytesTransferred == 0) {
			auto clientPtr = client->shared_from_this();
			_logger->Info("[Graceful Disconnect] Socket #{} is disconnected from the server (this)",
				clientPtr->socket);
			this->CloseSocket(clientPtr, true);
			continue;
		}

		auto clientPtr = client->shared_from_this();

		auto ioCtx = (IoContext*)overlapped;
		switch (ioCtx->type) {
			case EIoType::Recv:
			{
				ioCtx->buffer[bytesTransferred] = NULL;

				auto data = std::string(ioCtx->buffer, bytesTransferred);

				_logger->Info("[Recv] socket #{}, data: {}, bytesTransferred: {}",
					clientPtr->socket,
					data,
					bytesTransferred);

				auto res = this->Send(
					clientPtr,
					{ ioCtx->buffer, std::size(ioCtx->buffer) },
					bytesTransferred);
				if (res.IsErr()) {
					_logger->Error("{}: {}",
						EIocpErrorHelper::ToString(res.Err()),
						WSAGetLastError());
					this->CloseSocket(clientPtr, true);
					break;
				}

				// prepare next recv
				res = this->Recv(clientPtr);
				if (res.IsErr()) {
					_logger->Error("{}: {}",
						EIocpErrorHelper::ToString(res.Err()),
						WSAGetLastError());
					this->CloseSocket(clientPtr, true);
				}
			}
			break;

			case EIoType::Send:
			{
				_logger->Info("[Send] socket #{}, bytesTransferred: {}",
					(int)clientPtr->socket,
					bytesTransferred);
			}
			break;

			default:
			{
				_logger->Error("{}, socket #{}, bytesTransferred: {}",
					EIocpErrorHelper::ToString(EIocpError::UnknownError),
					(int)clientPtr->socket,
					bytesTransferred);
			}
			break;
		}
	}
}

void EchoServer::AccepterLoop(std::stop_token st) {
	SOCKADDR_IN clientAddr{};
	int addrLen = sizeof(SOCKADDR_IN);

	while (!st.stop_requested()) {
		auto client = this->FindClient();
		if (!client) {
			_logger->Error("Client Full!");
			return;
		}

		client->socket = accept(
			_listenerSocket,
			(SOCKADDR*)&clientAddr,
			&addrLen);
		if (client->socket == INVALID_SOCKET) {
			_logger->Error("{}: {}",
				EIocpErrorHelper::ToString(EIocpError::AcceptFailed),
				WSAGetLastError());
			continue;
		}

		// Associate client socket with IOCP
		HANDLE iocpConnHandle = CreateIoCompletionPort(
			(HANDLE)client->socket,
			_iocpHandle,
			(ULONG_PTR)(client.get()),
			0);
		if (iocpConnHandle == NULL || iocpConnHandle != _iocpHandle) {
			_logger->Error("{}: {}",
				EIocpErrorHelper::ToString(EIocpError::CreateIocpFailed),
				WSAGetLastError());
			continue;
		}

		if (auto res = this->Recv(client); res.IsErr()) {
			_logger->Error("error propated. error: {}",
				EIocpErrorHelper::ToString(res.Err()));
			return;
		}

		char clientIp[Const::clientIpBufferSize]{ 0, };
		inet_ntop(
			AF_INET,
			&clientAddr.sin_addr,
			clientIp,
			Const::clientIpBufferSize - 1);
		_logger->Info("Client connected. socket: {}", (int)client->socket);

		_clientCount++;
	}
}

void EchoServer::CloseSocket(std::shared_ptr<Client> client, bool isFireAndForget) {
	client->Close(isFireAndForget);
}

std::shared_ptr<Client> EchoServer::FindClient() {
	auto found = std::find_if(_clients.begin(), _clients.end(), [](std::shared_ptr<Client> const& c) {
		return c->socket == INVALID_SOCKET;
	});
	if (found != _clients.end())
		return *found;

	return nullptr;
}
}
