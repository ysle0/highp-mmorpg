#pragma once
// Auto-generated from: echo\echo-server\config.compile.toml
// Do not edit manually. Run embed_config.ps1 to regenerate.

namespace highp::echo::server::config
{

inline constexpr const char* ECHO_SERVER_CONFIG = R"TOML(
# Echo Server Compile-time Configuration
# These values are embedded at compile time for static buffer sizes.
# See config.runtime.toml for runtime-configurable values.

[socket]
buffer_size = 4096

[network]
client_ip_buffer_size = 32
)TOML";

} // namespace highp::echo::server::config
