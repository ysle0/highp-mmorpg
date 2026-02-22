#include <logger/Logger.hpp>
#include <logger/TextLogger.h>
#include <config/NetworkCfg.h>
#include <transport/NetworkTransport.hpp>
#include <socket/SocketOptionBuilder.h>
#include <socket/SocketHelper.h>

using namespace highp;

int main() {
    auto logger = log::Logger::Default<log::TextLogger>();
    auto networkCfg = net::NetworkCfg::FromFile("config.runtime.toml");
    auto transport = net::NetworkTransport(net::ETransport::TCP);
    auto socketOptBuilder = std::make_shared<net::SocketOptionBuilder>(logger);
    auto listenSocket = net::SocketHelper::MakeDefaultListener(
        logger,
        transport,
        networkCfg,
        socketOptBuilder);
    return 0;
}
