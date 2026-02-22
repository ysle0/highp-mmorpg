#include <chrono>
#include <Logger.hpp>
#include <TextLogger.h>
#include <thread>
#include "EchoClient.h"

using namespace std::chrono_literals;

using namespace highp;

int main() {
    auto logger = Logger::Default<log::TextLogger>();
    logger->Info("hello, echo clientSocket {}!", 1);

    EchoClient ec(logger);
    if (bool result = ec.Connect("127.0.0.1", 8080u); !result) {
        logger->Error("echo clientSocket connect failed.");
        return -1;
    }

    for (int i = 0; i < 5; i++) {
        const auto msg = std::format("Hello from echo clientSocket! #{}", i + 1);
        ec.Send(msg);
    }

    std::this_thread::sleep_for(5s);

    ec.Disconnect();

    return 0;
}
