#pragma once

#include <memory>
#include <vector>
#include <istream>
#include <map>
#include <functional>
#include <string_view>

namespace libdbc
{

enum class Endian : uint8_t
{
    Little_Intel = 0,
    Big_Motorola = 1
};

struct Signal
{
    const size_t Id;

    std::string Name;
    const Endian Endianness;
    const bool Signed;
    const uint8_t Bitpos;
    const uint8_t Length;
    const float Factor;
    const float Offset;
    float Min;
    float Max;

    Signal(size_t id, std::string_view name, Endian endian, bool isSigned, uint8_t bitpos, uint8_t length, float factor, float offset, float min, float max)
        : Id(id)
        , Name(name)
        , Endianness(endian)
        , Signed(isSigned)
        , Bitpos(bitpos)
        , Length(length)
        , Factor(factor)
        , Offset(offset)
        , Min(min)
        , Max(max)
    {
    }
};

struct Message
{
    const uint32_t Id;
    const uint8_t Dlc;
    std::string Name;
    std::vector<Signal> Signals;

    Message(uint32_t id, uint8_t dlc, std::string_view name)
        : Id(id)
        , Dlc(dlc)
        , Name(name)
    {
    }
};

using DecoderFunc = std::function<void(const Signal& signal, uint64_t bitsValue, float scaledValue)>;

struct CanFrame
{
    uint32_t Id;
    uint8_t Dlc;

    union
    {
        uint8_t Data8[8];
        uint64_t Data64;
    };
};

class Dbc
{
public:
    Dbc(size_t signalCount, const std::map<uint32_t, Message> messages)
        : m_signalCount(signalCount)
        , m_messages(messages)
    {
    }

    void Decode(CanFrame& frame, DecoderFunc onDecoded) const;

    size_t SignalCount() const
    {
        return m_signalCount;
    }

    const std::map<uint32_t, Message>& Messages() const
    {
        return m_messages;
    }

    static uint32_t GetSignalBits(uint64_t data, Endian endianness, uint8_t bitpos, uint8_t length);

private:
    const size_t m_signalCount;
    std::map<uint32_t, Message> m_messages;
};

std::unique_ptr<Dbc> ParseDbcFile(std::istream& file);

namespace impl
{

class FileReader
{
public:
    FileReader(std::istream& file);

    void AdvanceLine();
    std::string_view Line() const;

private:
    char m_lineBuffer[256];
    std::string_view m_line;

    std::istream& m_file;
};

} // namespace impl

namespace util
{
    int64_t ParseInt(std::string_view str);
    float ParseFloat(std::string_view str);
}

} // namespace libdbc
