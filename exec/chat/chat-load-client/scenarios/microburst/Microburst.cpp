#include "Microburst.h"

Microburst::Microburst(
    ScenarioConfig config,
    std::shared_ptr<highp::log::Logger> logger,
    std::shared_ptr<highp::log::ILogger> innerLogger,
    std::shared_ptr<highp::metrics::IClientMetrics> metrics)
    : _config(std::move(config)),
      _logger(std::move(logger)),
      _innerLogger(std::move(innerLogger)),
      _metrics(std::move(metrics)) {
}

void Microburst::Run() {
    const auto& connectPhase = _config.FindPhase(PhaseType::Connect);
    const auto& burstPhase = _config.FindPhase(PhaseType::BurstSend);

    _logger->Info("[1/4] connect ({} clients, ramp={}ms)",
                  _config.client_count, connectPhase.connect.ramp_delay_ms);
    ConnectClients(_sessions, _config, _config.client_count,
                   connectPhase.connect.ramp_delay_ms, _logger, _innerLogger, _metrics);

    _logger->Info("[2/4] join");
    JoinAll(_sessions, _logger);

    _logger->Info("[3/4] burst_send (waves={}, msgs/burst={}, idle={}ms, size={}B)",
                  burstPhase.burst_send.burst_count, burstPhase.burst_send.burst_messages,
                  burstPhase.burst_send.idle_window_ms, burstPhase.burst_send.message_size_bytes);
    BurstSend(_sessions, burstPhase.burst_send.burst_count, burstPhase.burst_send.burst_messages,
              burstPhase.burst_send.idle_window_ms, burstPhase.burst_send.message_size_bytes, _logger);

    _logger->Info("[4/4] disconnect");
    DisconnectAll(_sessions, _logger);
}
