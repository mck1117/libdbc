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
        auto line = m_file.Line();

        if (!line.size())
        {
            // end of input file
            return std::nullopt;
        }


        CommaSplitter splitter(line);

        uint64_t timestamp = std::stoul(splitter.Next());

        libdbc::CanFrame frame;
        frame.Id = libdbc::util::from_sv<uint32_t>(splitter.Next(), 16);

        // burn std/ext
        splitter.Next();
        // burn tx/rx
        splitter.Next();
        // burn bus
        splitter.Next();

        frame.Dlc = libdbc::util::from_sv<uint8_t>(splitter.Next(), 10);

        for (size_t i = 0; i < 8; i++)
        {
            frame.Data8[i] = libdbc::util::from_sv<uint8_t>(splitter.Next(), 16);
        }

        return std::pair<uint64_t, libdbc::CanFrame>{ timestamp, frame };
    }

private:
    libdbc::impl::FileReader& m_file;
};


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

    libdbc::impl::FileReader inputReader(inputFile);
    FrameParser parser(inputReader);

    std::vector<float> data;
    data.resize(dbc->SignalCount());
    std::vector<float> lastData;
    lastData.resize(dbc->SignalCount());

    size_t frameCount = 0;
    size_t logLineCount = 0;
    size_t skipCount = 0;

    auto startTime = std::chrono::steady_clock::now();

    while (true)
    {
        auto opt_frame = parser.GetFrame();

        if (!opt_frame)
        {
            // end of file
            break;
        }

        const auto& [timestamp, frame] = opt_frame.value();

        frameCount++;

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

        if (dataChange)
        {
            outFile << (timestamp * 1e-3) << ",0";

            for (size_t i = 0; i < data.size(); i++)
            {
                // only if the data changed write the value
                if (data[i] != lastData[i] || !sparse)
                {
                    constexpr size_t sz = 128;
                    char buf[sz];
                    buf[0] = ',';
                    const auto res = std::to_chars(buf + 1, buf + sz, data[i]);

                    outFile << std::string_view(buf, res.ptr);

                    lastData[i] = data[i];
                }
                else
                {
                    outFile << ',';
                }
            }

            // std::endl forces a flush - just write a newline instead
            outFile << '\n';

            logLineCount++;
        }
        else
        {
            skipCount++;
        }
    }

    // Flush so we get an accurate time estimate
    outFile << std::flush;

    auto endTime = std::chrono::steady_clock::now();
    float durationSec = 1e-9f * (endTime - startTime).count();

    std::cout << "Done! Processed " << frameCount << " frames, wrote " << logLineCount << " log entries and skipped " << skipCount << " duplicate lines." << std::endl;
    std::cout << "Duration " << durationSec << " s, rate " << (1e-3f * frameCount / durationSec) << " kfps";

    return 0;
}
