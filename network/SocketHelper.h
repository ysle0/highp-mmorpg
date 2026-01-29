#pragma once
#include "ISocket.h"
#include <Logger.hpp>
#include "NetworkCfg.h"
#include "NetworkTransport.hpp"

namespace highp::network {
class SocketHelper {
public:
	SocketHelper() = delete;

	static std::shared_ptr<ISocket> MakeDefaultListener(
		std::shared_ptr<log::Logger> logger,
		NetworkTransport netTransport,
		NetworkCfg networkCfg
	);
};
}

