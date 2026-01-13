#pragma once

#include <compileTime/Config.hpp>
#include <Windows.h>
#include "ConfigEmbedded.h"

namespace highp::network {

// =============================================================================
// Compile-time Config
// =============================================================================

/// <summary>컴파일 타임 설정 객체. ConfigEmbedded.h의 NETWORK_CONFIG에서 로드.</summary>
inline constexpr auto Config = highp::config::CompileTimeConfig<>::From(
	highp::network::NETWORK_CONFIG);

/// <summary>
/// 네트워크 라이브러리에서 사용하는 컴파일 타임 상수 모음.
/// 설정 파일에서 값을 읽어 컴파일 타임에 결정된다.
/// </summary>
struct Const {
	/// <summary>소켓 I/O 버퍼 크기 (바이트). OverlappedExt::buffer 크기에 사용.</summary>
	static constexpr INT socketBufferSize = Config.Int("socket.buffer_size", 4096);

	/// <summary>클라이언트 IP 문자열 버퍼 크기. inet_ntop() 결과 저장용.</summary>
	static constexpr INT clientIpBufferSize = Config.Int("network.client_ip_buffer_size", 32);

	// Compile-time verification
	static_assert(socketBufferSize > 0, "socket.buffer_size must be positive");
	static_assert(clientIpBufferSize > 0, "network.client_ip_buffer_size must be positive");
};

} // namespace highp::network
