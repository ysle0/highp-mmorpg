#include "util/MalformedPacketGenerator.h"

#include <algorithm>

MalformedPacketGenerator::MalformedPacketGenerator(unsigned int seed)
    : _rng(seed) {
}

std::vector<uint8_t> MalformedPacketGenerator::Generate(int approximateSize) {
    std::uniform_int_distribution<int> dist(0, 2);
    auto type = static_cast<Type>(dist(_rng));
    return Generate(type, approximateSize);
}

std::vector<uint8_t> MalformedPacketGenerator::Generate(Type type, int approximateSize) {
    switch (type) {
    case Type::RandomBytes:
        return GenerateRandomBytes(approximateSize);
    case Type::TruncatedPayload:
        return GenerateTruncatedPayload(approximateSize);
    case Type::InvalidMessageType:
        return GenerateInvalidMessageType(approximateSize);
    default:
        return GenerateRandomBytes(approximateSize);
    }
}

std::vector<uint8_t> MalformedPacketGenerator::GenerateRandomBytes(int size) {
    size = std::max(size, 4);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    std::uniform_int_distribution<int> dist(0, 255);
    for (auto& byte : data) {
        byte = static_cast<uint8_t>(dist(_rng));
    }
    return data;
}

std::vector<uint8_t> MalformedPacketGenerator::GenerateTruncatedPayload(int size) {
    int truncatedSize = std::max(size / 2, 4);
    std::vector<uint8_t> data(static_cast<size_t>(truncatedSize), 0);

    uint32_t rootOffset = static_cast<uint32_t>(truncatedSize);
    data[0] = static_cast<uint8_t>(rootOffset & 0xFF);
    data[1] = static_cast<uint8_t>((rootOffset >> 8) & 0xFF);
    data[2] = static_cast<uint8_t>((rootOffset >> 16) & 0xFF);
    data[3] = static_cast<uint8_t>((rootOffset >> 24) & 0xFF);

    std::uniform_int_distribution<int> dist(0, 255);
    for (size_t i = 4; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(dist(_rng));
    }
    return data;
}

std::vector<uint8_t> MalformedPacketGenerator::GenerateInvalidMessageType(int size) {
    size = std::max(size, 16);
    std::vector<uint8_t> data(static_cast<size_t>(size), 0);

    data[0] = 4; data[1] = 0; data[2] = 0; data[3] = 0;
    data[4] = 4; data[5] = 0; data[6] = 0; data[7] = 0;

    if (size > 10) {
        data[8] = 0xFF;
        data[9] = 0xFF;
    }

    std::uniform_int_distribution<int> dist(0, 255);
    for (size_t i = 10; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(dist(_rng));
    }
    return data;
}
