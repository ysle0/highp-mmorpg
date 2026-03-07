#pragma once
#include <atomic>
#include <memory>
#include <thread>

#include "config/NetworkCfg.h"
#include "server/ISessionEventReceiver.h"
#include "server/PacketDispatcher.hpp"
#include "server/ServerLifecycle.h"
#include "socket/ISocket.h"
#include "socket/SocketOptionBuilder.h"

#include "handlers/ChatMessageHandler.h"
#include "handlers/JoinRoomHandler.h"

using namespace highp;

class Server : public net::ISessionEventReceiver {
	using Res = fn::Result<void, err::ENetworkError>;

public:
	explicit Server(
		std::shared_ptr<log::Logger> logger,
		net::NetworkCfg networkCfg,
		std::shared_ptr<net::SocketOptionBuilder> socketOptionBuilder = nullptr);
	~Server() noexcept override;

	Res Start(std::shared_ptr<net::ISocket> listenSocket);
	void Stop();

private:
	void OnAccept(std::shared_ptr<net::Client> client) override;
	void OnRecv(std::shared_ptr<net::Client> client, std::span<const char> data) override;
	void OnSend(std::shared_ptr<net::Client> client, size_t bytesTransferred) override;
	void OnDisconnect(std::shared_ptr<net::Client> client) override;

	/// Logic thread 메인 루프
	void LogicLoop(std::stop_token st);

	std::shared_ptr<log::Logger> _logger;
	std::shared_ptr<net::SocketOptionBuilder> _socketOptionBuilder;
	net::NetworkCfg _config;
	std::unique_ptr<net::ServerLifeCycle> _lifecycle;
	bool _hasStopped = false;
	std::atomic<int> _tickMs;

	net::PacketDispatcher _dispatcher;
	ChatMessageHandler _chatMessageHandler;
	JoinRoomHandler _joinRoomHandler;

	std::jthread _logicThread;
};
