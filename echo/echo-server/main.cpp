#include <iostream>
#include <TextLogger.h>
#include "EchoServer.h"

using highp::log::Logger;
using highp::log::TextLogger;
using highp::err::EIocpErrorHelper;
using highp::echo_srv::EchoServer;

int main() {
	auto logger = Logger::Default<TextLogger>();
	EchoServer es(logger);

	if (auto res = es.Start(); res.IsErr()) {
		logger->Error(EIocpErrorHelper::ToString(res.Err()));
		return -1;
	}

	logger->Info("Press Enter to stop the server...");
	std::cin.get();

	es.Stop();
	return 0;
}
