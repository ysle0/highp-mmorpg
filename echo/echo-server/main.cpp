#include <iostream>
#include <Logger.hpp>
#include <NetworkCfg.h>
#include <TextLogger.h>
#include "EchoServer.h"

using namespace highp::echo_srv;
using namespace highp::network;

using Logger = highp::log::Logger;
using TextLogger = highp::log::TextLogger;

int main() {
	auto logger = Logger::Default<TextLogger>();
	auto config = NetworkCfg::FromFile("config.runtime.toml");

	EchoServer es(logger, config);
	if (auto res = es.Start(asyncSocket); res.HasErr()) {
		logger->Error("Failed to start server.");
		return -1;
	}

	logger->Info("Press Enter to stop the server...");
	std::cin.get();

	es.Stop();
	return 0;
}
