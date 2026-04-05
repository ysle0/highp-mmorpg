#include "ChatCli.h"

#include <format>
#include <iostream>
#include <unordered_map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

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

void ConsoleState::Log(std::string_view level, std::string_view message) {
    std::scoped_lock lock{_mtx};

    if (_promptVisible) {
        ClearPromptLineUnlocked();
    }

    std::cout << '[' << level << "] " << message << std::endl;

    if (_promptVisible) {
        PrintPromptUnlocked();
    }
}

void ConsoleState::ShowPrompt(std::string prompt) {
    std::scoped_lock lock{_mtx};

    _prompt = std::move(prompt);
    _promptVisible = true;
    PrintPromptUnlocked();
}

void ConsoleState::HidePrompt() {
    std::scoped_lock lock{_mtx};
    _promptVisible = false;
}

void ConsoleState::ClearPromptLineUnlocked() const {
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (consoleHandle == INVALID_HANDLE_VALUE || consoleHandle == nullptr) {
        std::cout << '\r';
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO bufferInfo{};
    if (!GetConsoleScreenBufferInfo(consoleHandle, &bufferInfo)) {
        std::cout << '\r';
        return;
    }

    COORD lineStart{0, bufferInfo.dwCursorPosition.Y};
    DWORD written = 0;

    FillConsoleOutputCharacterA(
        consoleHandle,
        ' ',
        static_cast<DWORD>(bufferInfo.dwSize.X),
        lineStart,
        &written);
    FillConsoleOutputAttribute(
        consoleHandle,
        bufferInfo.wAttributes,
        static_cast<DWORD>(bufferInfo.dwSize.X),
        lineStart,
        &written);
    SetConsoleCursorPosition(consoleHandle, lineStart);
}

void ConsoleState::PrintPromptUnlocked() const {
    std::cout << "[INFO] " << _prompt << std::flush;
}

ChatCli::ChatCli(
    Client* client,
    std::shared_ptr<highp::log::Logger> logger,
    std::shared_ptr<ConsoleState> console
)
    : _client(client),
      _logger(std::move(logger)),
      _console(std::move(console)) {
    //
}

void ChatCli::PromptNickname() {
    _console->ShowPrompt("Enter nickname: ");
    std::getline(std::cin, _nickname);
    _console->HidePrompt();

    if (_nickname.empty()) {
        _nickname = "guest";
    }

    _logger->Info("Nickname set to '{}'", _nickname);
}

void ChatCli::Run() {
    PrintHelp();

    while (true) {
        PrintPrompt();

        std::string line;
        if (!std::getline(std::cin, line)) {
            _console->HidePrompt();
            break;
        }

        _console->HidePrompt();

        if (line.empty()) {
            continue;
        }

        const bool startWithSlash = line[0] == '/';
        if (const Action action = startWithSlash ? HandleCommand(line) : HandleChat(line);
            action == Action::Quit
        ) {
            break;
        }
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
    _console->ShowPrompt(BuildPrompt());
}

std::string ChatCli::BuildPrompt() const {
    if (_joined) {
        return std::format("[{}] > ", _nickname);
    }

    return "[not joined] > ";
}
