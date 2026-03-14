#pragma once

#include <cstdint>
#include <string>

class Client;

void SendJoinRoom(Client& client, uint32_t roomId);
void SendLeaveRoom(Client& client, uint32_t roomId);
void SendMessage(Client& client, uint32_t roomId, const std::string& message);
