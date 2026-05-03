#include "ReconnectStorm.h"

ReconnectStorm::ReconnectStorm(
    ScenarioConfig config,
    std::shared_ptr<highp::log::Logger> logger,
    std::shared_ptr<highp::log::ILogger> innerLogger,
    std::shared_ptr<highp::metrics::IClientMetrics> metrics)
    : _config(std::move(config)),
      _logger(std::move(logger)),
      _innerLogger(std::move(innerLogger)),
      _metrics(std::move(metrics)) {
}

void ReconnectStorm::Run() {
    const auto& connectPhase = _config.FindPhase(PhaseType::Connect);
    const auto& reconnectPhase = _config.FindPhase(PhaseType::ReconnectWave);

    _logger->Info("[1/4] connect ({} clients, ramp={}ms)",
                  _config.client_count, connectPhase.connect.ramp_delay_ms);
    ConnectClients(_sessions, _config, _config.client_count,
                   connectPhase.connect.ramp_delay_ms, _logger, _innerLogger, _metrics);

    _logger->Info("[2/4] join");
    JoinAll(_sessions, _logger);

    _logger->Info("[3/4] reconnect_wave (waves={}, hold={}s)",
                  reconnectPhase.reconnect_wave.wave_count,
                  reconnectPhase.reconnect_wave.hold_between_wave_sec);
    ReconnectWave(_sessions, _config, reconnectPhase.reconnect_wave.wave_count,
                  reconnectPhase.reconnect_wave.hold_between_wave_sec,
                  _logger, _innerLogger, _metrics);

    _logger->Info("[4/4] disconnect");
    DisconnectAll(_sessions, _logger);
}
