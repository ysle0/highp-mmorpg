#pragma once
#include "platform.h"

namespace highp::network {

struct AcceptContext {
	SocketHandle acceptSocket = InvalidSocket;
	SocketHandle listenSocket = InvalidSocket;
	SOCKADDR_IN localAddr{};
	SOCKADDR_IN remoteAddr{};
};

}
