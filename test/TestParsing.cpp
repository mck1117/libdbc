#include <gtest/gtest.h>

#include <libdbc/libdbc.h>

#include <fstream>

TEST(Parsing, ParseInt)
{
    EXPECT_EQ(0, libdbc::util::ParseInt("0"));
    EXPECT_EQ(0, libdbc::util::ParseInt("-0"));
    EXPECT_EQ(1, libdbc::util::ParseInt("1"));
    EXPECT_EQ(-1, libdbc::util::ParseInt("-1"));

    EXPECT_EQ(10, libdbc::util::ParseInt("10"));
    EXPECT_EQ(100000, libdbc::util::ParseInt("100000"));
    EXPECT_EQ(-10, libdbc::util::ParseInt("-10"));
    EXPECT_EQ(-100000, libdbc::util::ParseInt("-100000"));

    EXPECT_EQ(12345, libdbc::util::ParseInt("12345"));
    EXPECT_EQ(1234567890, libdbc::util::ParseInt("01234567890"));
}

TEST(Parsing, ParseFloat)
{
    EXPECT_EQ(0, libdbc::util::ParseFloat("0"));
    EXPECT_EQ(1, libdbc::util::ParseFloat("1"));
    EXPECT_EQ(-1, libdbc::util::ParseFloat("-1"));

    EXPECT_EQ(10, libdbc::util::ParseFloat("10"));
    EXPECT_EQ(100000, libdbc::util::ParseFloat("100000"));
    EXPECT_EQ(-10, libdbc::util::ParseFloat("-10"));
    EXPECT_EQ(-100000, libdbc::util::ParseFloat("-100000"));

    EXPECT_EQ(12345, libdbc::util::ParseFloat("12345"));
    EXPECT_EQ(1234567890, libdbc::util::ParseFloat("01234567890"));


    EXPECT_FLOAT_EQ(0.1f, libdbc::util::ParseFloat("0.1"));
    EXPECT_FLOAT_EQ(123.45f, libdbc::util::ParseFloat("123.45"));
    EXPECT_FLOAT_EQ(0.0001f, libdbc::util::ParseFloat("0.0001"));
    EXPECT_FLOAT_EQ(0.00048828125f, libdbc::util::ParseFloat("0.00048828125"));
    EXPECT_FLOAT_EQ(0.11111111f, libdbc::util::ParseFloat("0.111111111111111111111111"));

    EXPECT_FLOAT_EQ(-0.1f, libdbc::util::ParseFloat("-0.1"));
    EXPECT_FLOAT_EQ(-123.45f, libdbc::util::ParseFloat("-123.45"));
    EXPECT_FLOAT_EQ(-0.0001f, libdbc::util::ParseFloat("-0.0001"));
}

static std::unique_ptr<libdbc::Dbc> ParseDbcFile(const char* path)
{
    std::ifstream f(path);
    return libdbc::ParseDbcFile(f);
}

TEST(Parsing, OneMessage_OneSignal)
{
    auto dbc = ParseDbcFile("dbc/Test_OneMessage_OneSignal.dbc");
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
    // EXPECT_EQ("test unit", signal.Unit);
}

TEST(Parsing, RealVolvoDbc)
{
    auto dbc = ParseDbcFile("dbc/VolvoCanBus.dbc");
    ASSERT_NE(nullptr, dbc);
}
