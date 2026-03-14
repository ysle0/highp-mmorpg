#include "ChatCli.h"

#include <iostream>
#include <sstream>
#include "Client.h"
#include "PacketSender.h"

ChatCli::ChatCli(Client& client, std::shared_ptr<highp::log::Logger> logger)
    : _client(client), _logger(std::move(logger)) {
}

void ChatCli::PromptNickname() {
    std::cout << "Enter nickname: " << std::flush;
    std::getline(std::cin, _nickname);
    if (_nickname.empty()) _nickname = "guest";
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

        Action action = (line[0] == '/')
            ? HandleCommand(line)
            : HandleChat(line);

        if (action == Action::Quit)
            break;

        PrintPrompt();
    }
}

ChatCli::Action ChatCli::HandleCommand(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "/quit") {
        return Action::Quit;
    }
    else if (cmd == "/help") {
        PrintHelp();
    }
    else if (cmd == "/join") {
        uint32_t roomId;
        if (!(iss >> roomId)) {
            std::cout << "Usage: /join <room_id>\n";
        }
        else {
            if (_currentRoom) {
                _logger->Info("Leaving room {} before joining room {}", *_currentRoom, roomId);
                SendLeaveRoom(_client, *_currentRoom);
            }
            SendJoinRoom(_client, roomId);
            _currentRoom = roomId;
            _logger->Info("'{}' joined room {}", _nickname, roomId);
        }
    }
    else if (cmd == "/leave") {
        if (!_currentRoom) {
            std::cout << "Not in a room.\n";
        }
        else {
            _logger->Info("'{}' leaving room {}", _nickname, *_currentRoom);
            SendLeaveRoom(_client, *_currentRoom);
            _currentRoom.reset();
        }
    }
    else if (cmd == "/nick") {
        std::string newNick;
        if (!(iss >> newNick) || newNick.empty()) {
            std::cout << "Usage: /nick <name>\n";
        }
        else {
            _logger->Info("Nickname changed: '{}' -> '{}'", _nickname, newNick);
            _nickname = newNick;
        }
    }
    else {
        std::cout << "Unknown command. Type /help for usage.\n";
    }

    return Action::Continue;
}

ChatCli::Action ChatCli::HandleChat(const std::string& line) {
    if (!_currentRoom) {
        std::cout << "Not in a room. Use /join <room_id> first.\n";
    }
    else {
        SendMessage(_client, *_currentRoom, line);
    }
    return Action::Continue;
}

void ChatCli::PrintHelp() const {
    std::cout << "Commands:\n"
        << "  /join <room_id>  - Join a room\n"
        << "  /leave           - Leave current room\n"
        << "  /nick <name>     - Change nickname\n"
        << "  /help            - Show this help\n"
        << "  /quit            - Disconnect and exit\n"
        << "  <text>           - Send chat message to current room\n";
}

void ChatCli::PrintPrompt() const {
    if (_currentRoom) {
        std::cout << "[" << _nickname << "@room:" << *_currentRoom << "] > " << std::flush;
    }
    else {
        std::cout << "[" << _nickname << "] > " << std::flush;
    }
}
