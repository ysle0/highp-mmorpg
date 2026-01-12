#include <iostream>
#include <Logger.hpp>
#include <TextLogger.h>
#include "EchoServer.h"

using namespace highp::echo_srv;

using Logger = highp::log::Logger;
using TextLogger = highp::log::TextLogger;

int main() {
	auto logger = Logger::Default<TextLogger>();
	EchoServer es(logger);

	if (auto res = es.Start(); res.HasErr()) {
		res.LogError(logger);
		return -1;
	}

	logger->Info("Press Enter to stop the server...");
	std::cin.get();

	es.Stop();
	return 0;
}
