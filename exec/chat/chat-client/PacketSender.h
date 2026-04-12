#pragma once

#include <string>

class Client;

void sendJoinRoom(Client* client, std::string_view nickname);
void sendLeave(Client* client);
void sendMessage(Client* client, std::string_view username, std::string_view message);
