#include "MalformedPacket.h"

MalformedPacket::MalformedPacket(
    ScenarioConfig config,
    std::shared_ptr<highp::log::Logger> logger,
    std::shared_ptr<highp::log::ILogger> innerLogger,
    std::shared_ptr<highp::metrics::IClientMetrics> metrics)
    : _config(std::move(config)),
      _logger(std::move(logger)),
      _innerLogger(std::move(innerLogger)),
      _metrics(std::move(metrics)) {
}

void MalformedPacket::Run() {
    const auto& connectPhase = _config.FindPhase(PhaseType::Connect);
    const auto& malformedPhase = _config.FindPhase(PhaseType::MalformedSend);

    _logger->Info("[1/4] connect ({} clients, ramp={}ms)",
                  _config.client_count, connectPhase.connect.ramp_delay_ms);
    ConnectClients(_sessions, _config, _config.client_count,
                   connectPhase.connect.ramp_delay_ms, _logger, _innerLogger, _metrics);

    _logger->Info("[2/4] join");
    JoinAll(_sessions, _logger);

    _logger->Info("[3/4] malformed_send ({}s, interval={}ms, size={}B, malformed={}%)",
                  malformedPhase.malformed_send.duration_sec,
                  malformedPhase.malformed_send.send_interval_ms,
                  malformedPhase.malformed_send.message_size_bytes,
                  malformedPhase.malformed_send.malformed_percent);
    MalformedSend(_sessions, malformedPhase.malformed_send.duration_sec,
                  malformedPhase.malformed_send.send_interval_ms,
                  malformedPhase.malformed_send.message_size_bytes,
                  malformedPhase.malformed_send.malformed_percent, _logger);

    _logger->Info("[4/4] disconnect");
    DisconnectAll(_sessions, _logger);
}
