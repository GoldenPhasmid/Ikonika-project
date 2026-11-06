#include "core/FTUuid.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <random>

namespace
{
    constexpr size_t UuidLength = 36;

    bool IsDashPosition(size_t Position)
    {
        return Position == 8 || Position == 13 || Position == 18 || Position == 23;
    }

    std::mt19937_64& GetGenerator()
    {
        // Seeded once per thread from the system. Thread local so two threads cannot interleave
        // draws from one generator and hand back the same value.
        static thread_local std::mt19937_64 Generator{std::random_device{}()};

        return Generator;
    }

    char ToHexDigit(unsigned int Value)
    {
        constexpr char Digits[] = "0123456789abcdef";

        return Digits[Value & 0xF];
    }

    constexpr uint32_t MaximumSequence = 0xFFF;

    std::string FormatUuid(const std::array<uint8_t, 16>& Bytes)
    {
        std::string Result;
        Result.reserve(UuidLength);

        for (size_t Index = 0; Index < Bytes.size(); ++Index)
        {
            if (Index == 4 || Index == 6 || Index == 8 || Index == 10)
            {
                Result.push_back('-');
            }

            Result.push_back(ToHexDigit(static_cast<unsigned int>(Bytes[Index] >> 4)));
            Result.push_back(ToHexDigit(static_cast<unsigned int>(Bytes[Index] & 0x0F)));
        }

        return Result;
    }

    bool IsLowercaseHexDigit(char Character)
    {
        return (Character >= '0' && Character <= '9')
            || (Character >= 'a' && Character <= 'f');
    }
}

std::string MakeUuid()
{
    std::uniform_int_distribution<uint64_t> Distribution;

    const uint64_t High = Distribution(GetGenerator());
    const uint64_t Low = Distribution(GetGenerator());

    std::array<uint8_t, 16> Bytes{};

    for (size_t Index = 0; Index < 8; ++Index)
    {
        const unsigned int Shift = static_cast<unsigned int>((7 - Index) * 8);

        Bytes[Index] = static_cast<uint8_t>((High >> Shift) & 0xFF);
        Bytes[Index + 8] = static_cast<uint8_t>((Low >> Shift) & 0xFF);
    }

    // Version 4 goes in the high nibble of byte 6, which is character 14.
    Bytes[6] = static_cast<uint8_t>((Bytes[6] & 0x0F) | 0x40);

    // The RFC 4122 variant goes in the top two bits of byte 8, which is character 19. That
    // restricts it to 8, 9, a or b.
    Bytes[8] = static_cast<uint8_t>((Bytes[8] & 0x3F) | 0x80);

    return FormatUuid(Bytes);
}

std::string MakeTimeOrderedUuid()
{
    static std::mutex Guard;
    static uint64_t LastMilliseconds = 0;
    static uint32_t LastSequence = 0;

    const auto Now = std::chrono::system_clock::now().time_since_epoch();
    uint64_t Milliseconds = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Now).count());
    uint32_t Sequence = 0;

    {
        const std::lock_guard<std::mutex> Lock{Guard};

        if (Milliseconds <= LastMilliseconds)
        {
            Milliseconds = LastMilliseconds;
            Sequence = LastSequence + 1;

            if (Sequence > MaximumSequence)
            {
                ++Milliseconds;
                Sequence = 0;
            }
        }

        LastMilliseconds = Milliseconds;
        LastSequence = Sequence;
    }

    std::uniform_int_distribution<uint64_t> Distribution;
    const uint64_t Random = Distribution(GetGenerator());

    std::array<uint8_t, 16> Bytes{};

    for (size_t Index = 0; Index < 6; ++Index)
    {
        Bytes[Index] = static_cast<uint8_t>((Milliseconds >> ((5 - Index) * 8)) & 0xFF);
    }

    Bytes[6] = static_cast<uint8_t>(0x70 | ((Sequence >> 8) & 0x0F));
    Bytes[7] = static_cast<uint8_t>(Sequence & 0xFF);

    for (size_t Index = 8; Index < 16; ++Index)
    {
        Bytes[Index] = static_cast<uint8_t>((Random >> ((15 - Index) * 8)) & 0xFF);
    }

    Bytes[8] = static_cast<uint8_t>((Bytes[8] & 0x3F) | 0x80);

    return FormatUuid(Bytes);
}

bool IsValidUuid(const std::string& Value)
{
    if (Value.size() != UuidLength)
    {
        return false;
    }

    for (size_t Position = 0; Position < UuidLength; ++Position)
    {
        if (IsDashPosition(Position))
        {
            if (Value[Position] != '-')
            {
                return false;
            }

            continue;
        }

        if (!IsLowercaseHexDigit(Value[Position]))
        {
            return false;
        }
    }

    if (Value[14] != '4' && Value[14] != '7')
    {
        return false;
    }

    const char Variant = Value[19];

    return Variant == '8' || Variant == '9' || Variant == 'a' || Variant == 'b';
}
