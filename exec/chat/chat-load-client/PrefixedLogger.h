#pragma once

#include "logger/ILogger.h"

#include <memory>
#include <string>

class PrefixedLogger final : public highp::log::ILogger {
public:
    PrefixedLogger(std::string prefix, std::shared_ptr<highp::log::ILogger> inner)
        : _prefix(std::move(prefix)), _inner(std::move(inner)) {
    }

    void Info(std::string_view msg) override {
        _inner->Info(Format(msg));
    }

    void Debug(std::string_view msg) override {
        _inner->Debug(Format(msg));
    }

    void Warn(std::string_view msg) override {
        _inner->Warn(Format(msg));
    }

    void Error(std::string_view msg) override {
        _inner->Error(Format(msg));
    }

    void Exception(std::string_view msg, const std::exception& ex) override {
        _inner->Exception(Format(msg), ex);
    }

private:
    std::string Format(std::string_view msg) const {
        return _prefix + std::string(msg);
    }

    std::string _prefix;
    std::shared_ptr<highp::log::ILogger> _inner;
};
