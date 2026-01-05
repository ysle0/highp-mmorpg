#pragma once

#include <compileTime/Config.h>
#include <Windows.h>
#include "ConfigEmbedded.h"

namespace highp::echo::server {

// =============================================================================
// Compile-time Config
// =============================================================================
namespace detail {
	inline constexpr auto CompileCfgParser = highp::lib::config::compileTime::Config<>::From(
		highp::echo::server::config::ECHO_SERVER_CONFIG);
}

struct Const {
	static constexpr INT socketBufferSize = detail::CompileCfgParser.Int("socket.buffer_size", 4096);
	static constexpr INT clientIpBufferSize = detail::CompileCfgParser.Int("network.client_ip_buffer_size", 32);

	// Compile-time verification
	static_assert(socketBufferSize > 0, "socket.buffer_size must be positive");
	static_assert(clientIpBufferSize > 0, "network.client_ip_buffer_size must be positive");
};

} // namespace highp::echo::server
