#include "SteadySmallTraffic.h"

SteadySmallTraffic::SteadySmallTraffic(
    ScenarioConfig config,
    std::shared_ptr<highp::log::Logger> logger,
    std::shared_ptr<highp::log::ILogger> innerLogger,
    std::shared_ptr<highp::metrics::IClientMetrics> metrics)
    : _config(std::move(config)),
      _logger(std::move(logger)),
      _innerLogger(std::move(innerLogger)),
      _metrics(std::move(metrics)) {
}

void SteadySmallTraffic::Run() {
    const auto& connectPhase = _config.FindPhase(PhaseType::Connect);
    const auto& sendPhase = _config.FindPhase(PhaseType::SteadySend);

    _logger->Info("[1/4] connect ({} clients, ramp={}ms)",
                  _config.client_count, connectPhase.ramp_delay_ms);
    ConnectClients(_sessions, _config, _config.client_count,
                   connectPhase.ramp_delay_ms, _logger, _innerLogger, _metrics);

    _logger->Info("[2/4] join");
    JoinAll(_sessions, _logger);

    _logger->Info("[3/4] steady_send ({}s, interval={}ms, size={}B)",
                  sendPhase.duration_sec, sendPhase.send_interval_ms, sendPhase.message_size_bytes);
    SteadySend(_sessions, sendPhase.duration_sec, sendPhase.send_interval_ms,
               sendPhase.message_size_bytes, _logger);

    _logger->Info("[4/4] disconnect");
    DisconnectAll(_sessions, _logger);
}
