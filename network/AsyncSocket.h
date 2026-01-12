#pragma once
#include "ISocket.h"

namespace highp::network {
class AsyncSocket :
	public ISocket,
	public std::enable_shared_from_this<AsyncSocket> {

public:
	explicit AsyncSocket(std::shared_ptr<log::Logger> logger);
	virtual ~AsyncSocket() noexcept = default;

	ISocket::Res Initialize() override;
	ISocket::Res CreateSocket(NetworkTransport) override;
	ISocket::Res Bind(unsigned short port) override;
	ISocket::Res Listen(int backlog) override;
	ISocket::Res Accept(SocketHandle) override;
	ISocket::Res Cleanup() override;

	SocketHandle GetSocketHandle() const {
		return _socketHandle;
	}

protected:
	SocketHandle _socketHandle;
	std::shared_ptr<log::Logger> _logger;
};
}
