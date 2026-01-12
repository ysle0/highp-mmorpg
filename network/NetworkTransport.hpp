#pragma once
#include "framework.h"

namespace highp::network {
enum class ETransport {
	TCP,
	UDP,
};

#ifdef _WIN32
using NetworkTransport = WindowsNetworkTransport;
#elif _LINUX
using NetworkTransport = LinuxNetworkTransport;
#endif

#ifdef _WIN32

class WindowsNetworkTransport {
public:
	explicit WindowsNetworkTransport(ETransport transportType) : _transportType(transportType) {}

	[[nodiscard]] constexpr std::pair<INT, IPPROTO> GetInfos() const {
		switch (_transportType) {
			case ETransport::TCP: return { SOCK_STREAM, IPPROTO_TCP };
			case ETransport::UDP: return { SOCK_DGRAM, IPPROTO_UDP };
		}
	}

private:
	ETransport _transportType;
};

#elif _LINUX

class LinuxNetworkTransport {
public:
	explicit LinuxNetworkTransport(ETransport transportType) : _transportType(transportType) {}
	[[nodiscard]] constexpr std::pair<INT, IPPROTO> GetInfos() const {
		switch (_transportType) {
			case ETransport::TCP: return { };
			case ETransport::UDP: return { };
		}
	}

private:
	ETransport _transportType;
};
#endif
}
