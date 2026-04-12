#pragma once

#include "metrics/client/IClientMetrics.h"

#include <memory>
#include <string>

struct ResultCollector {
    // Returns the directory path where results were written
    [[nodiscard]] static std::string WriteJson(
        const std::string& scenarioName,
        const std::shared_ptr<highp::metrics::IClientMetrics>& metrics,
        int durationSec);
};
