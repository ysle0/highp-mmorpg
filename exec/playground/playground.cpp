// playground.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <format>
#include <flatbuf/gen/enum_generated.h>
#include <flatbuf/gen/packet_generated.h>
#include <flatbuf/gen/client_generated.h>
#include <flatbuffers/flatbuffers.h>

int main() {
	// 1. 메시지 직렬화 (클라이언트 -> 서버)
	// LoginRequest 생성.
	flatbuffers::FlatBufferBuilder builder;
	auto username = builder.CreateString("player123");
	auto loginReq = highp::protocol::messages::CreateLoginRequest(builder, username);

	// Packet 으로 감싸기.
	auto packet = highp::protocol::CreatePacket(
		builder,
		highp::protocol::MessageType::CS_Login,
		highp::protocol::Payload::messages_LoginRequest,
		loginReq.Union()
	);
	builder.Finish(packet);

	// 전송할 바이트 버퍼
	const uint8_t* buffer = builder.GetBufferPointer();
	size_t size = builder.GetSize();
	// send(socket, buffer, size, 0);

	{
		// 2. 메시지 역직렬화 (서버 -> 클라이언트)
		const uint8_t* received_buffer = buffer; // 수신된 버퍼
		auto packet = highp::protocol::GetPacket(received_buffer);

		// 메시지 타입 확인
		switch (packet->type()) {
			case highp::protocol::MessageType::CS_Login:
			if (auto req = packet->payload_as_messages_LoginRequest()) {
				std::string_view username = req->username()->string_view();
				std::cout << std::format("Login request received for user: {}\n", username);
				std::string username_str = req->username()->str();
				std::cout << std::format("Login request received for user: {}\n", username);
				// 로그인 처리
			}
			break;

			case highp::protocol::MessageType::CS_Logout:
			if (auto r = packet->payload_as_messages_LeaveRoomRequest()) {
				const auto roomId = r->room_id();
				// 로그아웃 처리
			}
			break;
		}
	}

	// 3. 에러 응답 보내기
	{
		flatbuffers::FlatBufferBuilder builder;
		auto errorMsg = builder.CreateString("Room not found");
		auto errorResp = highp::protocol::CreateErrorResponse(
			builder,
			highp::protocol::ErrorCode::RoomNotFound,
			errorMsg);

		auto packet = highp::protocol::CreatePacket(
			builder,
			highp::protocol::MessageType::SC_Error,
			highp::protocol::Payload::ErrorResponse,
			errorResp.Union());
		builder.Finish(packet);
	}
}

