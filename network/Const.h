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
	/// <summary>[network] section</summary>
	struct Network {
		static constexpr INT clientIpBufferSize = 32;
		static constexpr INT workerIoPendingCancelGracePeriodMs = 100;
	};
	/// <summary>[socket] section</summary>
	struct Socket {
		static constexpr INT bufferSize = 4096;
		static constexpr INT recvBufferSize = 4096;
		static constexpr INT sendBufferSize = 1024;
	};
};

} // namespace highp::network