#include <libdbc/libdbc.h>

#include <bit>
#include <cstring>

using namespace libdbc;
using namespace libdbc::impl;

namespace libdbc
{
namespace util
{

int64_t ParseInt(std::string_view str)
{
    bool negative = str.size() && (str[0] == '-');

    size_t i = negative ? 1 : 0;

    int64_t result = 0;

    for (; i < str.size(); i++)
    {
        auto c = str[i];

        result = result * 10;
        result += (c - '0');
    }

    if (negative)
    {
        result *= -1;
    }

    return result;
}

float ParseFloat(std::string_view str)
{
    bool negative = str.size() && (str[0] == '-');

    size_t i = negative ? 1 : 0;

    int whole = 0;

    // Parse until we find the dot (if one exists)
    for (; i < str.size(); i++)
    {
        auto c = str[i];

        if (c == '.')
        {
            i++;
            break;
        }

        whole = whole * 10;
        whole += (c - '0');
    }

    uint32_t fraction = 0;
    uint32_t div = 1;

    for (; i < str.size(); i++)
    {
        auto c = str[i];

        fraction = fraction * 10;
        fraction += (c - '0');

        div *= 10;
    }

    float result = whole + ((float)fraction / div);

    if (negative)
    {
        result *= -1;
    }

    return result;
}

struct BitSpec
{
    Endian Endianness;
    uint8_t Bitpos;
    uint8_t Length;
    bool Signed;
};

static BitSpec ParseBitSpec(std::string_view str)
{
    BitSpec result;

    auto bitposEnd = str.find('|');

    auto lengthStart = bitposEnd + 1;
    auto lengthEnd = str.find('@', lengthStart);

    result.Bitpos = util::ParseInt(str.substr(0, bitposEnd));
    result.Length = util::ParseInt(str.substr(lengthStart, lengthEnd - lengthStart));
    result.Endianness = (str[str.size() - 2] == '1') ? Endian::Little_Intel : Endian::Big_Motorola;
    result.Signed = str[str.size() - 1] == '-';

    return result;
}

static std::pair<float, float> ParseScaleOffset(std::string_view str)
{
    auto scaleEnd = str.find(',');

    auto offsetStart = scaleEnd + 1;

    return
    {
        util::ParseFloat(str.substr(0, scaleEnd)),
        util::ParseFloat(str.substr(offsetStart))
    };
}

static std::pair<float, float> ParseMinmax(std::string_view str)
{
    auto minEnd = str.find('|');

    auto maxStart = minEnd + 1;

    return
    {
        util::ParseFloat(str.substr(0, minEnd)),
        util::ParseFloat(str.substr(maxStart))
    };
}

}

std::unique_ptr<Dbc> ParseDbcFile(std::istream& file)
{
    std::map<uint32_t, Message> messages;

    FileReader fr(file);

    Message* currentMessage = nullptr;

    uint32_t signalId = 100;

    while (true)
    {
        fr.AdvanceLine();
        auto line = fr.Line();

        if (!line.size())
        {
            break;
        }


        if (line.starts_with("NS_"))
        {

        }
        else if (line.starts_with("BS_"))
        {

        }
        else if (line.starts_with("BU_"))
        {
            
        }
        else if (line.starts_with("BO_TX_BU_") || line.starts_with("SG_MUL_VAL_"))
        {
            // TODO: handle this nicer
        }
        else if (line.starts_with("BO_"))
        {
            // Message header
            auto idStart = line.find(' ') + 1;
            auto idEnd = line.find(' ', idStart);
            uint32_t id = util::ParseInt(line.substr(idStart, idEnd - idStart));

            auto nameStart = idEnd + 1;
            auto nameEnd = line.find(':', nameStart);

            auto dlcStart = nameEnd + 2;
            uint8_t dlc = util::ParseInt(line.substr(dlcStart, 1));

            auto result = messages.try_emplace(id, id, dlc, line.substr(nameStart, nameEnd - nameStart));

            if (!result.second)
            {
                // Uh oh, duplicate message!
                return nullptr;
            }

            currentMessage = &(*result.first).second;
        }
        else if (line.starts_with("SG_"))
        {
            // Signal within a message
            if (!currentMessage)
            {
                // Signal with no message?
                return nullptr;
            }

            auto nameStart = line.find(' ') + 1;
            auto nameEnd = line.find(' ', nameStart);
            std::string_view nameString = line.substr(nameStart, nameEnd - nameStart);

            auto bitSpecStart = line.find(" : ") + 3;
            auto bitSpecEnd = line.find(' ', bitSpecStart);
            auto bitSpec = util::ParseBitSpec(line.substr(bitSpecStart, bitSpecEnd - bitSpecStart));

            auto scaleOffsetStart = line.find('(', bitSpecEnd) + 1;
            auto scaleOffsetEnd = line.find(')', scaleOffsetStart);
            auto scaleOffset = util::ParseScaleOffset(line.substr(scaleOffsetStart, scaleOffsetEnd - scaleOffsetStart));

            auto minmaxStart = line.find('[', scaleOffsetEnd) + 1;
            auto minmaxEnd = line.find(']', minmaxStart);
            auto minmax = util::ParseMinmax(line.substr(minmaxStart, minmaxEnd - minmaxStart));

            currentMessage->Signals.emplace_back(
                signalId++,
                std::string{nameString},
                bitSpec.Endianness,
                bitSpec.Signed,
                bitSpec.Bitpos,
                bitSpec.Length,
                scaleOffset.first,
                scaleOffset.second,
                minmax.first,
                minmax.second
            );
        }
        else
        {
            // didn't understand this line
        }
    }

    return std::make_unique<Dbc>(std::move(messages));
}

}

void Dbc::Decode(CanFrame& frame, DecoderFunc onDecoded) const
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

        // if signed, sign extend as necessary
        if (signal.Signed)
        {
            uint64_t signBit = bits & (1ULL << (signal.Length - 1));
        }

        float val = signal.Factor * bits + signal.Offset;

        onDecoded(signal, val);
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
        switch (bitpos % 8)
        {
            case 0:
                shift = 64 - bitpos - length - 7;
                break;
            case 1:
                shift = 64 - bitpos - length - 5;
                break;
            case 2:
                shift = 64 - bitpos - length - 3;
                break;
            case 3:
                shift = 64 - bitpos - length - 1;
                break;
            case 4:
                shift = 64 - bitpos - length + 1;
                break;
            case 5:
                shift = 64 - bitpos - length + 3;
                break;
            case 6:
                shift = 64 - bitpos - length + 5;
                break;
            case 7:
                shift = 64 - bitpos - length + 7;
                break;
        }
    }

    return (data >> shift) & mask;
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
