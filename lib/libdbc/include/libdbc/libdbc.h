#pragma once

#include <charconv>
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

struct Signal final
{
    const size_t Id;

    const std::string Name;
    const Endian Endianness;
    const bool Signed;
    const uint8_t Bitpos;
    const uint8_t Length;
    const float Factor;
    const float Offset;
    float Min;
    float Max;
    const std::string Unit;

    Signal(size_t id, std::string_view name, Endian endian, bool isSigned, uint8_t bitpos, uint8_t length, float factor, float offset, float min, float max, std::string_view unit)
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
        , Unit(unit)
    {
    }
};

struct Message final
{
    const uint32_t Id;
    const uint8_t Dlc;
    const std::string Name;
    std::vector<Signal> Signals;

    Message(uint32_t id, uint8_t dlc, std::string_view name)
        : Id(id)
        , Dlc(dlc)
        , Name(name)
    {
    }
};

struct CanFrame final
{
    uint32_t Id;
    uint8_t Dlc;

    union
    {
        uint8_t Data8[8];
        uint64_t Data64;
    };
};

class Dbc final
{
public:
    using message_container = std::unordered_map<uint32_t, Message>;
    using DecoderFunc = std::function<void(const Signal& signal, uint64_t bitsValue, float scaledValue)>;

    static std::unique_ptr<Dbc> Parse(std::istream& file, std::function<void(const Signal&)> = nullptr);

    Dbc(size_t signalCount, const message_container messages)
        : m_signalCount(signalCount)
        , m_messages(messages)
    {
    }

    void Decode(const CanFrame& frame, DecoderFunc onDecoded) const;

    size_t SignalCount() const
    {
        return m_signalCount;
    }

    const message_container& Messages() const
    {
        return m_messages;
    }

    static uint32_t GetSignalBits(uint64_t data, Endian endianness, uint8_t bitpos, uint8_t length);
    static float ToFloat(uint32_t bits, uint8_t length, bool isSigned);

private:
    const size_t m_signalCount;
    message_container m_messages;
};

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
    template <typename T>
    inline std::enable_if_t<!std::is_floating_point_v<T>, T> from_sv(std::string_view str, int base = 10)
    {
        T result{};
        std::from_chars(str.data(), str.data() + str.size(), result, base);
        return result;
    }

    template <typename T>
    inline std::enable_if_t<std::is_floating_point_v<T>, T> from_sv(std::string_view str)
    {
        T result{};
        std::from_chars(str.data(), str.data() + str.size(), result);
        return result;
    }
}

} // namespace libdbc
