#include <gtest/gtest.h>

#include <libdbc/libdbc.h>

#include <fstream>

static std::unique_ptr<libdbc::Dbc> ParseDbcFile(const char* path)
{
    std::ifstream f(path);
    return libdbc::Dbc::Parse(f);
}

TEST(Parsing, OneMessage_OneSignal)
{
    auto dbc = ParseDbcFile("data/Test_OneMessage_OneSignal.dbc");
    ASSERT_NE(nullptr, dbc);

    const auto& msgs = dbc->Messages();

    EXPECT_EQ(1, msgs.size());

    auto findResult = msgs.find(123);
    EXPECT_NE(findResult, msgs.end());

    const auto& msg = (*findResult).second;

    // Check message header info
    EXPECT_EQ(123, msg.Id);
    EXPECT_EQ("TestMessage", msg.Name);
    EXPECT_EQ(4, msg.Dlc);

    // Check that our one signal is there
    EXPECT_EQ(1, msg.Signals.size());
    const auto& signal = msg.Signals[0];
    EXPECT_EQ(5, signal.Bitpos);
    EXPECT_EQ(6, signal.Length);
    EXPECT_EQ(7, signal.Factor);
    EXPECT_EQ(8, signal.Offset);
    EXPECT_EQ(9, signal.Min);
    EXPECT_EQ(10, signal.Max);
    EXPECT_EQ("test unit", signal.Unit);
}

TEST(Parsing, RealVolvoDbc)
{
    auto dbc = ParseDbcFile("data/VolvoCanBus.dbc");
    ASSERT_NE(nullptr, dbc);
}
