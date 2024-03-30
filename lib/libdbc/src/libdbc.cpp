#include <libdbc/libdbc.h>

#include <bit>
#include <cstring>

using namespace libdbc;
using namespace libdbc::impl;

void Dbc::Decode(const CanFrame& frame, DecoderFunc onDecoded) const
{
    auto search = m_messages.find(frame.Id);

    // Do we know how to decode this frame?
    if (search == m_messages.end())
    {
        // If not, bail out early
        return;
    }

    const auto& message = search->second;

    // Decode every signal in the message
    for (const auto& signal : message.Signals)
    {
        auto bits = GetSignalBits(frame.Data64, signal.Endianness, signal.Bitpos, signal.Length);

        float val = ToFloat(bits, signal.Length, signal.Signed);

        val = signal.Factor * val + signal.Offset;

        if (onDecoded)
        {
            onDecoded(signal, bits, val);
        }
    }
}

/*static*/ float Dbc::ToFloat(uint32_t bits, uint8_t length, bool isSigned)
{
    // if signed, sign extend as necessary
    if (isSigned)
    {
        auto signBit = bits & (1UL << (length - 1));
        uint32_t mask = signBit ? ~((signBit) - 1) : 0;

        bits |= mask;

        int32_t signedBits;
        static_assert(sizeof(bits) == sizeof(signedBits));
        memcpy(&signedBits, &bits, sizeof(bits));

        return static_cast<float>(signedBits);
    }
    else
    {
        return static_cast<float>(bits);
    }
}

uint64_t swap_uint64_t(uint64_t x) {
    x = (x & 0x00000000FFFFFFFF) << 32 | (x & 0xFFFFFFFF00000000) >> 32;
    x = (x & 0x0000FFFF0000FFFF) << 16 | (x & 0xFFFF0000FFFF0000) >> 16;
    x = (x & 0x00FF00FF00FF00FF) << 8  | (x & 0xFF00FF00FF00FF00) >> 8;
    return x;
}

// https://github.com/collin80/SavvyCAN/blob/77f889e4d765d82242f200f4c1187f99a67eff9e/dbc/dbc_classes.cpp#L97

/*static*/ uint32_t Dbc::GetSignalBits(uint64_t data, Endian endianness, uint8_t bitpos, uint8_t length)
{
    uint64_t mask = (1ULL << length) - 1;

    uint8_t shift;

    if (endianness == Endian::Little_Intel)
    {
        // Little endian is easy, just shift by bitpos
        shift = bitpos;
    }
    else
    {
        data = swap_uint64_t(data);

        // I don't love this, but it works!?
        const int8_t shiftCorrection[8] = { -7, -5, -3, -1, 1, 3, 5, 7 };
        shift = 64 - bitpos - length + shiftCorrection[bitpos % 8];
    }

    return static_cast<uint32_t>((data >> shift) & mask);
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
        if (!m_file.good() || m_file.eof())
        {
            // If the file is empty, clear the string and break
            m_lineBuffer[0] = '\0';
            m_line = m_lineBuffer;
            break;
        }

        // Read a line
        m_file.getline(m_lineBuffer, std::size(m_lineBuffer));
        m_line = m_lineBuffer;

        // skip empty string
        if (!m_line.size())
        {
            continue;
        }

        // A line is good only if it contains non-whitespace (skip blank lines)
        for (char c : m_line)
        {
            if (!std::isspace(c))
            {
                lineGood = true;
                break;
            }
        }
    } while (!lineGood);

    // Strip any newline content and whitespace off the end
    while (m_line.size() && std::isspace(m_line[m_line.size() - 1]))
    {
        m_line.remove_suffix(1);
    }

    // Trim any leading whitespace
    while (m_line.size() && std::isspace(m_line[0]))
    {
        m_line.remove_prefix(1);
    }
}

std::string_view FileReader::Line() const
{
    return m_line;
}

}
