#pragma once
#pragma comment(lib, "ws2_32.lib")

#include <atomic>
#include <IocpError.h>
#include <Logger.hpp>
#include <memory>
#include <string_view>
#include <thread>
#include <vector>
#include <WinSock2.h>
#include "Client.h"
#include "Config.h"
#include "Const.h"

namespace highp::echo_srv {
using highp::err::IocpResult;
using highp::log::Logger;

class EchoServer final {
public:
	~EchoServer() noexcept;
	explicit EchoServer(std::shared_ptr<Logger> logger);
	EchoServer(std::shared_ptr<Logger> logger, RuntimeCfg config);

	/// <summary>
	/// Start EchoServer with configured values.
	/// <see href="config.runtime.toml"/>
	/// </summary>
	IocpResult Start();

	void Stop();

private:
	IocpResult Recv(std::shared_ptr<Client> client);
	IocpResult Send(std::shared_ptr<Client> client, std::string_view message, ULONG messageLen);

	void WorkerLoop(std::stop_token st);
	void AccepterLoop(std::stop_token st);

	void CloseSocket(std::shared_ptr<Client> client, bool isFireAndForget);

	std::shared_ptr<Client> FindClient();

	// dependency injected
private:
	std::shared_ptr<Logger> _logger;

	// networks
	SOCKET _listenerSocket = INVALID_SOCKET;
	HANDLE _iocpHandle = INVALID_HANDLE_VALUE;
	std::vector<std::shared_ptr<Client>> _clients;
	char _socketBuffer[Const::socketBufferSize]{ 0, };

	// threads
	std::vector<std::jthread> _workerThreads;
	std::jthread _accepterThread;

	// config & states
	RuntimeCfg _config = RuntimeCfg::WithDefaults();
	std::atomic_uint _clientCount = 0;
};
}
