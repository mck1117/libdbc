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
    ASSERT_EQ(1, msgs.size());

    auto findResult = msgs.find(123);
    ASSERT_NE(findResult, msgs.end());

    const auto& msg = (*findResult).second;

    // Check message header info
    EXPECT_EQ(123, msg.Id);
    EXPECT_EQ("TestMessage", msg.Name);
    EXPECT_EQ(4, msg.Dlc);

    // Check that our one signal is there
    EXPECT_EQ(1, msg.Signals.size());
    const auto& signal = msg.Signals[0];
    EXPECT_EQ("TestSignal", signal.Name);
    EXPECT_EQ(5, signal.Bitpos);
    EXPECT_EQ(6, signal.Length);
    EXPECT_EQ(7, signal.Factor);
    EXPECT_EQ(8, signal.Offset);
    EXPECT_EQ(9, signal.Min);
    EXPECT_EQ(10, signal.Max);
    EXPECT_EQ("test unit", signal.Unit);
    EXPECT_EQ(signal.MuxMode, libdbc::MultiplexMode::None);
}

TEST(Parsing, RealVolvoDbc)
{
    auto dbc = ParseDbcFile("data/VolvoCanBus.dbc");
    ASSERT_NE(nullptr, dbc);

    const auto& msgs = dbc->Messages();
    ASSERT_EQ(47, msgs.size());
}

TEST(Parsing, FileWithMultiplex)
{
    auto dbc = ParseDbcFile("data/Test_Multiplex.dbc");
    ASSERT_NE(nullptr, dbc);

    const auto& msgs = dbc->Messages();
    ASSERT_EQ(1, msgs.size());

    ASSERT_EQ(3, dbc->SignalCount());

    auto findResult = msgs.find(456);
    ASSERT_NE(findResult, msgs.end());

    const auto& msg = (*findResult).second;

    // Check message header info
    EXPECT_EQ(456, msg.Id);
    EXPECT_EQ("TestMessage", msg.Name);
    EXPECT_EQ(3, msg.Dlc);
    EXPECT_EQ(3, msg.Signals.size());

    {
        // Check the multiplexor
        const auto& signal = msg.Signals[0];
        EXPECT_EQ("SignalMux", signal.Name);
        EXPECT_EQ(0, signal.Bitpos);
        EXPECT_EQ(8, signal.Length);
        EXPECT_EQ(7, signal.Factor);
        EXPECT_EQ(8, signal.Offset);
        EXPECT_EQ(9, signal.Min);
        EXPECT_EQ(10, signal.Max);
        EXPECT_EQ("test unit", signal.Unit);

        EXPECT_EQ(signal.MuxMode, libdbc::MultiplexMode::Multiplexor);

        EXPECT_EQ(msg.MultiplexorSignalIndex, 0);
    }

    // Check the values
    {
        const auto& signal = msg.Signals[1];
        EXPECT_EQ("SignalMuxA", signal.Name);

        EXPECT_EQ(signal.MuxMode, libdbc::MultiplexMode::MEquals);
        EXPECT_EQ(signal.MuxVal, 5);
    }

    {
        const auto& signal = msg.Signals[2];
        EXPECT_EQ("HugeMEquals", signal.Name);

        EXPECT_EQ(signal.MuxMode, libdbc::MultiplexMode::MEquals);
        EXPECT_EQ(signal.MuxVal, 9999999);
    }
}
