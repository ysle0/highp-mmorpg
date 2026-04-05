#pragma once

#include <memory>
#include <logger/Logger.hpp>

namespace highp::protocol {
    struct Packet;
}

void onPacketReceived(const std::shared_ptr<highp::log::Logger>& logger,
                      const highp::protocol::Packet* packet);
