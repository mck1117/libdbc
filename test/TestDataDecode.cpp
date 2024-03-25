#include <gtest/gtest.h>
#include <libdbc/libdbc.h>

#include <fstream>

static std::unique_ptr<libdbc::Dbc> ParseDbcFile(const char* path)
{
    std::ifstream f(path);
    return libdbc::ParseDbcFile(f);
}


TEST(Decode, GmTorque)
{
    auto decodeFunc = [](const libdbc::Signal&, float){};

    libdbc::CanFrame f{ 0x1c3, { 0x46, 0xab, 0x06, 0xab, 0xff, 0x00, 0x00, 0x00 }};

    auto dbc = ParseDbcFile("dbc/VolvoCanBus.dbc");
    dbc->Decode(f, decodeFunc);


}
