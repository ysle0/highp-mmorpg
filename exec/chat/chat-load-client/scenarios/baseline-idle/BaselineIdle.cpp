#include "BaselineIdle.h"

BaselineIdle::BaselineIdle(
    ScenarioConfig config,
    std::shared_ptr<highp::log::Logger> logger,
    std::shared_ptr<highp::log::ILogger> innerLogger,
    std::shared_ptr<highp::metrics::IClientMetrics> metrics)
    : _config(std::move(config)),
      _logger(std::move(logger)),
      _innerLogger(std::move(innerLogger)),
      _metrics(std::move(metrics)) {
}

void BaselineIdle::Run() {
    const auto& connectPhase = _config.FindPhase(PhaseType::Connect);
    const auto& holdPhase = _config.FindPhase(PhaseType::Hold);

    _logger->Info("[1/4] connect ({} clients, ramp={}ms)",
                  _config.client_count, connectPhase.connect.ramp_delay_ms);
    ConnectClients(_sessions, _config, _config.client_count,
                   connectPhase.connect.ramp_delay_ms, _logger, _innerLogger, _metrics);

    _logger->Info("[2/4] join");
    JoinAll(_sessions, _logger);

    _logger->Info("[3/4] hold ({}s)", holdPhase.hold.duration_sec);
    Hold(holdPhase.hold.duration_sec, _logger);

    _logger->Info("[4/4] disconnect");
    DisconnectAll(_sessions, _logger);
}
