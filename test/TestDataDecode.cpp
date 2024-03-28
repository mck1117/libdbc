#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <libdbc/libdbc.h>

#include <fstream>

using ::testing::_;
using ::testing::FloatEq;
using ::testing::InSequence;
using ::testing::MockFunction;

static std::unique_ptr<libdbc::Dbc> ParseDbcFile(const char* path)
{
    std::ifstream f(path);
    return libdbc::Dbc::Parse(f);
}

TEST(Decode, GmTorque)
{
    MockFunction<void(const libdbc::Signal& signal, uint64_t bitsValue, float scaledValue)> decode;

    {
        InSequence s;
        EXPECT_CALL(decode, Call(_, 0, 0));
        EXPECT_CALL(decode, Call(_, 1707, FloatEq(5.5f)));
    }

    libdbc::CanFrame f{ 0x1c3, 8, { { 0x46, 0xab, 0x06, 0xab, 0xff, 0x00, 0x00, 0x00} }};

    auto dbc = ParseDbcFile("data/VolvoCanBus.dbc");
    dbc->Decode(f, decode.AsStdFunction());
}
