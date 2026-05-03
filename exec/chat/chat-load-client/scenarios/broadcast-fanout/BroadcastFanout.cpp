#include "BroadcastFanout.h"

BroadcastFanout::BroadcastFanout(
    ScenarioConfig config,
    std::shared_ptr<highp::log::Logger> logger,
    std::shared_ptr<highp::log::ILogger> innerLogger,
    std::shared_ptr<highp::metrics::IClientMetrics> metrics)
    : _config(std::move(config)),
      _logger(std::move(logger)),
      _innerLogger(std::move(innerLogger)),
      _metrics(std::move(metrics)) {
}

void BroadcastFanout::Run() {
    const PhaseConfig& connectPhase = _config.FindPhase(PhaseType::Connect);
    const PhaseConfig& rolePhase = _config.FindPhase(PhaseType::RoleSend);

    _logger->Info("[1/4] connect ({} clients, ramp={}ms)",
                  _config.client_count, connectPhase.connect.ramp_delay_ms);
    ConnectClients(_sessions, _config, _config.client_count,
                   connectPhase.connect.ramp_delay_ms, _logger, _innerLogger, _metrics);

    _logger->Info("[2/4] join");
    JoinAll(_sessions, _logger);

    _logger->Info("[3/4] role_send ({}s, senders={}, interval={}ms, size={}B)",
                  rolePhase.role_send.duration_sec, rolePhase.role_send.sender_count,
                  rolePhase.role_send.send_interval_ms, rolePhase.role_send.message_size_bytes);
    RoleSend(_sessions, rolePhase.role_send.sender_count, rolePhase.role_send.duration_sec,
             rolePhase.role_send.send_interval_ms, rolePhase.role_send.message_size_bytes, _logger);

    _logger->Info("[4/4] disconnect");
    DisconnectAll(_sessions, _logger);
}
