#pragma once
#include "platform.h"
#include "IAsyncAcceptor.h"
#include <Logger.hpp>
#include <vector>
#include <thread>

namespace highp::network {
class WindowsAsyncAcceptor : public IAsyncAcceptor {
	using Res = highp::fn::Result<void, highp::err::ESocketError>;
public:
	explicit WindowsAsyncAcceptor(
		std::shared_ptr<log::Logger> logger,
		unsigned prewarmAcceptorThreadCount
	);
	virtual ~WindowsAsyncAcceptor() noexcept override;

	virtual Res Initialize(
		SocketHandle listenrSocket,
		SocketHandle clientSocket,
		SOCKADDR_IN& clientAddr
	) override;

	virtual Res PostAccept() override;

	virtual Res PostAcceptMany(unsigned batchCount) override;

private:
	std::shared_ptr<log::Logger> _logger;
	unsigned _prewarmAcceptorThreadCount;
	std::vector<std::jthread> _acceptorThreadsPool;
};
}
