#include "StepConnect.h"

StepConnect::StepConnect(
    ScenarioConfig config,
    std::shared_ptr<highp::log::Logger> logger,
    std::shared_ptr<highp::log::ILogger> innerLogger,
    std::shared_ptr<highp::metrics::IClientMetrics> metrics)
    : _config(std::move(config)),
      _logger(std::move(logger)),
      _innerLogger(std::move(innerLogger)),
      _metrics(std::move(metrics)) {
}

void StepConnect::Run() {
    int phaseNum = 1;
    const int totalPhases = static_cast<int>(_config.phases.size());

    for (const auto& phase : _config.phases) {
        switch (phase.type) {
        case PhaseType::Connect: {
            int target = phase.target_count > 0 ? phase.target_count : _config.client_count;
            _logger->Info("[{}/{}] connect (target={}, ramp={}ms)",
                          phaseNum, totalPhases, target, phase.ramp_delay_ms);
            ConnectClients(_sessions, _config, target, phase.ramp_delay_ms,
                           _logger, _innerLogger, _metrics);
            break;
        }
        case PhaseType::Hold:
            _logger->Info("[{}/{}] hold ({}s)", phaseNum, totalPhases, phase.duration_sec);
            Hold(phase.duration_sec, _logger);
            break;
        case PhaseType::Disconnect:
            _logger->Info("[{}/{}] disconnect", phaseNum, totalPhases);
            DisconnectAll(_sessions, _logger);
            break;
        default:
            break;
        }
        ++phaseNum;
    }
}
