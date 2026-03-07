#pragma once
#include <memory>

#include "config/NetworkCfg.h"
#include "server/PacketHandler.hpp"
#include "server/ServerLifecycle.h"
#include "socket/ISocket.h"
#include "socket/SocketOptionBuilder.h"

using namespace highp;

class Server : public net::PacketHandler {
	using Res = fn::Result<void, err::ENetworkError>;

public:
	explicit Server(
		std::shared_ptr<log::Logger> logger,
		net::NetworkCfg networkCfg,
		std::shared_ptr<net::SocketOptionBuilder> socketOptionBuilder = nullptr);
	virtual ~Server() noexcept override;

	Res Start(std::shared_ptr<net::ISocket> listenSocket);
	void Stop();

private:
	void OnRecv(std::shared_ptr<net::Client> client, std::span<const char> data) override;
	void OnSend(std::shared_ptr<net::Client> client, size_t bytesTransferred) override;
	void OnDisconnect(std::shared_ptr<net::Client> client) override;

	std::shared_ptr<net::ISocket> _listenSocket;
	std::shared_ptr<net::SocketOptionBuilder> _socketOptionBuilder;
	net::NetworkCfg _config;
	std::unique_ptr<net::ServerLifeCycle> _lifecycle;
	bool _hasStopped = false;
};
