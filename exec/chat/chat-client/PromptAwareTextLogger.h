#pragma once

#include <memory>

#include <logger/ILogger.h>

class ConsoleState;

class PromptAwareTextLogger final : public highp::log::ILogger {
public:
    explicit PromptAwareTextLogger(std::shared_ptr<ConsoleState> console);

    void Info(std::string_view msg) override;
    void Debug(std::string_view msg) override;
    void Warn(std::string_view msg) override;
    void Error(std::string_view msg) override;
    void Exception(std::string_view msg, const std::exception& ex) override;

private:
    std::shared_ptr<ConsoleState> _console;
};
