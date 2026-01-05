#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")
#include <memory>
#include <string_view>
#include <WinSock2.h>

// fwddecl
namespace highp::lib::logger {
class Logger;
};

namespace highp::echo::client {
using highp::lib::logger::Logger;

class EchoClient final {
public:
	explicit EchoClient(std::shared_ptr<Logger> logger) : _logger(logger) {}

	[[nodiscard]] bool Connect(const char* ipAddress, unsigned short port);

	bool Disconnect();

	void Send(std::string_view message);

private:
	std::shared_ptr<Logger> _logger;
	SOCKET _serverSocket = INVALID_SOCKET;
};
};
