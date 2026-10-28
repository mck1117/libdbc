#pragma once

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <istream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace libdbc
{

enum class Endian : uint8_t
{
    Little_Intel = 0,
    Big_Motorola = 1
};

enum class ValueType : uint8_t
{
    Integer = 0,
    Float32 = 1,    // SIG_VALTYPE_ 1: IEEE single, signal must be 32 bits
    Float64 = 2,    // SIG_VALTYPE_ 2: IEEE double, signal must be 64 bits
};

enum class MuxRole : uint8_t
{
    None,           // Always present
    Multiplexor,    // "M": selects which multiplexed signals are present
    Multiplexed,    // "mN": only present when the multiplexor's value is N
};

struct Signal final
{
    const size_t Id;

    const std::string Name;
    const Endian Endianness;
    const bool Signed;
    const uint16_t Bitpos;
    const uint8_t Length;
    const float Factor;
    const float Offset;
    float Min;
    float Max;
    const std::string Unit;

    const MuxRole Mux;
    // Value of the multiplexor that selects this signal, if Mux == Multiplexed
    const uint32_t MuxValue;

    // Set by a SIG_VALTYPE_ line, which comes after the signal's definition
    ValueType Type = ValueType::Integer;

    // Names for particular raw values, from VAL_ lines. Keyed by the signal's raw bits, so a
    // signed value like -1 is stored as all ones in the signal's length.
    std::unordered_map<uint64_t, std::string> ValueNames;

    Signal(size_t id, std::string_view name, Endian endian, bool isSigned, uint16_t bitpos, uint8_t length, float factor, float offset, float min, float max, std::string_view unit, MuxRole mux, uint32_t muxValue)
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
        , Mux(mux)
        , MuxValue(muxValue)
    {
    }

    // Name for a raw value (as passed to the decoder callback), or nullptr if it has none
    const std::string* ValueName(uint64_t bits) const
    {
        auto search = ValueNames.find(bits);
        return search == ValueNames.end() ? nullptr : &search->second;
    }
};

struct Message final
{
    // CAN ID with the DBC extended flag stripped
    const uint32_t Id;
    const bool Extended;
    // Length in bytes, up to 64 for CAN FD
    const uint8_t Dlc;
    const std::string Name;
    std::vector<Signal> Signals;

    Message(uint32_t id, bool extended, uint8_t dlc, std::string_view name)
        : Id(id)
        , Extended(extended)
        , Dlc(dlc)
        , Name(name)
    {
    }
};

struct CanFrame final
{
    uint32_t Id;
    bool Extended;
    // Length in bytes, up to 64 for CAN FD
    uint8_t Dlc;
    uint8_t Data[64];
};

class Dbc final
{
public:
    // Messages are keyed by their ID as written in the DBC: extended IDs have ExtendedIdFlag set
    using message_container = std::unordered_map<uint32_t, Message>;
    using DecoderFunc = std::function<void(const Signal& signal, uint64_t bitsValue, float scaledValue)>;

    static constexpr uint32_t ExtendedIdFlag = 0x80000000;

    // Returns nullptr if the stream can't be read or the DBC is malformed.
    // onSignal is called for each signal in Id order, only if parsing succeeds.
    static std::unique_ptr<Dbc> Parse(std::istream& file, std::function<void(const Signal&)> onSignal = nullptr);

    Dbc(size_t signalCount, message_container messages)
        : m_signalCount(signalCount)
        , m_messages(std::move(messages))
    {
    }

    void Decode(const CanFrame& frame, DecoderFunc onDecoded) const;

    const Message* FindMessage(uint32_t id, bool extended) const;

    size_t SignalCount() const
    {
        return m_signalCount;
    }

    const message_container& Messages() const
    {
        return m_messages;
    }

    // Number of bytes of frame data a signal needs to be present
    static size_t BytesRequired(Endian endianness, uint16_t bitpos, uint8_t length);
    // data must contain at least BytesRequired() bytes, length must be 1-64
    static uint64_t GetSignalBits(std::span<const uint8_t> data, Endian endianness, uint16_t bitpos, uint8_t length);
    static float ToFloat(uint64_t bits, uint8_t length, bool isSigned);

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
    std::string m_lineBuffer;
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
