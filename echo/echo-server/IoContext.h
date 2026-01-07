#pragma once
#pragma comment(lib, "ws2_32.lib")

#include <WinSock2.h>
#include "Const.h"
#include "EIoType.h"

namespace highp::echo_srv {
struct IoContext {
	// WSAOVERLAPPED must be first for correct casting to LPWSAOVERLAPPED
	WSAOVERLAPPED overlapped;
	EIoType type;
	SOCKET client;
	WSABUF wsaBuffer;
	char buffer[Const::socketBufferSize];

	// Overlapped struct 는 등록되고나서 주소 변경이나 이동이 일어나면
	// mem corruption 이 발생할 수 있으므로 복사 및 이동 금지
	IoContext() = default;
	IoContext(const IoContext&) = delete;
	IoContext& operator=(const IoContext&) = delete;
	IoContext(IoContext&&) = delete;
	IoContext& operator=(IoContext&&) = delete;
};
}
