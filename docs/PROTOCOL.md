# Protocol Documentation

> **Last Updated:** 2026-02-05
> **Version:** 1.0
> **Status:** Foundation Phase - FlatBuffers Schema Complete

---

## Table of Contents

1. [Protocol Overview](#protocol-overview)
2. [Design Philosophy](#design-philosophy)
3. [FlatBuffers Integration](#flatbuffers-integration)
4. [Message Type Catalog](#message-type-catalog)
5. [Request/Response Patterns](#requestresponse-patterns)
6. [Error Handling](#error-handling)
7. [Message Structure and Format](#message-structure-and-format)
8. [Code Generation Workflow](#code-generation-workflow)
9. [Usage Examples](#usage-examples)
10. [Protocol Evolution and Versioning](#protocol-evolution-and-versioning)
11. [Performance Considerations](#performance-considerations)
12. [Best Practices](#best-practices)

---

## Protocol Overview

The highp-mmorpg protocol is a **binary, schema-based communication protocol** built on Google FlatBuffers. It provides type-safe, efficient serialization for client-server and server-server communication in a high-performance MMORPG environment.

### Key Features

- **Zero-Copy Deserialization**: FlatBuffers allows direct access to serialized data without parsing
- **Type Safety**: Strong typing with generated C++ classes
- **Schema Evolution**: Forward and backward compatibility support
- **Compact Binary Format**: Smaller than JSON/XML, no parsing overhead
- **Cross-Platform**: Works on Windows, Linux, and other platforms
- **Validation**: Built-in schema validation at runtime

### Protocol Characteristics

| Aspect | Details |
|--------|---------|
| **Encoding** | Binary (FlatBuffers) |
| **Endianness** | Little-endian (FlatBuffers default) |
| **Schema Language** | FlatBuffers IDL (.fbs files) |
| **File Identifier** | `"CHAT"` (4 bytes) |
| **Root Type** | `Packet` |
| **Namespace** | `highp.protocol` |

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                      Application Layer                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ Login Logic  │  │ Room Logic   │  │ Chat Logic   │          │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘          │
└─────────┼──────────────────┼──────────────────┼─────────────────┘
          │                  │                  │
┌─────────┼──────────────────┼──────────────────┼─────────────────┐
│         │        Protocol Layer (FlatBuffers)                   │
│  ┌──────▼──────────────────▼──────────────────▼──────┐          │
│  │                  Packet Router                    │          │
│  │  (Deserialize, Validate, Dispatch by MessageType)│          │
│  └──────┬──────────────────┬──────────────────┬──────┘          │
│         │                  │                  │                  │
│  ┌──────▼──────┐  ┌────────▼────────┐  ┌──────▼──────┐          │
│  │   Client    │  │     Server      │  │  Broadcast  │          │
│  │  Messages   │  │    Messages     │  │  Messages   │          │
│  │  (Request)  │  │   (Response)    │  │   (Push)    │          │
│  └─────────────┘  └─────────────────┘  └─────────────┘          │
└─────────────────────────────────────────────────────────────────┘
          │                  │                  │
┌─────────┼──────────────────┼──────────────────┼─────────────────┐
│         │         Network Layer (IOCP)                          │
│  ┌──────▼──────────────────▼──────────────────▼──────┐          │
│  │        Binary Stream (WSASend / WSARecv)         │          │
│  └──────────────────────────────────────────────────┘          │
└─────────────────────────────────────────────────────────────────┘
```

---

## Design Philosophy

### 1. Performance First

The protocol is designed for high-throughput, low-latency scenarios:

- **Zero-Copy**: FlatBuffers data can be accessed directly without unpacking
- **No Parsing Overhead**: Unlike JSON/XML, no parsing step required
- **Compact Size**: Binary format is significantly smaller than text protocols
- **Cache-Friendly**: Flat memory layout improves CPU cache utilization

### 2. Type Safety and Compile-Time Validation

Schema-driven development ensures correctness:

- **Generated Code**: All message types are generated from schemas
- **Compile-Time Checks**: Invalid field access caught at compile time
- **Required Fields**: Schema enforces required fields
- **Type Checking**: Strong typing prevents message corruption

### 3. Schema Evolution and Compatibility

Support for protocol upgrades without breaking existing clients:

- **Backward Compatibility**: Old clients work with new servers
- **Forward Compatibility**: New clients gracefully handle old servers
- **Optional Fields**: Default values for new fields
- **Deprecation**: Fields can be marked deprecated without removal

### 4. Clear Request/Response Semantics

Predictable communication patterns:

- **Client-Server RPC**: Request/response pairs (e.g., Login → LoginResult)
- **Server Broadcasts**: Push notifications (e.g., UserJoined, ChatMessage)
- **Error Handling**: Standardized error responses with codes and messages

### 5. Separation of Concerns

Schema organization mirrors architectural layers:

```
protocol/flatbuf/schemas/
├── enum.fbs              # Enumerations (MessageType, ErrorCode, UserStatus)
├── common.fbs            # Shared types (Timestamp)
├── user.fbs              # User domain objects
├── room.fbs              # Room domain objects
├── error.fbs             # Error structures
├── packet.fbs            # Top-level packet wrapper
└── messages/
    ├── client.fbs        # Client → Server requests
    ├── server.fbs        # Server → Client responses
    └── broadcast.fbs     # Server → Clients broadcasts
```

### 6. Minimal Memory Allocation

Designed to work with object pooling and pre-allocated buffers:

- **Fixed-Size Enums**: Enums use fixed integer types (ushort, byte)
- **Inline Structs**: Timestamp uses struct (no indirection)
- **Vector Support**: Efficient arrays for room lists, user lists
- **String Optimization**: FlatBuffers strings are null-terminated and compact

---

## FlatBuffers Integration

### Why FlatBuffers?

**FlatBuffers** was chosen over alternatives (Protocol Buffers, JSON, MessagePack) for several reasons:

| Feature | FlatBuffers | Protocol Buffers | JSON | MessagePack |
|---------|-------------|------------------|------|-------------|
| **Zero-Copy** | ✓ | ✗ | ✗ | ✗ |
| **Schema Required** | ✓ | ✓ | ✗ | ✗ |
| **Type Safety** | ✓ | ✓ | ✗ | ✗ |
| **Human Readable** | ✗ | ✗ | ✓ | ✗ |
| **Size Efficiency** | High | High | Low | Medium |
| **Parse Speed** | Instant | Fast | Slow | Fast |
| **Mutation Support** | Limited | ✗ | ✓ | ✓ |

**Decision Rationale**:
- Zero-copy access critical for high-frequency game messages
- Type safety reduces runtime errors in C++
- Compact binary format reduces network bandwidth
- Schema evolution support for live updates

### FlatBuffers Concepts

#### 1. Tables (Reference Types)

Tables are the primary FlatBuffers type for complex objects:

```fbs
table User {
    id: uint32 (key);
    username: string (required);
    status: UserStatus = Online;
    level: uint16 = 1;
    created_at: uint64;
}
```

**Characteristics**:
- **Optional Fields**: All fields optional by default (use `required` attribute)
- **Default Values**: Fields can have defaults (`= Online`)
- **Extensible**: New fields added without breaking compatibility
- **Heap Allocated**: Stored via offsets (not inline)

#### 2. Structs (Value Types)

Structs are fixed-size, inline value types:

```fbs
struct Timestamp {
    seconds: uint64;
    nanos: uint32;
}
```

**Characteristics**:
- **Fixed Size**: All fields required, no optional fields
- **Inline Storage**: Stored directly in buffer (no offset)
- **Fast Access**: No pointer dereferencing
- **Use Case**: Small, fixed-size data (timestamps, vectors, colors)

#### 3. Unions (Polymorphic Types)

Unions allow a field to hold one of several types:

```fbs
union Payload {
    messages.LoginRequest,
    messages.CreateRoomRequest,
    messages.LoginResponse,
    // ...
}
```

**Characteristics**:
- **Type Discrimination**: Runtime type checking
- **Single Value**: Union holds exactly one variant at a time
- **Safe Casting**: Type-safe access via generated getters
- **Use Case**: Polymorphic message payloads

#### 4. Enumerations

Type-safe integer constants:

```fbs
enum MessageType : ushort {
    None = 0,
    CS_Login = 1,
    SC_LoginResult = 101,
    // ...
}
```

**Characteristics**:
- **Underlying Type**: Can specify size (byte, ushort, uint)
- **Scoped**: Generated as C++11 scoped enums (`enum class`)
- **Default Value**: First value is default

### FlatBuffers File Organization

```
protocol/
└── flatbuf/
    ├── schemas/              # Source schema definitions (.fbs)
    │   ├── enum.fbs          # Enumerations
    │   ├── common.fbs        # Common types (Timestamp)
    │   ├── user.fbs          # User types
    │   ├── room.fbs          # Room types
    │   ├── error.fbs         # Error types
    │   ├── packet.fbs        # Root packet definition
    │   └── messages/
    │       ├── client.fbs    # Client requests
    │       ├── server.fbs    # Server responses
    │       └── broadcast.fbs # Broadcast messages
    └── gen/                  # Generated C++ headers
        ├── enum_generated.h
        ├── common_generated.h
        ├── user_generated.h
        ├── room_generated.h
        ├── error_generated.h
        ├── packet_generated.h
        ├── client_generated.h
        ├── server_generated.h
        └── broadcast_generated.h
```

### FlatBuffers Attributes

| Attribute | Usage | Example |
|-----------|-------|---------|
| `required` | Field must be set | `username: string (required);` |
| `key` | Primary key field | `id: uint32 (key);` |
| `deprecated` | Mark field as obsolete | `old_field: int (deprecated);` |
| `force_align` | Align struct to N bytes | `struct Vec3 (force_align: 16) { ... }` |
| `bit_flags` | Enum is bit flags | `enum Flags : uint (bit_flags) { ... }` |

---

## Message Type Catalog

### Message Type Numbering Scheme

```
0-99:      Reserved (None)
1-99:      Client → Server (CS_*)
100-199:   Server → Client (SC_*)
200-299:   Reserved for future expansion
500-999:   Broadcast messages
999:       Error response
```

### Complete MessageType Enumeration

```cpp
enum class MessageType : uint16_t {
    None = 0,

    // Client → Server (1-99)
    CS_Login = 1,
    CS_Logout = 2,
    CS_CreateRoom = 10,
    CS_JoinRoom = 11,
    CS_LeaveRoom = 12,
    CS_GetRoomList = 13,
    CS_SendMessage = 20,

    // Server → Client (100-199)
    SC_LoginResult = 101,
    SC_LogoutResult = 102,
    SC_RoomCreated = 110,
    SC_JoinedRoom = 111,
    SC_LeftRoom = 112,
    SC_RoomList = 113,
    SC_UserJoined = 120,
    SC_UserLeft = 121,
    SC_ChatMessage = 122,

    // Errors (500+)
    SC_Error = 999
};
```

### Error Codes

```cpp
enum class ErrorCode : uint16_t {
    None = 0,
    InvalidCredentials = 1,
    RoomFull = 2,
    RoomNotFound = 3,
    AlreadyInRoom = 4,
    NotInRoom = 5,
    InvalidMessage = 6,
    ServerError = 99
};
```

### User Status

```cpp
enum class UserStatus : uint8_t {
    Offline = 0,
    Online = 1,
    InRoom = 2,
    Away = 3
};
```

---

## Request/Response Patterns

### Pattern 1: Simple Request/Response

**Login Flow**

```
Client                          Server
  │                               │
  ├─ CS_Login(username) ─────────>│
  │                               ├─ Validate credentials
  │                               ├─ Create session
  │<──────── SC_LoginResult ──────┤
  │   (ok, user, error)           │
```

**Messages**:

```fbs
// Client → Server
table LoginRequest {
    username: string (required);
}

// Server → Client
table LoginResponse {
    ok: bool;
    user: User (required);
    error: Error;
}
```

**Usage**:

```cpp
// Client: Build login request
flatbuffers::FlatBufferBuilder builder;
auto usernameOffset = builder.CreateString("Alice");
auto loginReq = messages::CreateLoginRequest(builder, usernameOffset);
auto packet = CreatePacket(builder, MessageType::CS_Login,
    Payload::messages_LoginRequest, loginReq.Union());
builder.Finish(packet, "CHAT");

// Send buffer to server
client->PostSend(builder.GetBufferPointer(), builder.GetSize());

// Server: Handle response
auto receivedPacket = GetPacket(buffer);
if (receivedPacket->type() == MessageType::CS_Login) {
    auto loginReq = receivedPacket->payload_as_messages_LoginRequest();
    std::string username = loginReq->username()->str();
    // Process login...
}
```

### Pattern 2: List Request/Response

**Get Room List Flow**

```
Client                          Server
  │                               │
  ├─ CS_GetRoomList ─────────────>│
  │   (offset, limit)             ├─ Query database
  │                               ├─ Build room list
  │<──────── SC_RoomList ─────────┤
  │   (ok, rooms[], total, error) │
```

**Messages**:

```fbs
// Client → Server
table GetRoomListRequest {
    offset: uint32 = 0;
    limit: uint32 = 100;
}

// Server → Client
table RoomListResponse {
    ok: bool;
    rooms: [Room] (required);
    total_count: uint32;
    error: Error;
}
```

### Pattern 3: Stateful Request/Response

**Create Room Flow**

```
Client                          Server
  │                               │
  ├─ CS_CreateRoom ──────────────>│
  │   (room_name, max_users)      ├─ Validate user logged in
  │                               ├─ Check room limit
  │                               ├─ Create room
  │<──────── SC_RoomCreated ──────┤
  │   (ok, room, error)           ├─ Auto-join creator
  │                               │
  │<──────── SC_JoinedRoom ────────┤
  │   (ok, room+users, error)     │
```

**Messages**:

```fbs
// Client → Server
table CreateRoomRequest {
    room_name: string (required);
    max_users: ushort = 10;
}

// Server → Client (confirmation)
table RoomCreatedResponse {
    ok: bool;
    room: Room (required);
    error: Error;
}

// Server → Client (join notification)
table JoinedRoomResponse {
    ok: bool;
    room: RoomWithUsers (required);
    error: Error;
}
```

### Pattern 4: Broadcast Notifications

**User Joined Room Flow**

```
Client A                Server                Client B, C, D
  │                       │                      │
  ├─ CS_JoinRoom ────────>│                      │
  │   (room_id)           ├─ Add to room        │
  │                       │                      │
  │<──── SC_JoinedRoom ───┤                      │
  │   (ok, room+users)    │                      │
  │                       ├─ UserJoinedBroadcast ┤──> All room members
  │                       │   (room_id, user)    │    (except joiner)
```

**Messages**:

```fbs
// Broadcast to room members
table UserJoinedBroadcast {
    room_id: uint32;
    user: User (required);
}
```

**Server Broadcast Logic**:

```cpp
void OnClientJoinRoom(Client* client, uint32_t roomId) {
    // 1. Add client to room
    auto room = _roomManager.GetRoom(roomId);
    room->AddUser(client);

    // 2. Send confirmation to joiner
    SendJoinedRoomResponse(client, room);

    // 3. Broadcast to existing members
    for (auto* member : room->GetMembers()) {
        if (member != client) {  // Don't send to joiner
            SendUserJoinedBroadcast(member, roomId, client->GetUser());
        }
    }
}
```

### Pattern 5: Error Response

**Error Handling Flow**

```
Client                          Server
  │                               │
  ├─ CS_JoinRoom(999) ───────────>│
  │                               ├─ Lookup room
  │                               ├─ Room not found!
  │<──────── SC_Error ────────────┤
  │   (RoomNotFound, "Room 999    │
  │    does not exist", details)  │
```

**Messages**:

```fbs
table ErrorResponse {
    error_code: ErrorCode;
    message: string;
    details: string;
}
```

**Usage**:

```cpp
// Server: Send error
void SendRoomNotFoundError(Client* client, uint32_t roomId) {
    flatbuffers::FlatBufferBuilder builder;

    auto msg = builder.CreateString("Room not found");
    auto details = builder.CreateString(std::format("Room ID {} does not exist", roomId));

    auto errorResp = CreateErrorResponse(builder, ErrorCode::RoomNotFound, msg, details);
    auto packet = CreatePacket(builder, MessageType::SC_Error,
        Payload::ErrorResponse, errorResp.Union());

    builder.Finish(packet, "CHAT");
    client->PostSend(builder.GetBufferPointer(), builder.GetSize());
}
```

---

## Error Handling

### Error Response Structure

All errors use the standardized `ErrorResponse` structure:

```fbs
table ErrorResponse {
    error_code: ErrorCode;      // Machine-readable error code
    message: string;             // Human-readable error message
    details: string;             // Additional context (optional)
}
```

### Error Handling Strategy

#### 1. Server-Side Error Handling

```cpp
Result<void, ErrorCode> HandleJoinRoom(Client* client, const JoinRoomRequest* req) {
    auto roomId = req->room_id();

    // Validation: Check if room exists
    auto room = _roomManager.GetRoom(roomId);
    if (!room) {
        SendError(client, ErrorCode::RoomNotFound,
            "Room not found",
            std::format("Room {} does not exist", roomId));
        return Result::Err(ErrorCode::RoomNotFound);
    }

    // Validation: Check room capacity
    if (room->IsFull()) {
        SendError(client, ErrorCode::RoomFull,
            "Room is full",
            std::format("Room {} has reached max capacity", roomId));
        return Result::Err(ErrorCode::RoomFull);
    }

    // Validation: Check already in room
    if (client->IsInRoom(roomId)) {
        SendError(client, ErrorCode::AlreadyInRoom,
            "Already in room",
            "You are already a member of this room");
        return Result::Err(ErrorCode::AlreadyInRoom);
    }

    // Success path
    room->AddMember(client);
    SendJoinedRoomResponse(client, room);
    BroadcastUserJoined(room, client);

    return Result::Ok();
}
```

#### 2. Client-Side Error Handling

```cpp
void OnPacketReceived(const Packet* packet) {
    if (packet->type() == MessageType::SC_Error) {
        auto error = packet->payload_as_ErrorResponse();

        switch (error->error_code()) {
            case ErrorCode::RoomNotFound:
                ShowErrorDialog("Room not found. It may have been deleted.");
                break;

            case ErrorCode::RoomFull:
                ShowErrorDialog("Room is full. Please try another room.");
                break;

            case ErrorCode::InvalidCredentials:
                ShowErrorDialog("Login failed. Check your username.");
                ReturnToLoginScreen();
                break;

            default:
                ShowErrorDialog(std::format("Error: {}", error->message()->str()));
                break;
        }

        LogError("Server error: {} - {} ({})",
            static_cast<int>(error->error_code()),
            error->message()->str(),
            error->details() ? error->details()->str() : "");
    }
}
```

#### 3. Response-Level Error Handling

Some responses include `ok` flag and optional `error` field:

```fbs
table LoginResponse {
    ok: bool;
    user: User (required);
    error: Error;
}
```

**Usage**:

```cpp
void OnLoginResponse(const LoginResponse* resp) {
    if (!resp->ok()) {
        auto error = resp->error();
        LogError("Login failed: {}", error->message()->str());
        ShowLoginError(error->message()->str());
        return;
    }

    // Success path
    auto user = resp->user();
    LogInfo("Logged in as {}", user->username()->str());
    EnterLobby(user);
}
```

### Error Code Categories

| Code Range | Category | Examples |
|------------|----------|----------|
| 0 | None | No error |
| 1-10 | Authentication | InvalidCredentials |
| 11-30 | Room Operations | RoomFull, RoomNotFound, AlreadyInRoom, NotInRoom |
| 31-50 | Message Operations | InvalidMessage |
| 51-98 | Reserved | Future use |
| 99 | Server | ServerError (catch-all) |

### Best Practices for Error Handling

1. **Always check `ok` flag** before accessing response data
2. **Log errors with context** (user ID, room ID, action attempted)
3. **Provide actionable error messages** to users
4. **Use `details` field** for debugging information (not shown to users)
5. **Return `Result<T, E>`** from handler functions for error propagation
6. **Don't trust client input** - validate everything server-side

---

## Message Structure and Format

### Packet Structure

Every message is wrapped in a `Packet`:

```fbs
table Packet {
    type: MessageType;          // Discriminates union type
    payload: Payload;            // Union of all message types
    sequence: uint32 = 0;        // Optional sequence number
}

root_type Packet;
file_identifier "CHAT";          // Magic number for validation
file_extension "bin";
```

### Payload Union

The `Payload` union contains all possible message types:

```fbs
union Payload {
    // Client → Server
    messages.LoginRequest,
    messages.CreateRoomRequest,
    messages.JoinRoomRequest,
    messages.LeaveRoomRequest,
    messages.SendMessageRequest,
    messages.GetRoomListRequest,

    // Server → Client
    messages.LoginResponse,
    messages.RoomCreatedResponse,
    messages.JoinedRoomResponse,
    messages.RoomListResponse,

    // Broadcast
    messages.UserJoinedBroadcast,
    messages.UserLeftBroadcast,
    messages.ChatMessageBroadcast,

    // Error
    ErrorResponse
}
```

### Binary Format Layout

FlatBuffers uses **offset-based** encoding:

```
Packet Binary Layout:
┌─────────────────────────────────────────────────────────────┐
│ Offset 0: File Identifier ("CHAT")             [4 bytes]   │
├─────────────────────────────────────────────────────────────┤
│ Offset 4: Root Table Offset (points to Packet) [4 bytes]   │
├─────────────────────────────────────────────────────────────┤
│ Packet VTable Offset                            [4 bytes]   │
├─────────────────────────────────────────────────────────────┤
│ MessageType (type field)                        [2 bytes]   │
├─────────────────────────────────────────────────────────────┤
│ Payload Union Type (discriminator)             [1 byte]    │
├─────────────────────────────────────────────────────────────┤
│ Payload Offset (points to actual message)      [4 bytes]   │
├─────────────────────────────────────────────────────────────┤
│ Sequence Number                                 [4 bytes]   │
├─────────────────────────────────────────────────────────────┤
│ ... Payload Data (variable length) ...                     │
├─────────────────────────────────────────────────────────────┤
│ ... String/Vector Data (variable length) ...               │
└─────────────────────────────────────────────────────────────┘
```

### Domain Objects

#### User

```fbs
table User {
    id: uint32 (key);
    username: string (required);
    status: UserStatus = Online;
    level: uint16 = 1;
    created_at: uint64;  // UNIX timestamp
}
```

**Fields**:
- `id`: Unique user identifier (primary key)
- `username`: Display name (required, validated server-side)
- `status`: Current status (Online, InRoom, Away, Offline)
- `level`: User level (default 1)
- `created_at`: Account creation timestamp (UNIX seconds)

#### Room

```fbs
table Room {
    id: uint32 (key);
    name: string (required);
    owner_id: uint32;
    current_users: uint16;
    max_users: uint16 = 10;
    created_at: uint64;  // UNIX timestamp
}
```

**Fields**:
- `id`: Unique room identifier
- `name`: Room display name
- `owner_id`: User ID of room creator
- `current_users`: Current occupancy
- `max_users`: Maximum capacity (default 10)
- `created_at`: Room creation timestamp

#### RoomWithUsers

```fbs
table RoomWithUsers {
    room: Room (required);
    users: [User];
}
```

**Purpose**: Used in `JoinedRoomResponse` to send room info + member list.

#### Timestamp (Struct)

```fbs
struct Timestamp {
    seconds: uint64;
    nanos: uint32;
}
```

**Purpose**: High-precision timestamp for chat messages (nanosecond resolution).

**Usage**:

```cpp
// Create timestamp
auto now = std::chrono::system_clock::now();
auto epoch = now.time_since_epoch();
auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(epoch).count() % 1000000000;

Common::Timestamp ts(seconds, nanos);

// Access in message
auto chatMsg = packet->payload_as_messages_ChatMessageBroadcast();
auto timestamp = chatMsg->timestamp();
uint64_t secs = timestamp->seconds();
uint32_t ns = timestamp->nanos();
```

### Wire Format Examples

#### Example 1: Login Request

**Schema**:

```fbs
table LoginRequest {
    username: string (required);
}
```

**Wire Format** (hexdump):

```
Offset  Hex                                        ASCII
------  -------------------------------------------  ---------
0000    43 48 41 54                                 CHAT      (file identifier)
0004    10 00 00 00                                 ........  (root offset = 16)
0008    00 00                                       ..        (vtable offset)
000A    01 00                                       ..        (type = CS_Login)
000C    01                                          .         (payload type)
000D    0C 00 00 00                                 ....      (payload offset)
0011    00 00 00 00                                 ....      (sequence = 0)
0015    05 00 00 00                                 ....      (string length = 5)
0019    41 6C 69 63 65 00                           Alice.    (username + null)
```

#### Example 2: Room List Response

**Schema**:

```fbs
table RoomListResponse {
    ok: bool;
    rooms: [Room] (required);
    total_count: uint32;
    error: Error;
}
```

**Conceptual Layout**:

```
Packet
├── type = SC_RoomList
├── payload = RoomListResponse
│   ├── ok = true
│   ├── rooms = [Room, Room, Room]  (vector offset)
│   │   ├── Room[0] (offset to Room table)
│   │   ├── Room[1] (offset to Room table)
│   │   └── Room[2] (offset to Room table)
│   ├── total_count = 42
│   └── error = null
└── sequence = 0
```

---

## Code Generation Workflow

### Schema Compilation Pipeline

```
┌───────────────────────────────────────────────────────────────┐
│ 1. Write FlatBuffers Schema (.fbs files)                     │
│    protocol/flatbuf/schemas/                                 │
│    ├── enum.fbs                                              │
│    ├── packet.fbs                                            │
│    └── messages/client.fbs                                   │
└──────────────────┬────────────────────────────────────────────┘
                   │
                   ▼
┌───────────────────────────────────────────────────────────────┐
│ 2. Run FlatBuffers Compiler (flatc)                          │
│    scripts/gen_flatbuffers.ps1                               │
│                                                               │
│    flatc --cpp --scoped-enums \                              │
│          -I schemas/ -o gen/ schemas/**/*.fbs                │
└──────────────────┬────────────────────────────────────────────┘
                   │
                   ▼
┌───────────────────────────────────────────────────────────────┐
│ 3. Generated C++ Headers                                     │
│    protocol/flatbuf/gen/                                     │
│    ├── enum_generated.h                                      │
│    ├── packet_generated.h                                    │
│    ├── client_generated.h                                    │
│    ├── server_generated.h                                    │
│    └── broadcast_generated.h                                 │
└──────────────────┬────────────────────────────────────────────┘
                   │
                   ▼
┌───────────────────────────────────────────────────────────────┐
│ 4. Include in Application Code                               │
│    #include "protocol/flatbuf/gen/packet_generated.h"        │
│    #include "protocol/flatbuf/gen/client_generated.h"        │
│                                                               │
│    using namespace highp::protocol;                          │
│    using namespace highp::protocol::messages;                │
└───────────────────────────────────────────────────────────────┘
```

### Code Generation Script

**scripts/gen_flatbuffers.ps1**:

```powershell
#!/usr/bin/env pwsh
# FlatBuffers schema compilation script

$flatcPath = "flatc"
$schemasDir = "..\protocol\flatbuf\schemas"
$outputDir = "..\protocol\flatbuf\gen"

# Check if flatc is available
try {
    $null = & $flatcPath --version 2>&1
} catch {
    Write-Host "Error: flatc compiler not found in PATH" -ForegroundColor Red
    exit 1
}

# Find all .fbs files recursively
$fbsFiles = Get-ChildItem -Path $schemasDir -Filter "*.fbs" -Recurse

foreach ($file in $fbsFiles) {
    Write-Host "Compiling: $file"

    # Compile with C++ output
    & $flatcPath --cpp --scoped-enums -I $schemasDir -o $outputDir $file.FullName

    if ($LASTEXITCODE -eq 0) {
        Write-Host "  [OK] Success" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] Failed" -ForegroundColor Red
    }
}
```

**Usage**:

```powershell
# From scripts/ directory
cd scripts
.\gen_flatbuffers.ps1

# Output:
# Compiling: enum.fbs
#   [OK] Success
# Compiling: packet.fbs
#   [OK] Success
# ...
```

### Build Integration

**Option 1: Pre-Build Event** (Visual Studio):

```xml
<PropertyGroup>
  <PreBuildEvent>
    powershell -ExecutionPolicy Bypass -File $(SolutionDir)scripts\gen_flatbuffers.ps1
  </PreBuildEvent>
</PropertyGroup>
```

**Option 2: Manual Regeneration**:

```powershell
# Only regenerate when schemas change
scripts\gen_flatbuffers.ps1
```

**Option 3: CMake Integration**:

```cmake
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/protocol/flatbuf/gen/packet_generated.h
    COMMAND flatc --cpp --scoped-enums
        -I ${CMAKE_SOURCE_DIR}/protocol/flatbuf/schemas
        -o ${CMAKE_SOURCE_DIR}/protocol/flatbuf/gen
        ${CMAKE_SOURCE_DIR}/protocol/flatbuf/schemas/packet.fbs
    DEPENDS ${CMAKE_SOURCE_DIR}/protocol/flatbuf/schemas/packet.fbs
)
```

### Compiler Options

| Option | Purpose |
|--------|---------|
| `--cpp` | Generate C++ code |
| `--scoped-enums` | Generate C++11 scoped enums (`enum class`) |
| `-I <dir>` | Include directory for schema imports |
| `-o <dir>` | Output directory for generated files |
| `--gen-mutable` | Generate mutable FlatBuffers (optional) |
| `--gen-object-api` | Generate object-based API (not recommended) |

### Generated Code Structure

**Example: enum_generated.h**

```cpp
namespace highp {
namespace protocol {

enum class MessageType : uint16_t {
  None = 0,
  CS_Login = 1,
  CS_Logout = 2,
  // ...
};

enum class ErrorCode : uint16_t {
  None = 0,
  InvalidCredentials = 1,
  // ...
};

}  // namespace protocol
}  // namespace highp
```

**Example: client_generated.h**

```cpp
namespace highp {
namespace protocol {
namespace messages {

struct LoginRequest FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  typedef LoginRequestBuilder Builder;

  const flatbuffers::String *username() const {
    return GetPointer<const flatbuffers::String *>(VT_USERNAME);
  }
};

struct LoginRequestBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;

  void add_username(flatbuffers::Offset<flatbuffers::String> username) {
    fbb_.AddOffset(LoginRequest::VT_USERNAME, username);
  }
};

}  // namespace messages
}  // namespace protocol
}  // namespace highp
```

---

## Usage Examples

### Example 1: Client Login

#### Client Side (Serialization)

```cpp
#include "protocol/flatbuf/gen/packet_generated.h"
#include "protocol/flatbuf/gen/client_generated.h"

using namespace highp::protocol;
using namespace highp::protocol::messages;

void SendLoginRequest(const std::string& username) {
    // 1. Create FlatBufferBuilder
    flatbuffers::FlatBufferBuilder builder(1024);

    // 2. Create string
    auto usernameOffset = builder.CreateString(username);

    // 3. Build LoginRequest
    auto loginReq = CreateLoginRequest(builder, usernameOffset);

    // 4. Build Packet with union
    auto packet = CreatePacket(
        builder,
        MessageType::CS_Login,
        Payload::messages_LoginRequest,
        loginReq.Union());

    // 5. Finalize with file identifier
    builder.Finish(packet, "CHAT");

    // 6. Send binary data
    const uint8_t* buf = builder.GetBufferPointer();
    size_t size = builder.GetSize();

    client->PostSend(reinterpret_cast<const char*>(buf), size);
}
```

#### Server Side (Deserialization)

```cpp
#include "protocol/flatbuf/gen/packet_generated.h"
#include "protocol/flatbuf/gen/client_generated.h"

using namespace highp::protocol;
using namespace highp::protocol::messages;

void OnRecv(Client* client, std::span<const char> data) {
    // 1. Verify buffer (optional but recommended)
    auto verifier = flatbuffers::Verifier(
        reinterpret_cast<const uint8_t*>(data.data()), data.size());

    if (!VerifyPacketBuffer(verifier)) {
        logger->Error("Invalid packet received from client #{}", client->socket);
        client->Close(true);
        return;
    }

    // 2. Get root packet (zero-copy access)
    const Packet* packet = GetPacket(data.data());

    // 3. Verify file identifier
    if (!flatbuffers::BufferHasIdentifier(data.data(), "CHAT")) {
        logger->Error("Invalid file identifier");
        return;
    }

    // 4. Route by message type
    switch (packet->type()) {
        case MessageType::CS_Login: {
            auto loginReq = packet->payload_as_messages_LoginRequest();
            HandleLogin(client, loginReq);
            break;
        }

        case MessageType::CS_CreateRoom: {
            auto createReq = packet->payload_as_messages_CreateRoomRequest();
            HandleCreateRoom(client, createReq);
            break;
        }

        // ... other cases ...

        default:
            logger->Warn("Unknown message type: {}",
                static_cast<int>(packet->type()));
            break;
    }
}
```

#### Server: Handle Login

```cpp
void HandleLogin(Client* client, const LoginRequest* req) {
    std::string username = req->username()->str();

    // Validate username
    if (username.empty() || username.length() > 20) {
        SendError(client, ErrorCode::InvalidCredentials,
            "Invalid username", "Username must be 1-20 characters");
        return;
    }

    // Create user session
    auto user = _userManager.CreateUser(username);

    // Send response
    SendLoginResponse(client, true, user, nullptr);
}
```

#### Server: Send Login Response

```cpp
void SendLoginResponse(Client* client, bool ok, const User* user, const Error* error) {
    flatbuffers::FlatBufferBuilder builder(1024);

    // Build User table
    auto usernameOffset = builder.CreateString(user->username);
    auto userOffset = CreateUser(
        builder,
        user->id,
        usernameOffset,
        UserStatus::Online,
        user->level,
        user->createdAt);

    // Build Error table (if any)
    flatbuffers::Offset<Error> errorOffset = 0;
    if (error) {
        auto msgOffset = builder.CreateString(error->message);
        errorOffset = CreateError(builder, error->errorCode, msgOffset);
    }

    // Build LoginResponse
    auto response = CreateLoginResponse(builder, ok, userOffset, errorOffset);

    // Build Packet
    auto packet = CreatePacket(
        builder,
        MessageType::SC_LoginResult,
        Payload::messages_LoginResponse,
        response.Union());

    builder.Finish(packet, "CHAT");

    // Send
    client->PostSend(builder.GetBufferPointer(), builder.GetSize());
}
```

### Example 2: Room Creation and Broadcast

#### Client: Create Room Request

```cpp
void SendCreateRoomRequest(const std::string& roomName, uint16_t maxUsers) {
    flatbuffers::FlatBufferBuilder builder;

    auto nameOffset = builder.CreateString(roomName);
    auto createReq = CreateCreateRoomRequest(builder, nameOffset, maxUsers);

    auto packet = CreatePacket(
        builder,
        MessageType::CS_CreateRoom,
        Payload::messages_CreateRoomRequest,
        createReq.Union());

    builder.Finish(packet, "CHAT");
    client->PostSend(builder.GetBufferPointer(), builder.GetSize());
}
```

#### Server: Handle Create Room

```cpp
void HandleCreateRoom(Client* client, const CreateRoomRequest* req) {
    std::string roomName = req->room_name()->str();
    uint16_t maxUsers = req->max_users();

    // Validation
    if (roomName.empty()) {
        SendError(client, ErrorCode::InvalidMessage, "Room name required", "");
        return;
    }

    // Create room
    auto room = _roomManager.CreateRoom(roomName, client->GetUserId(), maxUsers);

    // Send creation response
    SendRoomCreatedResponse(client, room);

    // Auto-join creator to room
    _roomManager.JoinRoom(client, room->id);

    // Send joined response
    SendJoinedRoomResponse(client, room);
}
```

#### Server: Broadcast User Joined

```cpp
void BroadcastUserJoined(Room* room, User* user) {
    flatbuffers::FlatBufferBuilder builder;

    // Build User
    auto usernameOffset = builder.CreateString(user->username);
    auto userOffset = CreateUser(
        builder,
        user->id,
        usernameOffset,
        UserStatus::InRoom,
        user->level,
        user->createdAt);

    // Build broadcast message
    auto broadcast = CreateUserJoinedBroadcast(builder, room->id, userOffset);

    // Build packet
    auto packet = CreatePacket(
        builder,
        MessageType::SC_UserJoined,
        Payload::messages_UserJoinedBroadcast,
        broadcast.Union());

    builder.Finish(packet, "CHAT");

    // Send to all room members (except the joined user)
    for (auto* member : room->GetMembers()) {
        if (member->GetUserId() != user->id) {
            member->PostSend(builder.GetBufferPointer(), builder.GetSize());
        }
    }
}
```

### Example 3: Chat Message with Timestamp

#### Client: Send Chat Message

```cpp
void SendChatMessage(uint32_t roomId, const std::string& message) {
    flatbuffers::FlatBufferBuilder builder;

    auto msgOffset = builder.CreateString(message);
    auto sendReq = CreateSendMessageRequest(builder, roomId, msgOffset);

    auto packet = CreatePacket(
        builder,
        MessageType::CS_SendMessage,
        Payload::messages_SendMessageRequest,
        sendReq.Union());

    builder.Finish(packet, "CHAT");
    client->PostSend(builder.GetBufferPointer(), builder.GetSize());
}
```

#### Server: Broadcast Chat Message

```cpp
void HandleSendMessage(Client* client, const SendMessageRequest* req) {
    uint32_t roomId = req->room_id();
    std::string message = req->message()->str();

    auto room = _roomManager.GetRoom(roomId);
    if (!room) {
        SendError(client, ErrorCode::RoomNotFound, "Room not found", "");
        return;
    }

    if (!room->IsMember(client->GetUserId())) {
        SendError(client, ErrorCode::NotInRoom, "Not in room", "");
        return;
    }

    BroadcastChatMessage(room, client->GetUser(), message);
}

void BroadcastChatMessage(Room* room, User* sender, const std::string& message) {
    flatbuffers::FlatBufferBuilder builder;

    // Create timestamp
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(epoch).count() % 1000000000;
    Common::Timestamp timestamp(secs, nanos);

    // Build User
    auto usernameOffset = builder.CreateString(sender->username);
    auto userOffset = CreateUser(builder, sender->id, usernameOffset,
        UserStatus::InRoom, sender->level, sender->createdAt);

    // Build message
    auto msgOffset = builder.CreateString(message);

    // Build ChatMessageBroadcast
    auto chatMsg = CreateChatMessageBroadcast(
        builder, room->id, userOffset, msgOffset, &timestamp);

    // Build packet
    auto packet = CreatePacket(
        builder,
        MessageType::SC_ChatMessage,
        Payload::messages_ChatMessageBroadcast,
        chatMsg.Union());

    builder.Finish(packet, "CHAT");

    // Broadcast to all members
    for (auto* member : room->GetMembers()) {
        member->PostSend(builder.GetBufferPointer(), builder.GetSize());
    }
}
```

#### Client: Receive and Display Chat

```cpp
void OnChatMessage(const ChatMessageBroadcast* msg) {
    auto sender = msg->sender();
    std::string username = sender->username()->str();
    std::string message = msg->message()->str();

    auto timestamp = msg->timestamp();
    uint64_t seconds = timestamp->seconds();

    // Convert to readable time
    auto time = std::chrono::system_clock::from_time_t(seconds);

    // Display in chat window
    chatWindow->AppendMessage(
        std::format("[{}] {}: {}",
            FormatTime(time), username, message));
}
```

### Example 4: Error Handling

#### Server: Send Error Response

```cpp
void SendError(Client* client, ErrorCode code,
               const std::string& message, const std::string& details) {
    flatbuffers::FlatBufferBuilder builder;

    auto msgOffset = builder.CreateString(message);
    auto detailsOffset = details.empty() ? 0 : builder.CreateString(details);

    auto errorResp = CreateErrorResponse(builder, code, msgOffset, detailsOffset);

    auto packet = CreatePacket(
        builder,
        MessageType::SC_Error,
        Payload::ErrorResponse,
        errorResp.Union());

    builder.Finish(packet, "CHAT");
    client->PostSend(builder.GetBufferPointer(), builder.GetSize());

    logger->Warn("Sent error to client #{}: {} - {}",
        client->socket, static_cast<int>(code), message);
}
```

#### Client: Handle Error Response

```cpp
void OnError(const ErrorResponse* error) {
    ErrorCode code = error->error_code();
    std::string message = error->message()->str();
    std::string details = error->details() ? error->details()->str() : "";

    logger->Error("Server error {}: {} ({})",
        static_cast<int>(code), message, details);

    // Show user-friendly error
    switch (code) {
        case ErrorCode::InvalidCredentials:
            ShowDialog("Login Failed", "Invalid username. Please try again.");
            break;

        case ErrorCode::RoomFull:
            ShowDialog("Cannot Join", "Room is full. Try another room.");
            break;

        case ErrorCode::ServerError:
            ShowDialog("Server Error", "An unexpected error occurred. Please try again later.");
            break;

        default:
            ShowDialog("Error", message);
            break;
    }
}
```

---

## Protocol Evolution and Versioning

### Schema Versioning Strategy

FlatBuffers supports schema evolution through careful design:

#### 1. Adding New Fields (Backward Compatible)

**Before**:

```fbs
table User {
    id: uint32 (key);
    username: string (required);
    level: uint16 = 1;
}
```

**After** (add new field with default):

```fbs
table User {
    id: uint32 (key);
    username: string (required);
    level: uint16 = 1;
    experience: uint64 = 0;       // NEW: Default value for compatibility
    guild_id: uint32 = 0;         // NEW: Default value
}
```

**Compatibility**:
- Old clients reading new server data: Ignore unknown fields (safe)
- New clients reading old server data: Use default values for missing fields (safe)

#### 2. Deprecating Fields

**Mark as deprecated** (do not remove):

```fbs
table User {
    id: uint32 (key);
    username: string (required);
    old_level: uint16 = 1 (deprecated);  // Keep but mark deprecated
    level_v2: uint32 = 1;                // New field
}
```

**Migration Strategy**:
1. Add new field (`level_v2`)
2. Server writes both fields for compatibility period
3. Update all clients to read new field
4. Server stops writing old field
5. Keep old field in schema (never remove for backward compatibility)

#### 3. Adding New Message Types

**Safe**: Always backward compatible

```fbs
enum MessageType : ushort {
    // ... existing types ...
    CS_SendMessage = 20,

    // NEW: Add at end
    CS_SendEmoji = 21,           // NEW
    CS_SendVoiceMessage = 22,    // NEW
}

union Payload {
    // ... existing types ...
    messages.SendMessageRequest,

    // NEW: Add to union
    messages.SendEmojiRequest,
    messages.SendVoiceMessageRequest,
}
```

**Compatibility**:
- Old clients: Ignore unknown message types
- New server: Handle both old and new message types

#### 4. Changing Field Types (Breaking Change)

**DO NOT** change field types directly:

```fbs
// BAD: Breaks compatibility
table User {
    level: uint32;  // Changed from uint16 to uint32
}
```

**DO** add new field:

```fbs
// GOOD: Add new field, deprecate old
table User {
    level_old: uint16 = 1 (deprecated);
    level: uint32 = 1;
}
```

### Version Negotiation

#### Protocol Version in Handshake

Add version field to login:

```fbs
table LoginRequest {
    username: string (required);
    protocol_version: uint32 = 1;  // Client protocol version
}

table LoginResponse {
    ok: bool;
    user: User (required);
    error: Error;
    server_protocol_version: uint32 = 1;  // Server protocol version
    min_client_version: uint32 = 1;        // Minimum supported client version
}
```

**Server Logic**:

```cpp
void HandleLogin(Client* client, const LoginRequest* req) {
    uint32_t clientVersion = req->protocol_version();

    const uint32_t SERVER_VERSION = 2;
    const uint32_t MIN_CLIENT_VERSION = 1;

    if (clientVersion < MIN_CLIENT_VERSION) {
        SendError(client, ErrorCode::InvalidMessage,
            "Client version too old",
            std::format("Please upgrade to version {}", MIN_CLIENT_VERSION));
        return;
    }

    // Store client version for feature gating
    client->SetProtocolVersion(clientVersion);

    // Send response with version info
    SendLoginResponse(client, user, SERVER_VERSION, MIN_CLIENT_VERSION);
}
```

**Client Logic**:

```cpp
void OnLoginResponse(const LoginResponse* resp) {
    uint32_t serverVersion = resp->server_protocol_version();
    uint32_t minVersion = resp->min_client_version();

    const uint32_t CLIENT_VERSION = 1;

    if (CLIENT_VERSION < minVersion) {
        ShowDialog("Update Required",
            std::format("Your client version {} is too old. Please update to version {}.",
                CLIENT_VERSION, minVersion));
        ExitApplication();
        return;
    }

    // Store server version for feature detection
    _serverProtocolVersion = serverVersion;

    // Enable features based on server version
    if (serverVersion >= 2) {
        EnableVoiceChat();
    }
}
```

### Schema Evolution Best Practices

1. **Never Remove Fields**: Mark as deprecated instead
2. **Always Add Defaults**: New fields must have default values
3. **Test Both Directions**: Old client + new server, new client + old server
4. **Version Handshake**: Negotiate protocol version during login
5. **Feature Flags**: Use version checks for optional features
6. **Migration Period**: Support old and new fields simultaneously during transitions
7. **Document Changes**: Maintain changelog for schema modifications

### Changelog Example

**PROTOCOL_CHANGELOG.md**:

```markdown
# Protocol Changelog

## Version 2.0 (2026-03-01)

### Breaking Changes
- None

### New Features
- Added `CS_SendEmoji` (21) and `SC_EmojiReceived` (123) for emoji support
- Added `experience` and `guild_id` fields to `User` table

### Deprecated
- None

### Migration Guide
- Update client to protocol version 2
- Server supports both v1 and v2 clients

## Version 1.0 (2026-02-05)

### Initial Release
- Basic chat functionality
- Room management
- User authentication
```

---

## Performance Considerations

### 1. Zero-Copy Deserialization

**FlatBuffers Advantage**: Access data without copying

```cpp
// BAD: Copying data (Protocol Buffers style)
LoginRequest req;
req.ParseFromArray(buffer, size);  // Copies and parses
std::string username = req.username();  // Another copy

// GOOD: Zero-copy access (FlatBuffers)
const Packet* packet = GetPacket(buffer);  // No copy, just cast
const LoginRequest* req = packet->payload_as_messages_LoginRequest();
const char* username = req->username()->c_str();  // Direct pointer into buffer
```

**Performance Impact**:
- No deserialization CPU overhead
- Reduced memory allocations
- Lower latency (critical for game servers)

### 2. Builder Reuse and Pooling

**Optimization**: Reuse FlatBufferBuilder to avoid allocations

```cpp
class PacketBuilder {
public:
    PacketBuilder() : _builder(1024) {}  // Pre-allocate 1KB

    void Reset() {
        _builder.Clear();  // Reuse existing buffer
    }

    flatbuffers::FlatBufferBuilder& Builder() { return _builder; }

private:
    flatbuffers::FlatBufferBuilder _builder;
};

// Per-thread builder pool
thread_local PacketBuilder g_builderPool;

void SendLoginRequest(const std::string& username) {
    auto& builder = g_builderPool.Builder();
    builder.Clear();  // Reuse

    auto usernameOffset = builder.CreateString(username);
    auto loginReq = CreateLoginRequest(builder, usernameOffset);
    auto packet = CreatePacket(builder, MessageType::CS_Login,
        Payload::messages_LoginRequest, loginReq.Union());
    builder.Finish(packet, "CHAT");

    client->PostSend(builder.GetBufferPointer(), builder.GetSize());
}
```

**Performance Gain**:
- Eliminates allocator calls
- Reuses existing buffer memory
- Reduces GC pressure (if using managed languages)

### 3. Batch Message Processing

**Pattern**: Process multiple messages in single network operation

```cpp
// Instead of sending 10 separate packets:
for (const auto& msg : messages) {
    SendChatMessage(msg);  // 10 WSASend calls
}

// Batch into single buffer:
flatbuffers::FlatBufferBuilder builder;
std::vector<flatbuffers::Offset<Packet>> packets;

for (const auto& msg : messages) {
    auto msgOffset = builder.CreateString(msg);
    // ... build packet ...
    packets.push_back(packetOffset);
}

auto packetList = builder.CreateVector(packets);
// Send entire batch in one WSASend call
```

**Performance Gain**:
- Fewer system calls
- Better network utilization
- Reduced per-packet overhead

### 4. String Handling Optimization

**Avoid Unnecessary String Copies**:

```cpp
// BAD: Creates std::string (allocates)
void HandleLogin(const LoginRequest* req) {
    std::string username = req->username()->str();  // Copy!
    ProcessUsername(username);
}

// GOOD: Use string_view (no allocation)
void HandleLogin(const LoginRequest* req) {
    std::string_view username = req->username()->string_view();  // Zero-copy
    ProcessUsername(username);
}

// BEST: Direct pointer (if null-terminated)
void HandleLogin(const LoginRequest* req) {
    const char* username = req->username()->c_str();  // Zero-copy
    ProcessUsername(username);
}
```

### 5. Verification Cost

**Verification** ensures buffer integrity but has overhead:

```cpp
// EXPENSIVE: Full verification every message
void OnRecv(std::span<const char> data) {
    flatbuffers::Verifier verifier(
        reinterpret_cast<const uint8_t*>(data.data()), data.size());

    if (!VerifyPacketBuffer(verifier)) {  // CPU intensive
        return;
    }
    // ... process ...
}

// OPTIMIZED: Verify only in debug or on untrusted input
void OnRecv(std::span<const char> data) {
#ifdef DEBUG
    flatbuffers::Verifier verifier(...);
    assert(VerifyPacketBuffer(verifier));
#endif

    // In release, trust our own network layer
    const Packet* packet = GetPacket(data.data());
    // ... process ...
}
```

**Trade-off**:
- Verification: Safety vs. performance
- Recommendation: Verify on external input (client → server), skip on trusted internal messages

### 6. Field Access Patterns

**Hot Path Optimization**:

```cpp
// BAD: Repeated field access (may involve offset lookups)
void ProcessUser(const User* user) {
    logger->Info("{}", user->username()->str());
    logger->Info("{}", user->username()->str());  // Duplicate lookup
    logger->Info("{}", user->username()->str());
}

// GOOD: Cache field access
void ProcessUser(const User* user) {
    std::string_view username = user->username()->string_view();
    logger->Info("{}", username);
    logger->Info("{}", username);
    logger->Info("{}", username);
}
```

### 7. Memory Layout and Cache Efficiency

**FlatBuffers Layout** is cache-friendly:

```
Sequential Memory Layout:
┌──────────────────────────────────────┐
│ Packet Header                        │  Cache Line 1
├──────────────────────────────────────┤
│ MessageType, Payload Type            │  Cache Line 1
├──────────────────────────────────────┤
│ Payload Data (User, Room, etc.)      │  Cache Line 2-3
└──────────────────────────────────────┘
```

**Access Patterns**:
- Sequential reads are cache-efficient
- Small messages fit in single cache line
- Prefetching benefits sequential access

### 8. Protocol Size Comparison

**Size Benchmarks** (LoginRequest with username "Alice"):

| Format | Size (bytes) | Overhead |
|--------|--------------|----------|
| FlatBuffers | 32 | Baseline |
| Protocol Buffers | 28 | -12% |
| JSON | 45 | +40% |
| MessagePack | 38 | +18% |

**FlatBuffers Trade-off**:
- Slightly larger than Protocol Buffers
- Much smaller than JSON
- Zero-copy benefit outweighs size overhead for game servers

### 9. Network Bandwidth Optimization

**Compression** (optional):

```cpp
// For large messages (e.g., room list with 100+ rooms)
void SendRoomList(Client* client, const std::vector<Room>& rooms) {
    flatbuffers::FlatBufferBuilder builder;
    // ... build packet ...

    auto buffer = builder.GetBufferPointer();
    auto size = builder.GetSize();

    // Compress if large
    if (size > 1024) {
        auto compressed = CompressZlib(buffer, size);
        client->PostSendCompressed(compressed);
    } else {
        client->PostSend(buffer, size);
    }
}
```

**Compression Trade-off**:
- CPU overhead for compression/decompression
- Bandwidth savings (50-70% for text-heavy messages)
- Recommendation: Compress only large, infrequent messages

### Performance Summary

| Optimization | Impact | Complexity |
|-------------|--------|------------|
| Zero-copy deserialization | High | Low (built-in) |
| Builder reuse | Medium | Low |
| Skip verification (release) | Medium | Low |
| String view over string | Medium | Low |
| Batch messages | High | Medium |
| Compression | Medium | Medium |
| Field access caching | Low | Low |

---

## Best Practices

### 1. Schema Design

**DO**:
- Use `required` for critical fields (username, id)
- Provide sensible defaults for optional fields
- Use fixed-size types (uint32, uint16) for numeric fields
- Group related fields in separate tables (User, Room)
- Use structs for small, fixed-size data (Timestamp)

**DON'T**:
- Make all fields required (limits evolution)
- Use large default values (wastes space)
- Nest tables too deeply (performance penalty)
- Use deprecated field slots for new fields

### 2. Message Handling

**DO**:
- Always verify file identifier on receive
- Check message type before casting
- Use string_view to avoid copies
- Log unknown message types
- Handle all enum cases in switch statements

**DON'T**:
- Trust client input without validation
- Cast union without type check
- Ignore verification in debug builds
- Block worker threads in message handlers

### 3. Error Handling

**DO**:
- Use ErrorResponse for all errors
- Provide user-friendly messages
- Include details for debugging (server-side logging)
- Return early on validation failures
- Log errors with context

**DON'T**:
- Expose internal errors to clients
- Use error codes inconsistently
- Ignore error responses on client
- Crash on invalid input

### 4. Code Organization

**DO**:
- Separate protocol layer from business logic
- Create helper functions for common operations
- Use namespaces to avoid naming conflicts
- Document custom schema attributes
- Keep generated files separate from application code

**DON'T**:
- Modify generated files (regenerated on schema change)
- Mix protocol serialization with game logic
- Hardcode message type values

### 5. Testing

**DO**:
- Unit test serialization/deserialization
- Test backward compatibility (old client + new server)
- Fuzz test with invalid buffers
- Measure message sizes in tests
- Validate schema changes before deployment

**DON'T**:
- Test only happy path
- Assume FlatBuffers handles all errors
- Skip verification in tests

### 6. Performance

**DO**:
- Reuse FlatBufferBuilder instances
- Use thread-local builder pools
- Measure message processing latency
- Profile hot paths (login, chat)
- Batch messages when appropriate

**DON'T**:
- Allocate builder per message
- Copy strings unnecessarily
- Verify buffers multiple times
- Block on compression in critical path

### 7. Documentation

**DO**:
- Document schema evolution strategy
- Maintain protocol changelog
- Comment non-obvious field meanings
- Provide usage examples for complex messages
- Document error codes

**DON'T**:
- Leave deprecated fields undocumented
- Assume schema is self-documenting

---

## Integration with Network Layer

### Network Layer Responsibilities

See [NETWORK_LAYER.md](NETWORK_LAYER.md) for details on:

- **IOCP-based async I/O**: WSARecv/WSASend with completion ports
- **Client connection management**: Connection lifecycle, buffer management
- **Packet framing**: Length-prefixed message framing (not shown in schemas)

### Protocol + Network Integration

```cpp
// Network layer provides raw byte streams
void ServerLifeCycle::HandleRecv(CompletionEvent& event) {
    auto* client = static_cast<Client*>(event.completionKey);
    auto* overlapped = reinterpret_cast<RecvOverlapped*>(event.overlapped);

    std::span<const char> rawData{ overlapped->buf, event.bytesTransferred };

    // Protocol layer: Deserialize FlatBuffers packet
    const Packet* packet = GetPacket(rawData.data());

    // Route to handler
    _packetRouter.RoutePacket(client, packet);

    // Continue receiving
    client->PostRecv();
}
```

### Packet Framing (Length-Prefixed)

FlatBuffers messages are variable-length. Use length prefix for framing:

```
Network Stream:
┌──────────────────────────────────────────────────────────────┐
│ [4 bytes: length] [N bytes: FlatBuffers Packet]             │
└──────────────────────────────────────────────────────────────┘

Example:
┌─────────┬──────────────────────────────────────────────────┐
│ 0x20 00 │ 43 48 41 54 ... (32 bytes of FlatBuffers data)  │
│ 00 00   │                                                  │
└─────────┴──────────────────────────────────────────────────┘
```

**Send with Length Prefix**:

```cpp
void SendPacket(Client* client, const flatbuffers::FlatBufferBuilder& builder) {
    uint32_t packetSize = builder.GetSize();

    // Allocate buffer: 4 bytes (size) + packet data
    std::vector<char> buffer(sizeof(uint32_t) + packetSize);

    // Write length prefix (little-endian)
    std::memcpy(buffer.data(), &packetSize, sizeof(uint32_t));

    // Write packet data
    std::memcpy(buffer.data() + sizeof(uint32_t),
        builder.GetBufferPointer(), packetSize);

    // Send combined buffer
    client->PostSend(buffer.data(), buffer.size());
}
```

**Receive with Length Prefix**:

```cpp
void OnRecv(Client* client, std::span<const char> data) {
    // Read length prefix
    if (data.size() < sizeof(uint32_t)) {
        // Incomplete, wait for more data
        return;
    }

    uint32_t packetSize;
    std::memcpy(&packetSize, data.data(), sizeof(uint32_t));

    // Check if full packet received
    if (data.size() < sizeof(uint32_t) + packetSize) {
        // Incomplete, wait for more data
        return;
    }

    // Extract packet data (skip length prefix)
    const char* packetData = data.data() + sizeof(uint32_t);

    // Deserialize FlatBuffers packet
    const Packet* packet = GetPacket(packetData);

    // Process packet
    RoutePacket(client, packet);
}
```

---

## Related Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) - Overall system architecture
- [NETWORK_LAYER.md](NETWORK_LAYER.md) - IOCP network layer details
- [CLAUDE.md](../CLAUDE.md) - Build and development guide

---

## References

**FlatBuffers Documentation**:
- [FlatBuffers Official Documentation](https://google.github.io/flatbuffers/)
- [FlatBuffers C++ Guide](https://google.github.io/flatbuffers/flatbuffers_guide_use_cpp.html)
- [FlatBuffers Schema Language](https://google.github.io/flatbuffers/flatbuffers_guide_writing_schema.html)
- [FlatBuffers Benchmarks](https://google.github.io/flatbuffers/flatbuffers_benchmarks.html)

**Related Technologies**:
- [Protocol Buffers](https://developers.google.com/protocol-buffers)
- [MessagePack](https://msgpack.org/)
- [Cap'n Proto](https://capnproto.org/)

---

**Last Updated**: 2026-02-05
**Version**: 1.0
**Maintainer**: Protocol Team
