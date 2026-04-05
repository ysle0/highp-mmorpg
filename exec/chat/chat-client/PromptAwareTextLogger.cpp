#include "PromptAwareTextLogger.h"

#include <format>
#include <utility>

#include "ChatCli.h"

PromptAwareTextLogger::PromptAwareTextLogger(std::shared_ptr<ConsoleState> console)
    : _console(std::move(console)) {
}

void PromptAwareTextLogger::Info(std::string_view msg) {
    _console->Log("INFO", msg);
}

void PromptAwareTextLogger::Debug(std::string_view msg) {
    _console->Log("DEBUG", msg);
}

void PromptAwareTextLogger::Warn(std::string_view msg) {
    _console->Log("WARN", msg);
}

void PromptAwareTextLogger::Error(std::string_view msg) {
    _console->Log("ERROR", msg);
}

void PromptAwareTextLogger::Exception(std::string_view msg, const std::exception& ex) {
    _console->Log("EXCEPTION", std::format("{}: {}", msg, ex.what()));
}
