#pragma once
// Auto-generated from: network/config.compile.toml
// Do not edit manually. Run scripts/parse_compile_cfg.ps1 to regenerate.

#include <Windows.h>

namespace highp::network {

/// <summary>
/// Compile-time network constants.
/// TOML sections are represented as nested structs.
/// </summary>
struct Const {
	/// <summary>[buffer] section</summary>
	struct Buffer {
		static constexpr INT recvBufferSize = 4096;
		static constexpr INT sendBufferSize = 1024;
		static constexpr INT addressBufferSize = 64;
		static constexpr INT clientIpBufferSize = 32;
	};
};

} // namespace highp::network