// chat-server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include "server/PacketHandler.hpp"

using namespace highp;

int main() {
    std::cout << "Hello World!\n";

    net::PacketHandler handler(nullptr);

    handler.RegisterHandler<protocol::messages::ChatMessageBroadcast>([](
        std::shared_ptr<net::Client> c,
        const protocol::messages::ChatMessageBroadcast* p) {
            std::cout << "ChatMessageBroadcast" << std::endl;
            p->room_id();
        });

    handler.RegisterHandler<protocol::messages::JoinRoomRequest>([](
        std::shared_ptr<net::Client> c,
        const protocol::messages::JoinRoomRequest* p) {
            std::cout << "JoinRoomRequest" << std::endl;
            p->room_id();
        });
}
