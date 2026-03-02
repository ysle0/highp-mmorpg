#pragma once
// Auto-generated from: network/config.compile.toml
// Do not edit manually. Run scripts/parse_compile_cfg.ps1 to regenerate.

namespace highp::net {

/// <summary>
/// Compile-time network constants.
/// TOML sections are represented as nested structs.
/// </summary>
struct Const {
	/// <summary>[buffer] section</summary>
	struct Buffer {
		static constexpr int recvBufferSize = 4096;
		static constexpr int sendBufferSize = 1024;
		static constexpr int addressBufferSize = 64;
		static constexpr int clientIpBufferSize = 32;
		static constexpr int maxFrameSize = 65536;
	};
	/// <summary>[io] section</summary>
	struct Io {
		static constexpr int workerIoPendingCancelGracePeriodMs = 100;
	};
};

} // namespace highp::net