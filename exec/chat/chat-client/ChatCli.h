#pragma once

#include <mutex>
#include <string>
#include <logger/Logger.hpp>

class Client;

class ConsoleState final {
public:
    void Log(std::string_view level, std::string_view message);
    void ShowPrompt(std::string prompt);
    void HidePrompt();

private:
    void ClearPromptLineUnlocked() const;
    void PrintPromptUnlocked() const;

private:
    mutable std::mutex _mtx;
    std::string _prompt;
    bool _promptVisible = false;
};

class ChatCli final {
public:
    explicit ChatCli(
        Client* client,
        std::shared_ptr<highp::log::Logger> logger,
        std::shared_ptr<ConsoleState> console
    );

    /// 닉네임 입력 프롬프트. 빈 입력 시 "guest".
    void PromptNickname();

    /// 메인 입력 루프. /quit 또는 EOF 시 반환.
    void Run();

private:
    enum class Action { Continue, Quit };

    Action HandleCommand(const std::string& line);
    Action HandleChat(const std::string& line) const;

    void PrintHelp() const;
    void PrintPrompt() const;
    [[nodiscard]] std::string BuildPrompt() const;

private:
    Client* _client;
    std::shared_ptr<highp::log::Logger> _logger;
    std::shared_ptr<ConsoleState> _console;
    std::string _nickname;
    bool _joined = false;
};
