#pragma once
#include <platform.h>
#include <Const.h>
#include <IocpError.h>
#include <ISocket.h>
#include <RuntimeCfg.h>
#include <stop_token>
#include <thread>

namespace highp::network {
struct Client;
}

namespace highp::log {
class Logger;
}

namespace highp::echo_srv {
class EchoServer final {
	using Res = highp::fn::Result<void, highp::err::EIocpError>;
public:
	~EchoServer() noexcept;
	explicit EchoServer(std::shared_ptr<log::Logger> logger);
	EchoServer(std::shared_ptr<log::Logger> logger, network::RuntimeCfg config);

	/// <summary>
	/// Start EchoServer with configured values.
	/// <see href="config.runtime.toml"/>
	/// </summary>
	Res Start(std::shared_ptr<network::ISocket> asyncSocket);

	void Stop();

private:
	Res Recv(std::shared_ptr<network::Client> clientSocket);
	Res Send(std::shared_ptr<network::Client> clientSocket, std::string_view message, ULONG messageLen);

	void WorkerLoop(std::stop_token st);
	void AccepterLoop(std::shared_ptr<network::ISocket> asyncSocket, std::stop_token st);

	void CloseSocket(std::shared_ptr<network::Client> clientSocket, bool isFireAndForget);

	std::shared_ptr<network::Client> FindClient();

	// dependency injected
private:
	std::shared_ptr<log::Logger> _logger;

	// networks
	HANDLE _iocpHandle;
	std::vector<std::shared_ptr<network::Client>> _clients;
	char _socketBuffer[network::Const::socketBufferSize]{ 0, };

	// threads
	std::vector<std::jthread> _workerThreads;
	std::jthread _accepterThread;

	// config & states
	network::RuntimeCfg _config = network::RuntimeCfg::WithDefaults();
	std::atomic_uint _clientCount = 0;
};
}
