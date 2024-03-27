#include <libdbc/libdbc.h>

#include <iostream>
#include <fstream>

#include <cstring>
#include <charconv>
#include <numeric>

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

        dbc = libdbc::ParseDbcFile(dbcFile, [&](const libdbc::Signal& s)
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
    inputReader.AdvanceLine();

    std::vector<float> data;
    data.resize(dbc->SignalCount());
    std::vector<float> lastData;
    lastData.resize(dbc->SignalCount());

    size_t frameCount = 0;
    size_t logLineCount = 0;
    size_t skipCount = 0;

    while (true)
    {
        inputReader.AdvanceLine();
        auto line = inputReader.Line();

        if (!line.size())
        {
            // end of input file
            break;
        }

        libdbc::CanFrame frame;
        frameCount++;

        CommaSplitter splitter(line);

        uint64_t timestamp = std::stoul(splitter.Next());

        frame.Id = std::stoul(splitter.Next(), nullptr, 16);

        // burn std/ext
        splitter.Next();
        // burn tx/rx
        splitter.Next();
        // burn bus
        splitter.Next();

        frame.Dlc = std::stoul(splitter.Next());

        for (size_t i = 0; i < 8; i++)
        {
            frame.Data8[i] = std::stoul(splitter.Next(), nullptr, 16);
        }

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
                if (data[i] != lastData[i] && sparse)
                {
                    outFile << ',' << data[i];
                    lastData[i] = data[i];
                }
                else
                {
                    outFile << ',';
                }
            }

            outFile << std::endl;

            logLineCount++;
        }
        else
        {
            skipCount++;
        }
    }

    std::cout << "Done! Processed " << frameCount << " frames, wrote " << logLineCount << " log entries and skipped " << skipCount << " duplicate lines." << std::endl;

    return 0;
}
