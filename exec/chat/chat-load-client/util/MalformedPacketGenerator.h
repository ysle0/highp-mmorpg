#pragma once

#include <cstdint>
#include <random>
#include <vector>

class MalformedPacketGenerator {
public:
    enum class Type { RandomBytes, TruncatedPayload, InvalidMessageType };

    explicit MalformedPacketGenerator(unsigned int seed = std::random_device{}());

    [[nodiscard]] std::vector<uint8_t> Generate(int approximateSize);
    [[nodiscard]] std::vector<uint8_t> Generate(Type type, int approximateSize);

private:
    std::mt19937 _rng;

    [[nodiscard]] std::vector<uint8_t> GenerateRandomBytes(int size);
    [[nodiscard]] std::vector<uint8_t> GenerateTruncatedPayload(int size);
    [[nodiscard]] std::vector<uint8_t> GenerateInvalidMessageType(int size);
};
