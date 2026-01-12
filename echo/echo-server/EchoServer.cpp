#include <Client.h>
#include <Windows.h>
#include <WS2tcpip.h>
#include "EchoServer.h"

namespace highp::echo_srv {
using namespace highp::err;
using namespace highp::log;

EchoServer::~EchoServer() {
	WSACleanup();
}

EchoServer::EchoServer(std::shared_ptr<Logger> logger)
	: _logger(logger)
	, _config(RuntimeCfg::WithDefaults()) {}

EchoServer::EchoServer(std::shared_ptr<Logger> logger, RuntimeCfg config)
	: _logger(logger)
	, _config(std::move(config)) {}

EchoServer::Res EchoServer::Start(
	std::shared_ptr<highp::network::ISocket> asyncSocket
) {
	// 1. Create a new IOCP handle
	_iocpHandle = CreateIoCompletionPort(
		INVALID_HANDLE_VALUE,
		nullptr,
		NULL,
		_config.workerThreadCount);
	if (_iocpHandle == NULL) {
		asyncSocket->Cleanup();
		return err::LogErrorWithResult<EIocpError::CreateIocpFailed>(_logger);
	}

	// 6. Populate clients
	for (auto i = 0; i < _config.maxClients; i++) {
		_clients.emplace_back(std::make_shared<highp::network::Client>());
	}

	// 7. Create worker threads
	for (auto i = 0; i < _config.workerThreadCount; i++) {
		_workerThreads.emplace_back([this](std::stop_token st) {
			WorkerLoop(st);
		});
	}
	_logger->Info("worker threads created. count: {}", _config.workerThreadCount);

	// 8. Create accepter thread
	_accepterThread = std::jthread([this, asyncSocket](std::stop_token st) {
		AccepterLoop(asyncSocket, st);
	});
	_logger->Info("accepter thread created.");

	_logger->Info("server started on port {}.", _config.port);
	return Res::Ok();
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
	//closesocket(_socket);
	_accepterThread.request_stop();

	// 3. IOCP 핸들 정리
	CloseHandle(_iocpHandle);
}

EchoServer::Res EchoServer::Recv(std::shared_ptr<highp::network::Client> clientSocket) {
	ZeroMemory(&clientSocket->recvOverlapped.overlapped, sizeof(WSAOVERLAPPED));

	clientSocket->recvOverlapped.wsaBuffer.buf = clientSocket->recvOverlapped.buffer;
	clientSocket->recvOverlapped.wsaBuffer.len = network::Const::socketBufferSize;
	clientSocket->recvOverlapped.ioType = network::EIoType::Recv;

	DWORD flag = 0;
	DWORD recvNumBytes = 0;

	const int result = WSARecv(
		clientSocket->socket,
		&(clientSocket->recvOverlapped.wsaBuffer),
		1,
		&recvNumBytes,
		&flag,
		(LPWSAOVERLAPPED)(&clientSocket->recvOverlapped),
		NULL);
	if (result == SOCKET_ERROR &&
		(WSAGetLastError() != ERROR_IO_PENDING)
		) {
		return err::LogErrorWithResult<EIocpError::RecvFailed>(_logger);
	}

	return Res::Ok();
}

EchoServer::Res EchoServer::Send(
	std::shared_ptr<network::Client> clientSocket,
	std::string_view message,
	ULONG messageLen
) {
	ZeroMemory(&clientSocket->sendOverlapped.overlapped, sizeof(WSAOVERLAPPED));
	DWORD recvNumBytes = 0;
	CopyMemory(clientSocket->sendOverlapped.buffer, message.data(), messageLen);

	clientSocket->sendOverlapped.wsaBuffer.len = messageLen;
	clientSocket->sendOverlapped.wsaBuffer.buf = clientSocket->sendOverlapped.buffer;
	clientSocket->sendOverlapped.ioType = network::EIoType::Send;

	const int result = WSASend(
		clientSocket->socket,
		&clientSocket->sendOverlapped.wsaBuffer,
		1,
		&recvNumBytes,
		0,
		(LPWSAOVERLAPPED)(&clientSocket->sendOverlapped),
		NULL);
	if (result == SOCKET_ERROR &&
		WSAGetLastError() != ERROR_IO_PENDING) {
		return err::LogErrorWithResult<EIocpError::SendFailed>(_logger);
	}

	return Res::Ok();
}

void EchoServer::WorkerLoop(std::stop_token st) {
	while (!st.stop_requested()) {
		DWORD bytesTransferred = 0;
		network::Client* clientSocket = nullptr;
		LPOVERLAPPED overlapped = nullptr;

		// Completion Packet 추가 시 까지 sleep...
		const BOOL ok = GetQueuedCompletionStatus(
			_iocpHandle,
			&bytesTransferred,
			(PULONG_PTR)&clientSocket,
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
				err::LogError<EIocpError::IoFailed>(_logger);
				continue;
			}

			// IOCP internal error handling.
			_logger->Error("{}: {}",
				err::FromIocpErrorToString<EIocpError::IocpInternalError>(),
				err);

			if (err == WAIT_TIMEOUT) {
				continue;
			}

			// CRITICAL ERROR! shutdown server.
			break;
		}

		// Stop()에서 보낸 종료 신호 처리
		if (clientSocket == nullptr && overlapped == nullptr) {
			break;
		}

		// 클라이언트 graceful disconnect 처리
		if (bytesTransferred == 0) {
			auto clientPtr = clientSocket->shared_from_this();
			_logger->Info("[Graceful Disconnect] Socket #{} is disconnected from the server (this)",
				clientPtr->socket);
			this->CloseSocket(clientPtr, true);
			continue;
		}

		auto clientPtr = clientSocket->shared_from_this();

		auto ioCtx = (network::OverlappedExt*)overlapped;
		switch (ioCtx->ioType) {
			case network::EIoType::Recv:
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
				if (res.HasErr()) {
					res.LogError(_logger);
					this->CloseSocket(clientPtr, true);
					break;
				}

				// prepare next recv
				res = this->Recv(clientPtr);
				if (res.HasErr()) {
					res.LogError(_logger);
					this->CloseSocket(clientPtr, true);
				}
			}
			break;

			case network::EIoType::Send:
			{
				_logger->Info("[Send] socket #{}, bytesTransferred: {}",
					(int)clientPtr->socket,
					bytesTransferred);
			}
			break;

			default:
			{
				_logger->Error("{}, socket #{}, bytesTransferred: {}",
					err::FromIocpErrorToString<EIocpError::UnknownError>(),
					(int)clientPtr->socket,
					bytesTransferred);
			}
			break;
		}
	}
}

