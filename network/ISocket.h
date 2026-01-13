#pragma once
#include <macro.h>
#include <Result.hpp>
#include <SocketError.h>
#include "NetworkTransport.hpp"

namespace highp::network {

class ISocket {
public:
	using Res = highp::fn::Result<void, highp::err::ESocketError>;
	virtual ~ISocket() = default;

	virtual Res Initialize() PURE;
	virtual Res CreateSocket(NetworkTransport) PURE;
	virtual Res Bind(unsigned short port) PURE;
	virtual Res Listen(int backlog) PURE;
	virtual Res Accept(SocketHandle clientSocket) PURE;
	virtual Res Cleanup() PURE;
	virtual SocketHandle GetSocketHandle() const PURE;
};
}
