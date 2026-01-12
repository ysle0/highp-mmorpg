#pragma once

#include "framework.h"

namespace highp::network {

enum class IoOperation {
	Accept,
	Recv,
	Send,
	Disconnect
};

struct OverlappedEx : OVERLAPPED {
	IoOperation operation = IoOperation::Accept;
	SOCKET clientSocket = INVALID_SOCKET;
	char buffer[1024] = {};
	WSABUF wsaBuf = {};

	OverlappedEx() {
		Reset();
	}

	void Reset() {
		Internal = 0;
		InternalHigh = 0;
		Offset = 0;
		OffsetHigh = 0;
		hEvent = nullptr;
		operation = IoOperation::Accept;
		clientSocket = INVALID_SOCKET;
		wsaBuf.buf = nullptr;
		wsaBuf.len = 0;
	}
};

} // namespace highp::network
