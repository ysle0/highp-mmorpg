#pragma once
#include "AsyncSocket.h"

namespace highp::network {
class WindowsAsyncSocket : public AsyncSocket {
	using Res = highp::fn::Result<void, highp::err::ESocketError>;
	using ResWithData = highp::fn::Result<SOCKET, highp::err::ESocketError>;

public:
	virtual ~WindowsAsyncSocket() noexcept override;

public:
	virtual Res Initialize() override;
	virtual Res CreateSocket(NetworkTransport transport) override;
	virtual Res Bind(unsigned short port) override;
	virtual Res Listen(int backlog) override;
	virtual Res Accept(SocketHandle clientSocket) override;
	virtual Res Cleanup() override;

private:
	SOCKADDR_IN _sockaddr;
};
}

