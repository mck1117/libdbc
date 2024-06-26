#include <libdbc/libdbc.h>

#include <iostream>
#include <fstream>

#include <chrono>
#include <cstring>
#include <charconv>
#include <numeric>
#include <optional>
#include <string>

class CommaSplitter
{
public:
    CommaSplitter(std::string_view str)
    : m_str(str)
    {
    }

    std::string Next()
    {
        size_t next = m_str.find(',', m_current);

        auto result = m_str.substr(m_current, next - m_current);

        m_current = next + 1;

        return std::string{result};
    }

private:
    const std::string_view m_str;
    size_t m_current = 0;
};

class FrameParser
{
public:
    FrameParser(libdbc::impl::FileReader& file)
        : m_file(file)
    {
        m_file.AdvanceLine();
    }

    std::optional<std::pair<uint64_t, libdbc::CanFrame>> GetFrame()
    {
        m_file.AdvanceLine();
        std::string_view line = m_file.Line();

        if (!line.size())
        {
            // end of input file
            return std::nullopt;
        }

        CommaSplitter splitter(line);

        uint64_t timestamp = libdbc::util::from_sv<uint64_t>(splitter.Next());

        libdbc::CanFrame frame;
        frame.Id = libdbc::util::from_sv<uint32_t>(splitter.Next(), 16);

        // burn std/ext
        splitter.Next();
        // burn tx/rx
        splitter.Next();
        // burn bus
        splitter.Next();

        frame.Dlc = libdbc::util::from_sv<uint8_t>(splitter.Next());

        for (size_t i = 0; i < 8; i++)
        {
            frame.Data8[i] = libdbc::util::from_sv<uint8_t>(splitter.Next(), 16);
        }

        return std::optional{ std::pair<uint64_t, libdbc::CanFrame>{ timestamp, frame } };
    }

private:
    libdbc::impl::FileReader& m_file;
};

struct ConversionStat
{
    size_t FrameCount = 0;
    size_t LogLineCount = 0;
    size_t SkipCount = 0;
};

static ConversionStat doConversion(const libdbc::Dbc* dbc, std::istream& inputFile, std::ostream& outFile, bool sparse)
{
    ConversionStat stats;

    libdbc::impl::FileReader inputReader(inputFile);
    FrameParser parser(inputReader);

    std::vector<float> data;
    data.resize(dbc->SignalCount());
    std::vector<float> lastData;
    lastData.resize(dbc->SignalCount());

    while (true)
    {
        auto opt_frame = parser.GetFrame();

        if (!opt_frame)
        {
            // end of file
            break;
        }

        const auto& [timestamp, frame] = opt_frame.value();

        stats.FrameCount++;

        bool dataChange = false;

        dbc->Decode(frame, [&](const libdbc::Signal& s, uint64_t, float value)
        {
            // Only update if data changed
            if (data[s.Id] != value)
            {
                data[s.Id] = value;
                dataChange = true;
            }
        });

        if (dataChange) [[unlikely]]
        {
            // If you need a line longer than this, good luck
            static char lineBuffer[16 * 1024];
            char* const lineEnd = lineBuffer + sizeof(lineBuffer);
            char* linePtr = lineBuffer;

            auto res = std::to_chars(linePtr, lineEnd, (timestamp * 1e-3f));
            // TODO: check res.err
            linePtr = res.ptr;
            *linePtr++ = ',';
            *linePtr++ = '0';   // this 0 is the UTC field

            for (size_t i = 0; i < data.size(); i++)
            {
                *linePtr++ = ',';

                // only if the data changed write the value
                if (data[i] != lastData[i] || !sparse)
                {
                    res = std::to_chars(linePtr, lineEnd, data[i]);
                    // TODO: check res.err
                    linePtr = res.ptr;

                    lastData[i] = data[i];
                }
            }

            *linePtr++ = '\n';

            outFile << std::string_view(lineBuffer, linePtr - lineBuffer);

            stats.LogLineCount++;
        }
        else
        {
            stats.SkipCount++;
        }
    }

    return stats;
}

int main(int argc, char** argv)
{
    bool sparse = false;
    std::string dbcPath;
    std::string inputPath;
    std::string outputPath;

    for (int i = 1; i < argc; i++)
    {
        if (0 == std::strcmp(argv[i], "-sparse"))
        {
            sparse = true;
        }
        else if (0 == std::strcmp(argv[i], "-dbc"))
        {
            dbcPath = argv[++i];
        }
        else if (0 == std::strcmp(argv[i], "-in"))
        {
            inputPath = argv[++i];
        }
        else if (0 == std::strcmp(argv[i], "-out"))
        {
            outputPath = argv[++i];
        }
        else
        {
            std::cout << "Bad argument: " << argv[i] << std::endl;
            break;
        }
    }

    std::cout << "DBC: " << dbcPath << std::endl;
    std::cout << "Input file: " << inputPath << std::endl;
    std::cout << "Output file: " << outputPath << std::endl;

    if (!dbcPath.size() || !inputPath.size() || !outputPath.size())
    {
        std::cout << "Usage: dbcconvert -dbc <format.dbc> -in <source.csv> -out <dest.csv> [-sparse]" << std::endl;

        return -1;
    }

    std::ifstream inputFile(inputPath);

    if (!inputFile.good())
    {
        std::cout << "Input file \"" << inputPath << "\" failed to open" << std::endl;
        return -2;
    }

    std::ofstream outFile(outputPath);

    if (!outFile.good())
    {
        std::cout << "Failed to open output file " << outputPath << std::endl;
        return -3;
    }

    // Load DBC file

    std::unique_ptr<libdbc::Dbc> dbc;

    {
        std::ifstream dbcFile(dbcPath);

        if (!dbcFile.good())
        {
            std::cout << "DBC file \"" << dbcPath << "\" failed to open" << std::endl;
            return -4;
        }

        outFile << "\"Interval\"|\"ms\"|0|0|1,\"Utc\"|\"ms\"|0|0|1";

        dbc = libdbc::Dbc::Parse(dbcFile, [&](const libdbc::Signal& s)
            {
                outFile << ",\"" << s.Name << "\"|\"" << s.Unit << "\"|" << s.Min << '|' << s.Max  << '|' << "3";
            });

        outFile << std::endl;

        if (!dbc)
        {
            std::cout << "DBC file \"" << dbcPath << "\" failed to parse" << std::endl;
            return -5;
        }
    }

    {
        const auto& msgs = dbc->Messages();

        std::cout
            << "DBC loaded successfully! "
            << msgs.size()
            << " messages with a total of "
            << dbc->SignalCount()
            << " signals."
            << std::endl;
    }

    auto startTime = std::chrono::steady_clock::now();

    auto stats = doConversion(dbc.get(), inputFile, outFile, sparse);

    // Flush so we get an accurate time estimate
    outFile << std::flush;

    auto endTime = std::chrono::steady_clock::now();
    float durationSec = 1e-9f * (endTime - startTime).count();

    std::cout << "Done! Processed " << stats.FrameCount << " frames, wrote " << stats.LogLineCount << " log entries and skipped " << stats.SkipCount << " duplicate lines." << std::endl;
    std::cout << "Duration " << durationSec << " s, rate " << (1e-3f * stats.FrameCount / durationSec) << " kfps" << std::endl << std::endl;

    return 0;
}
