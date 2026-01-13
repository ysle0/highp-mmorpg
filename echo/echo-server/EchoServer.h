#pragma once
#include <platform.h>
#include <Acceptor.h>
#include <AcceptContext.h>
#include <Const.h>
#include <IoCompletionPort.h>
#include <IocpError.h>
#include <ISocket.h>
#include <RuntimeCfg.h>

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

	Res Start(std::shared_ptr<network::ISocket> asyncSocket);
	void Stop();

private:
	void OnCompletion(const network::CompletionEvent& event);
	void OnAccept(network::AcceptContext& ctx);

	Res Recv(std::shared_ptr<network::Client> clientSocket);
	Res Send(std::shared_ptr<network::Client> clientSocket, std::string_view message, ULONG messageLen);

	void CloseSocket(std::shared_ptr<network::Client> clientSocket, bool isFireAndForget);
	std::shared_ptr<network::Client> FindClient();

private:
	std::shared_ptr<log::Logger> _logger;
	network::RuntimeCfg _config = network::RuntimeCfg::WithDefaults();

	std::unique_ptr<network::IoCompletionPort> _iocp;
	std::unique_ptr<network::Acceptor> _acceptor;

	std::vector<std::shared_ptr<network::Client>> _clients;
	std::atomic_uint _clientCount = 0;
};
}
