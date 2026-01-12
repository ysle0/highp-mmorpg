#pragma once

#include "platform.h"
#include <memory>
#include "OverlappedExt.h"

namespace highp::network {
struct Client : public std::enable_shared_from_this<Client> {
	Client();
	void Close(bool isFireAndForget);

	SocketHandle socket = INVALID_SOCKET;
	OverlappedExt recvOverlapped;
	OverlappedExt sendOverlapped;
};
}
