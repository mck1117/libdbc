#include <libdbc/libdbc.h>

#include <iostream>
#include <fstream>

#include <numeric>
#inculde <regex>

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cout << "Usage: dbcconvert format.dbc source.csv dest.csv" << std::endl;
        return -1;
    }

    // Load DBC file

    std::unique_ptr<libdbc::Dbc> dbc;

    {
        std::ifstream dbcFile(argv[1]);

        if (!dbcFile.good())
        {
            std::cout << "DBC file \"" << argv[1] << "\" failed to open" << std::endl;
            return -2;
        }

        dbc = libdbc::ParseDbcFile(dbcFile);

        if (!dbc)
        {
            std::cout << "DBC file \"" << argv[1] << "\" failed to parse" << std::endl;
            return -3;
        }
    }

    {
        const auto& msgs = dbc->Messages();

        size_t signalCount = 0;
        for (const auto& [id, msg] : msgs)
        {
            signalCount += msg.Signals.size();
        }

        std::cout
            << "DBC loaded successfully! "
            << msgs.size()
            << " messages with a total of "
            << signalCount
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

    const std::regex lineRegex("");

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

        dbc->Decode(frame, nullptr);
    }

    return 0;
}