void EchoServer::AccepterLoop(std::shared_ptr<highp::network::ISocket> asyncSocket, std::stop_token st) {
	SOCKADDR_IN clientAddr{};
	int addrLen = sizeof(SOCKADDR_IN);

	while (!st.stop_requested()) {
		auto clientSocket = this->FindClient();
		if (!clientSocket) {
			_logger->Error("Client Full!");
			return;
		}

		if (auto res = asyncSocket->Accept(clientSocket->socket); res.HasErr()) {
			err::LogError<EIocpError::AcceptThreadFailed>(_logger);
			continue;
		}


		//clientSocket->socket = accept(
		//	asyncSocket->GetSocket(),
		//	(SOCKADDR*)&clientAddr,
		//	&addrLen);
		//if (clientSocket->socket == INVALID_SOCKET) {
		//}

		// Associate clientSocket socket with IOCP
		HANDLE iocpConnHandle = CreateIoCompletionPort(
			(HANDLE)clientSocket->socket,
			_iocpHandle,
			(ULONG_PTR)(clientSocket.get()),
			0);
		if (iocpConnHandle == NULL || iocpConnHandle != _iocpHandle) {
			err::LogError<EIocpError::CreateIocpFailed>(_logger);
			continue;
		}

		if (auto res = this->Recv(clientSocket); res.HasErr()) {
			//_logger->Error("error propated. error: {}",
			//	EIocpErrorHelper::FromIocpErrorToString(res.Err()));
			return;
		}

		char clientIp[network::Const::clientIpBufferSize]{ 0, };
		inet_ntop(
			AF_INET,
			&clientAddr.sin_addr,
			clientIp,
			network::Const::clientIpBufferSize - 1);
		_logger->Info("Client connected. socket: {}", (int)clientSocket->socket);

		_clientCount++;
	}
}

void EchoServer::CloseSocket(std::shared_ptr<network::Client> clientSocket, bool isFireAndForget) {
	clientSocket->Close(isFireAndForget);
}

std::shared_ptr<network::Client> EchoServer::FindClient() {
	auto found = std::find_if(_clients.begin(), _clients.end(), [](std::shared_ptr<network::Client> const& c) {
		return c->socket == INVALID_SOCKET;
	});
	if (found != _clients.end())
		return *found;

	return nullptr;
}
}
