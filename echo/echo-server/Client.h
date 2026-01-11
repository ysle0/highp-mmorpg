#pragma once
#pragma comment(lib, "ws2_32.lib")

#include <WinSock2.h>
#include <memory>
#include "IoContext.h"

namespace highp::echo_srv {
struct Client : public std::enable_shared_from_this<Client> {
	Client();
	void Close(bool isFireAndForget);

	SOCKET socket = INVALID_SOCKET;
	IoContext recvOverlapped;
	IoContext sendOverlapped;
};
}
