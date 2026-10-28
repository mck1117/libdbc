#include <libdbc/libdbc.h>

#include <algorithm>
#include <bit>
#include <cctype>
#include <optional>

using namespace libdbc;
using namespace libdbc::impl;

void Dbc::Decode(const CanFrame& frame, DecoderFunc onDecoded) const
{
    const Message* message = FindMessage(frame.Id, frame.Extended);

    // Do we know how to decode this frame?
    if (!message)
    {
        // If not, bail out early
        return;
    }

    // Only the bytes the frame actually carried are valid
    std::span<const uint8_t> data(frame.Data, std::min<size_t>(frame.Dlc, std::size(frame.Data)));

    auto fits = [&](const Signal& signal)
    {
        return BytesRequired(signal.Endianness, signal.Bitpos, signal.Length) <= data.size();
    };

    // If the message is multiplexed, find which multiplexed signals are present
    std::optional<uint64_t> muxValue;
    for (const auto& signal : message->Signals)
    {
        if (signal.Mux == MuxRole::Multiplexor)
        {
            if (fits(signal))
            {
                muxValue = GetSignalBits(data, signal.Endianness, signal.Bitpos, signal.Length);
            }

            break;
        }
    }

    // Decode every signal in the message
    for (const auto& signal : message->Signals)
    {
        if (!fits(signal))
        {
            // Frame is too short to contain this signal
            continue;
        }

        if (signal.Mux == MuxRole::Multiplexed && muxValue != signal.MuxValue)
        {
            continue;
        }

        auto bits = GetSignalBits(data, signal.Endianness, signal.Bitpos, signal.Length);

        float val;

        switch (signal.Type)
        {
        case ValueType::Float32:
            val = signal.Factor * std::bit_cast<float>(static_cast<uint32_t>(bits)) + signal.Offset;
            break;
        case ValueType::Float64:
            val = static_cast<float>(signal.Factor * std::bit_cast<double>(bits) + signal.Offset);
            break;
        default:
            val = signal.Factor * ToFloat(bits, signal.Length, signal.Signed) + signal.Offset;
            break;
        }

        if (onDecoded)
        {
            onDecoded(signal, bits, val);
        }
    }
}

const Message* Dbc::FindMessage(uint32_t id, bool extended) const
{
    uint32_t key = extended ? ((id & 0x1FFFFFFF) | ExtendedIdFlag) : id;

    auto search = m_messages.find(key);
    return search == m_messages.end() ? nullptr : &search->second;
}

/*static*/ float Dbc::ToFloat(uint64_t bits, uint8_t length, bool isSigned)
{
    // if signed, sign extend as necessary
    if (isSigned)
    {
        auto signBit = bits & (1ULL << (length - 1));
        uint64_t mask = signBit ? ~((signBit) - 1) : 0;

        bits |= mask;

        return static_cast<float>(std::bit_cast<int64_t>(bits));
    }
    else
    {
        return static_cast<float>(bits);
    }
}

// Motorola signals are specified by their MSB, numbered "sawtooth" style: bit 7 of byte 0 is
// the first bit in the frame, followed by bits 6..0, then bit 15 (bit 7 of byte 1), etc.
// XOR with 7 flips the bit-within-byte, giving a linear index counted from the start of the frame.
static size_t MotorolaLinearIndex(size_t bitpos)
{
    return bitpos ^ 7;
}

/*static*/ size_t Dbc::BytesRequired(Endian endianness, uint16_t bitpos, uint8_t length)
{
    if (endianness == Endian::Little_Intel)
    {
        return (bitpos + length + 7) / 8;
    }
    else
    {
        size_t lsb = MotorolaLinearIndex(bitpos) + length - 1;
        return lsb / 8 + 1;
    }
}

/*static*/ uint64_t Dbc::GetSignalBits(std::span<const uint8_t> data, Endian endianness, uint16_t bitpos, uint8_t length)
{
    // Walk from the signal's LSB towards its MSB, a byte at a time. Within a byte, significance
    // increases with bit number for both endians; they differ in which way the next byte is.
    int byte;
    int bit;
    int byteStep;

    if (endianness == Endian::Little_Intel)
    {
        // bitpos is the LSB, more significant bits are in later bytes
        byte = bitpos / 8;
        bit = bitpos % 8;
        byteStep = 1;
    }
    else
    {
        // bitpos is the MSB, more significant bits are in earlier bytes
        size_t lsb = MotorolaLinearIndex(MotorolaLinearIndex(bitpos) + length - 1);
        byte = lsb / 8;
        bit = lsb % 8;
        byteStep = -1;
    }

    uint64_t result = 0;

    for (int got = 0; got < length; )
    {
        int take = std::min(8 - bit, length - got);
        uint64_t chunk = (data[byte] >> bit) & ((1u << take) - 1);

        result |= chunk << got;

        got += take;
        byte += byteStep;
        bit = 0;
    }

    return result;
}

namespace libdbc::impl
{

FileReader::FileReader(std::istream& file)
    : m_file(file)
{
}

void FileReader::AdvanceLine()
{
    bool lineGood = false;

    do
    {
        // Read a line
        if (!std::getline(m_file, m_lineBuffer))
        {
            // End of file (or read error), clear the string and break
            m_lineBuffer.clear();
            m_line = m_lineBuffer;
            break;
        }

        m_line = m_lineBuffer;

        // A line is good only if it contains non-whitespace (skip blank lines)
        for (char c : m_line)
        {
            if (!std::isspace(static_cast<unsigned char>(c)))
            {
                lineGood = true;
                break;
            }
        }
    } while (!lineGood);

    // Strip any newline content and whitespace off the end
    while (m_line.size() && std::isspace(static_cast<unsigned char>(m_line[m_line.size() - 1])))
    {
        m_line.remove_suffix(1);
    }

    // Trim any leading whitespace
    while (m_line.size() && std::isspace(static_cast<unsigned char>(m_line[0])))
    {
        m_line.remove_prefix(1);
    }
}

std::string_view FileReader::Line() const
{
    return m_line;
}

}
