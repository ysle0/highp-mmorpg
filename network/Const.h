#pragma once

#include <compileTime/Config.hpp>
#include <Windows.h>
#include "ConfigEmbedded.h"

namespace highp::network {

// =============================================================================
// Compile-time Config
// =============================================================================
inline constexpr auto Config = highp::config::CompileTimeConfig<>::From(
	highp::network::NETWORK_CONFIG);

struct Const {
	static constexpr INT socketBufferSize = Config.Int("socket.buffer_size", 4096);
	static constexpr INT clientIpBufferSize = Config.Int("network.client_ip_buffer_size", 32);

	// Compile-time verification
	static_assert(socketBufferSize > 0, "socket.buffer_size must be positive");
	static_assert(clientIpBufferSize > 0, "network.client_ip_buffer_size must be positive");
};

} // namespace highp::echo_srv
