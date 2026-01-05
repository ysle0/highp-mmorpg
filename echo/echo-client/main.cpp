#include <chrono>
#include <Logger.hpp>
#include <TextLogger.h>
#include <thread>
#include "EchoClient.h"

using namespace std::chrono_literals;

using highp::lib::logger::Logger;
using highp::lib::logger::TextLogger;
using highp::echo::client::EchoClient;

int main() {
	auto logger = Logger::Default<TextLogger>();
	logger->Info("hello, echo client {}!", 1);

	EchoClient ec(logger);
	if (bool result = ec.Connect("127.0.0.1", 8080u); !result) {
		logger->Error("echo client connect failed.");
		return -1;
	}

	for (int i = 0; i < 5; i++) {
		const auto msg = std::format("Hello from echo client! #{}", i + 1);
		ec.Send(msg);
	}

	std::this_thread::sleep_for(5s);

	ec.Disconnect();

	return 0;
}