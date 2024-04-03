#include <libdbc/libdbc.h>

#include <charconv>

using namespace libdbc;
using namespace libdbc::impl;

struct MultiplexSpec
{
    MultiplexMode Mode;
    size_t MuxValue;
};

static MultiplexSpec ParseMultiplex(std::string_view str)
{
    if (str.empty())
    {
        return { MultiplexMode::None, 0 };
    }
    else if (str[0] == 'm')
    {
        return { MultiplexMode::MEquals, util::from_sv<size_t>(str.substr(1)) };
    }
    else
    {
        return { MultiplexMode::Multiplexor, 0 };
    }
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

    result.Bitpos = util::from_sv<uint8_t>(str.substr(0, bitposEnd));
    result.Length = util::from_sv<uint8_t>(str.substr(lengthStart, lengthEnd - lengthStart));
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
        util::from_sv<float>(str.substr(0, scaleEnd)),
        util::from_sv<float>(str.substr(offsetStart))
    };
}

static std::pair<float, float> ParseMinmax(std::string_view str)
{
    auto minEnd = str.find('|');

    auto maxStart = minEnd + 1;

    return
    {
        util::from_sv<float>(str.substr(0, minEnd)),
        util::from_sv<float>(str.substr(maxStart))
    };
}

/*static*/ std::unique_ptr<Dbc> Dbc::Parse(std::istream& file, std::function<void(const Signal& s)> onSignal)
{
    message_container messages;

    FileReader fr(file);

    Message* currentMessage = nullptr;

    uint32_t signalId = 0;

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
            uint32_t id = util::from_sv<uint32_t>(line.substr(idStart, idEnd - idStart));

            auto nameStart = idEnd + 1;
            auto nameEnd = line.find(':', nameStart);

            auto dlcStart = nameEnd + 2;
            uint8_t dlc = util::from_sv<uint8_t>(line.substr(dlcStart, 1));

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

            auto multiplexStart = nameEnd + 1;
            auto multiplexEnd = line.find(" : ");
            auto multiplexSpec = ParseMultiplex(multiplexEnd > multiplexStart ? line.substr(multiplexStart, multiplexEnd - multiplexStart) : "");

            auto bitSpecStart = multiplexEnd + 3;
            auto bitSpecEnd = line.find(' ', bitSpecStart);
            auto bitSpec = ParseBitSpec(line.substr(bitSpecStart, bitSpecEnd - bitSpecStart));

            auto scaleOffsetStart = line.find('(', bitSpecEnd) + 1;
            auto scaleOffsetEnd = line.find(')', scaleOffsetStart);
            auto scaleOffset = ParseScaleOffset(line.substr(scaleOffsetStart, scaleOffsetEnd - scaleOffsetStart));

            auto minmaxStart = line.find('[', scaleOffsetEnd) + 1;
            auto minmaxEnd = line.find(']', minmaxStart);
            auto minmax = ParseMinmax(line.substr(minmaxStart, minmaxEnd - minmaxStart));

            auto unitStart = line.find('"', minmaxEnd) + 1;
            auto unitEnd = line.find('"', unitStart);
            auto unit = line.substr(unitStart, unitEnd - unitStart);

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
                minmax.second,
                unit,
                multiplexSpec.Mode,
                multiplexSpec.MuxValue
            );

            if (onSignal)
            {
                onSignal(*(currentMessage->Signals.end() - 1));
            }
        }
        else
        {
            // didn't understand this line
        }
    }

    return std::make_unique<Dbc>(signalId, std::move(messages));
}
