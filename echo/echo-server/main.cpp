#include <iostream>
#include <Logger.hpp>
#include <TextLogger.h>
#include <WindowsAsyncSocket.h>
#include <NetworkTransport.hpp>
#include <RuntimeCfg.h>
#include "EchoServer.h"

using namespace highp::echo_srv;
using namespace highp::network;

using Logger = highp::log::Logger;
using TextLogger = highp::log::TextLogger;

int main() {
	auto logger = Logger::Default<TextLogger>();
	auto config = RuntimeCfg::WithDefaults();

	auto asyncSocket = std::make_shared<WindowsAsyncSocket>(logger);
	if (auto res = asyncSocket->Initialize(); res.HasErr()) {
		logger->Error("Failed to initialize socket.");
		return -1;
	}

	auto transport = NetworkTransport{ ETransport::TCP };
	if (auto res = asyncSocket->CreateSocket(transport); res.HasErr()) {
		logger->Error("Failed to create socket.");
		return -1;
	}
	if (auto res = asyncSocket->Bind(config.port); res.HasErr()) {
		logger->Error("Failed to bind socket.");
		return -1;
	}
	if (auto res = asyncSocket->Listen(config.backlog); res.HasErr()) {
		logger->Error("Failed to listen socket.");
		return -1;
	}

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
