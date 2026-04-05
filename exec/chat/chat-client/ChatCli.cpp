#include "ChatCli.h"

#include <iostream>
#include <unordered_map>
#include "Client.h"
#include "PacketSender.h"

namespace {
    enum class Command { Quit, Help, Join, Leave, Unknown };

    Command parseCommand(const std::string& cmd) {
        static const std::unordered_map<std::string, Command> kCommands = {
            {"/quit", Command::Quit},
            {"/help", Command::Help},
            {"/join", Command::Join},
            {"/leave", Command::Leave},
        };

        const std::unordered_map<std::string, Command>::const_iterator it = kCommands.find(cmd);
        if (it != kCommands.end()) {
            return it->second;
        }
        return Command::Unknown;
    }
} // namespace

ChatCli::ChatCli(Client* client, std::shared_ptr<highp::log::Logger> logger)
    : _client(client),
      _logger(std::move(logger)) {
    //
}

void ChatCli::PromptNickname() {
    _logger->Info("Enter nickname: ");

    std::getline(std::cin, _nickname);

    if (_nickname.empty()) {
        _nickname = "guest";
    }

    _logger->Info("Nickname set to '{}'", _nickname);
}

void ChatCli::Run() {
    PrintHelp();
    PrintPrompt();

    std::string line;
    
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            PrintPrompt();
            continue;
        }

        const bool startWithSlash = line[0] == '/';
        if (const Action action = startWithSlash ? HandleCommand(line) : HandleChat(line);
            action == Action::Quit
        ) {
            break;
        }

        PrintPrompt();
    }
}

ChatCli::Action ChatCli::HandleCommand(const std::string& line) {
    switch (parseCommand(line)) {
    case Command::Quit:
        return Action::Quit;

    case Command::Help:
        PrintHelp();
        break;

    case Command::Join:
        if (_joined) {
            _logger->Warn("Already joined.");
        }
        else {
            sendJoinRoom(_client, _nickname);
            _joined = true;
            _logger->Info("'{}' joined the server", _nickname);
        }
        break;

    case Command::Leave:
        if (!_joined) {
            _logger->Warn("Not joined.");
        }
        else {
            sendLeave(_client);
            _joined = false;
            _logger->Info("'{}' left the server", _nickname);
        }
        break;

    case Command::Unknown:
        _logger->Warn("Unknown command. Type /help for usage.");
        break;
    }

    return Action::Continue;
}

ChatCli::Action ChatCli::HandleChat(const std::string& line) const {
    if (!_joined) {
        _logger->Warn("Not joined. Use /join first.");
    }
    else {
        sendMessage(_client, _nickname, line);
    }
    return Action::Continue;
}

void ChatCli::PrintHelp() const {
    _logger->Info("Commands:");
    _logger->Info("  /join   - Join the server");
    _logger->Info("  /leave  - Leave the server");
    _logger->Info("  /help   - Show this help");
    _logger->Info("  /quit   - Disconnect and exit");
    _logger->Info("  <text>  - Send chat message");
}

void ChatCli::PrintPrompt() const {
    if (_joined) {
        _logger->Info("[{}] > ", _nickname);
    }
    else {
        _logger->Info("[not joined] > ");
    }
}
