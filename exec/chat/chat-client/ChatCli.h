#pragma once

#include <string>
#include <logger/Logger.hpp>

class Client;

class ChatCli final {
public:
    explicit ChatCli(
        Client* client,
        std::shared_ptr<highp::log::Logger> logger
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

private:
    Client* _client;
    std::shared_ptr<highp::log::Logger> _logger;
    std::string _nickname;
    bool _joined = false;
};
