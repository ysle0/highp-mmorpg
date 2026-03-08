#pragma once
// Auto-generated from: exec/echo/echo-server/config.runtime.toml
// Do not edit manually. Run scripts/parse_network_cfg.ps1 to regenerate.

#include <filesystem>
#include <config/runTime/Config.hpp>
#include <stdexcept>
#include <string>

namespace highp::net {

/// <summary>
/// Runtime network configuration.
/// TOML sections are represented as nested structs.
/// </summary>
struct NetworkCfg {
	/// <summary>[server] section</summary>
	struct Server {
        int port;
        int maxClients;
        int backlog;
        int tickMs;

		struct Defaults {
			static constexpr int port = 8080;
			static constexpr int maxClients = 1000;
			static constexpr int backlog = 10;
			static constexpr int tickMs = 16;
		};
	} server;
	/// <summary>[thread] section</summary>
	struct Thread {
        int maxWorkerThreadMultiplier;
        int maxAcceptorThreadCount;
        int desirableIocpThreadCount;

		struct Defaults {
			static constexpr int maxWorkerThreadMultiplier = 2;
			static constexpr int maxAcceptorThreadCount = 2;
			static constexpr int desirableIocpThreadCount = -1;
		};
	} thread;

	/// <summary>Load from TOML file</summary>
	static NetworkCfg FromFile(const std::filesystem::path& path) {
		auto cfg = highp::config::Config::FromFile(path);
		if (!cfg.has_value()) {
			throw std::runtime_error("Failed to load config file: " + path.string());
		}
		return FromCfg(cfg.value());
	}

	/// <summary>Load from Config object</summary>
	static NetworkCfg FromCfg(const highp::config::Config& cfg) {
		return NetworkCfg{
			.server = {
				.port = cfg.Int("server.port", Server::Defaults::port, "SERVER_PORT"),
				.maxClients = cfg.Int("server.max_clients", Server::Defaults::maxClients, "SERVER_MAX_CLIENTS"),
				.backlog = cfg.Int("server.backlog", Server::Defaults::backlog, "SERVER_BACKLOG"),
				.tickMs = cfg.Int("server.tick_ms", Server::Defaults::tickMs, "SERVER_TICK_MS"),
			},
			.thread = {
				.maxWorkerThreadMultiplier = cfg.Int("thread.max_worker_thread_multiplier", Thread::Defaults::maxWorkerThreadMultiplier, "THREAD_MAX_WORKER_THREAD_MULTIPLIER"),
				.maxAcceptorThreadCount = cfg.Int("thread.max_acceptor_thread_count", Thread::Defaults::maxAcceptorThreadCount, "THREAD_MAX_ACCEPTOR_THREAD_COUNT"),
				.desirableIocpThreadCount = cfg.Int("thread.desirable_iocp_thread_count", Thread::Defaults::desirableIocpThreadCount, "THREAD_DESIRABLE_IOCP_THREAD_COUNT"),
			},
		};
	}

	/// <summary>Create with default values</summary>
	static NetworkCfg WithDefaults() {
		return NetworkCfg{
			.server = {
				.port = Server::Defaults::port,
				.maxClients = Server::Defaults::maxClients,
				.backlog = Server::Defaults::backlog,
				.tickMs = Server::Defaults::tickMs,
			},
			.thread = {
				.maxWorkerThreadMultiplier = Thread::Defaults::maxWorkerThreadMultiplier,
				.maxAcceptorThreadCount = Thread::Defaults::maxAcceptorThreadCount,
				.desirableIocpThreadCount = Thread::Defaults::desirableIocpThreadCount,
			},
		};
	}
};

} // namespace highp::net