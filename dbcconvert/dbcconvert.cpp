#include <libdbc/libdbc.h>

#include <iostream>
#include <fstream>

#include <numeric>
#include <regex>

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cout << "Usage: dbcconvert format.dbc source.csv dest.csv" << std::endl;
        return -1;
    }

    std::ofstream outFile(argv[3]);

    // Load DBC file

    std::unique_ptr<libdbc::Dbc> dbc;

    {
        std::ifstream dbcFile(argv[1]);

        if (!dbcFile.good())
        {
            std::cout << "DBC file \"" << argv[1] << "\" failed to open" << std::endl;
            return -2;
        }

        outFile << "\"Interval\"|\"ms\"|0|0|1,\"Utc\"|\"ms\"|0|0|1";

        dbc = libdbc::ParseDbcFile(dbcFile, [&](const libdbc::Signal& s)
            {
                outFile << ",\"" << s.Name << "\"|\"" << s.Unit << "\"|" << s.Min << '|' << s.Max  << '|' << "3";
            });

        outFile << std::endl;

        if (!dbc)
        {
            std::cout << "DBC file \"" << argv[1] << "\" failed to parse" << std::endl;
            return -3;
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

    std::ifstream inputFile(argv[2]);

    if (!inputFile.good())
    {
        std::cout << "Input file \"" << argv[2] << "\" failed to open" << std::endl;
        return -2;
    }

    libdbc::impl::FileReader inputReader(inputFile);

    const std::regex lineRegex("^([0-9]+),([0-9a-fA-F]+),[a-z]+,[A-Za-z]+,[0-9],([0-9]),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),$");

    std::vector<float> data;
    data.resize(dbc->SignalCount());
    std::vector<float> lastData;
    lastData.resize(dbc->SignalCount());

    while (true)
    {
        inputReader.AdvanceLine();
        auto line = inputReader.Line();

        if (!line.size())
        {
            // end of input file
            break;
        }

        std::string line2(line);

        std::smatch matches;
        if (std::regex_match(line2, matches, lineRegex))
        {
            libdbc::CanFrame frame;

            uint64_t timestamp = std::stoul(matches[1].str());
            (void)timestamp;

            frame.Id = std::stoul(matches[2].str(), nullptr, 16);
            frame.Dlc = std::stoul(matches[3].str());

            for (size_t i = 0; i < 8; i++)
            {
                frame.Data8[i] = std::stoul(matches[i + 4].str(), nullptr, 16);
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
                    if (data[i] != lastData[i])
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
            }
        }
        else
        {
            std::cout << "Skipping line " << line << std::endl;
        }
    }

    return 0;
}
