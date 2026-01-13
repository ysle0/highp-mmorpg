#pragma once

#include <filesystem>
#include <runTime/Config.hpp>
#include <stdexcept>
#include <thread>
#include <Windows.h>

namespace highp::network {

// =============================================================================
// Runtime Config
// =============================================================================

/// <summary>
/// 서버 런타임 설정 구조체.
/// 설정 파일 또는 기본값에서 런타임에 로드되며, 서버 동작 파라미터를 정의한다.
/// </summary>
struct RuntimeCfg {
	/// <summary>서버 리스닝 포트 번호 (1-65535)</summary>
	INT port;

	/// <summary>최대 동시 클라이언트 연결 수</summary>
	INT maxClients;

	/// <summary>Listen 대기 큐 크기. AcceptEx 사전 등록 수에도 사용.</summary>
	INT backlog;

	/// <summary>IOCP Worker 스레드 수</summary>
	INT workerThreadCount;

	/// <summary>
	/// 기본값 상수 모음.
	/// </summary>
	struct Defaults {
		/// <summary>기본 포트: 8080</summary>
		static constexpr INT port = 8080;

		/// <summary>기본 최대 클라이언트: 1000</summary>
		static constexpr INT maxClients = 1000;

		/// <summary>기본 backlog: 10</summary>
		static constexpr INT backlog = 10;

		/// <summary>기본 Worker 스레드 수: -1 (CPU 코어 수 자동 감지)</summary>
		static constexpr INT workerThreadCount = -1;
	};

	/// <summary>
	/// 설정 파일에서 RuntimeCfg를 로드한다.
	/// </summary>
	/// <param name="path">설정 파일 경로</param>
	/// <returns>로드된 RuntimeCfg 인스턴스</returns>
	/// <exception cref="std::runtime_error">파일 로드 실패 시</exception>
	static RuntimeCfg FromFile(const std::filesystem::path& path) {
		auto cfg = highp::config::Config::FromFile(path);
		if (!cfg.has_value()) {
			throw std::runtime_error("Failed to load config file: " + path.string());
		}
		return FromCfg(cfg.value());
	}

	/// <summary>
	/// Config 객체에서 RuntimeCfg를 생성한다.
	/// </summary>
	/// <param name="cfg">highp::config::Config 인스턴스</param>
	/// <returns>생성된 RuntimeCfg 인스턴스</returns>
	/// <remarks>
	/// workerThreadCount가 0 이하이면 CPU 코어 수로 자동 설정.
	/// 환경 변수 오버라이드 지원: SERVER_PORT, SERVER_MAX_CLIENTS, SERVER_BACKLOG, SERVER_WORKER_COUNT
	/// </remarks>
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

	/// <summary>
	/// 기본값으로 RuntimeCfg를 생성한다.
	/// </summary>
	/// <returns>기본값이 적용된 RuntimeCfg 인스턴스</returns>
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
	/// <summary>
	/// 설정값 유효성 검증.
	/// </summary>
	/// <exception cref="std::invalid_argument">유효하지 않은 값 발견 시</exception>
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

} // namespace highp::network
