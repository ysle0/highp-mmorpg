#pragma once

#include <filesystem>
#include <runTime/Config.h>
#include <stdexcept>
#include <thread>
#include <Windows.h>

namespace highp::echo_srv {
// =============================================================================
// Runtime Config
// =============================================================================
struct RuntimeCfg {
	INT port;
	INT maxClients;
	INT backlog;
	INT workerThreadCount;

	// Default values
	struct Defaults {
		static constexpr INT port = 8080;
		static constexpr INT maxClients = 1000;
		static constexpr INT backlog = 10;
		static constexpr INT workerThreadCount = -1;  // -1 = CPU core count
	};

	// Load from file with validation
	static RuntimeCfg FromFile(const std::filesystem::path& path) {
		auto cfg = highp::config::Config::FromFile(path);
		if (!cfg.has_value()) {
			throw std::runtime_error("Failed to load config file: " + path.string());
		}
		return FromCfg(cfg.value());
	}

	// Load from Config object
	static RuntimeCfg FromCfg(const highp::config::Config& cfg) {
		INT threadCount = cfg.Int("thread.worker_count", Defaults::workerThreadCount, "SERVER_WORKER_COUNT");
		if (threadCount <= 0) {
			threadCount = static_cast<INT>(std::thread::hardware_concurrency());
			if (threadCount <= 0) threadCount = 4;  // fallback
		}

		RuntimeCfg result{
			.port = cfg.Int("server.port", Defaults::port, "SERVER_PORT"),
			.maxClients = cfg.Int("server.max_clients", Defaults::maxClients, "SERVER_MAX_CLIENTS"),
			.backlog = cfg.Int("server.backlog", Defaults::backlog, "SERVER_BACKLOG"),
			.workerThreadCount = threadCount
		};
		result.Validate();
		return result;
	}

	// Create with default values
	static RuntimeCfg WithDefaults() {
		INT threadCount = Defaults::workerThreadCount;
		if (threadCount <= 0) {
			threadCount = static_cast<INT>(std::thread::hardware_concurrency());
			if (threadCount <= 0) threadCount = 4;  // fallback
		}
		return RuntimeCfg{
			.port = Defaults::port,
			.maxClients = Defaults::maxClients,
			.backlog = Defaults::backlog,
			.workerThreadCount = threadCount
		};
	}

private:
	void Validate() const {
		if (port <= 0 || port > 65535) {
			throw std::invalid_argument("server.port must be between 1 and 65535");
		}
		if (maxClients <= 0) {
			throw std::invalid_argument("server.max_clients must be positive");
		}
		if (backlog <= 0) {
			throw std::invalid_argument("server.backlog must be positive");
		}
		if (workerThreadCount <= 0) {
			throw std::invalid_argument("thread.worker_count must be positive (resolved before Validate)");
		}
	}
};
} // namespace highp::echo_srv
