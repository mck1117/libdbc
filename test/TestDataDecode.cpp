#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <libdbc/libdbc.h>

#include <bit>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

using ::testing::_;
using ::testing::FloatEq;
using ::testing::InSequence;
using ::testing::MockFunction;

static std::unique_ptr<libdbc::Dbc> ParseDbcFile(const char* name)
{
    std::ifstream f(std::string(TEST_DATA_DIR) + name);
    return libdbc::Dbc::Parse(f);
}

static std::unique_ptr<libdbc::Dbc> ParseDbcString(const std::string& text)
{
    std::istringstream s(text);
    return libdbc::Dbc::Parse(s);
}

// Decodes a frame, returning scaled values by signal name
static std::map<std::string, float> DecodeToMap(const libdbc::Dbc& dbc, const libdbc::CanFrame& frame)
{
    std::map<std::string, float> result;
    dbc.Decode(frame, [&](const libdbc::Signal& s, uint64_t, float value)
    {
        result[s.Name] = value;
    });
    return result;
}

TEST(Decode, GmTorque)
{
    MockFunction<void(const libdbc::Signal& signal, uint64_t bitsValue, float scaledValue)> decode;

    {
        InSequence s;
        EXPECT_CALL(decode, Call(_, 0, 0));
        EXPECT_CALL(decode, Call(_, 1707, FloatEq(5.5f)));
        EXPECT_CALL(decode, Call(_, 0, 0));
        EXPECT_CALL(decode, Call(_, 0, 0));
        EXPECT_CALL(decode, Call(_, 1707, FloatEq(5.5f)));
        EXPECT_CALL(decode, Call(_, 0, 0));
        EXPECT_CALL(decode, Call(_, 0, 0));
        EXPECT_CALL(decode, Call(_, 0, 0));
        EXPECT_CALL(decode, Call(_, 2, 2));
        EXPECT_CALL(decode, Call(_, 0, 0));
    }

    libdbc::CanFrame f{ 0x1c3, false, 8, { 0x46, 0xab, 0x06, 0xab, 0xff, 0x00, 0x00, 0x00 } };

    auto dbc = ParseDbcFile("VolvoCanBus.dbc");
    ASSERT_NE(nullptr, dbc);
    dbc->Decode(f, decode.AsStdFunction());
}

TEST(Decode, UnknownFrame)
{
    auto dbc = ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1+ (1,0) [0|0] \"\" X\n");
    ASSERT_NE(nullptr, dbc);

    EXPECT_TRUE(DecodeToMap(*dbc, { 101, false, 8, { 1 } }).empty());
}

TEST(Decode, ExtendedFrame)
{
    auto dbc = ParseDbcFile("VolvoCanBus.dbc");
    ASSERT_NE(nullptr, dbc);

    // VolvoPowerSteeringIgnitionOn, 0x1AE0092C extended
    libdbc::CanFrame ext{ 0x1AE0092C, true, 8, { 0x00, 0x2A } };
    auto values = DecodeToMap(*dbc, ext);
    ASSERT_EQ(1, values.count("EhpasIgnitionRollCount"));
    EXPECT_EQ(42, values["EhpasIgnitionRollCount"]);

    // The same ID as a standard frame is a different message
    libdbc::CanFrame notExt{ 0x1AE0092C, false, 8, { 0x00, 0x2A } };
    EXPECT_TRUE(DecodeToMap(*dbc, notExt).empty());
}

TEST(Decode, ShortFrame)
{
    auto dbc = ParseDbcString(
        "BO_ 100 M: 8 ECM\n"
        " SG_ Byte0 : 0|8@1+ (1,0) [0|0] \"\" X\n"
        " SG_ Bytes12 : 8|16@1+ (1,0) [0|0] \"\" X\n"
        " SG_ Byte7 : 63|8@0+ (1,0) [0|0] \"\" X\n");
    ASSERT_NE(nullptr, dbc);

    // Garbage after the DLC must not be decoded
    libdbc::CanFrame f{ 100, false, 2, { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 } };

    auto values = DecodeToMap(*dbc, f);
    EXPECT_EQ(1, values.size());
    EXPECT_EQ(0x11, values["Byte0"]);

    f.Dlc = 3;
    values = DecodeToMap(*dbc, f);
    EXPECT_EQ(2, values.size());
    EXPECT_EQ(0x3322, values["Bytes12"]);

    f.Dlc = 8;
    EXPECT_EQ(3, DecodeToMap(*dbc, f).size());

    f.Dlc = 0;
    EXPECT_TRUE(DecodeToMap(*dbc, f).empty());

    // A bogus DLC beyond the end of the data must not read past it
    f.Dlc = 255;
    EXPECT_EQ(3, DecodeToMap(*dbc, f).size());
}

TEST(Decode, Multiplexed)
{
    auto dbc = ParseDbcFile("VolvoCanBus.dbc");
    ASSERT_NE(nullptr, dbc);

    // OBD2_Resp: PID 6 is STFT1, PID 8 is STFT2, both in byte 3
    libdbc::CanFrame f{ 2024, false, 8, { 0x03, 0x41, 0x06, 0x80 } };

    auto values = DecodeToMap(*dbc, f);
    EXPECT_EQ(1, values.count("OBD2_Resp_DataBytes"));
    EXPECT_EQ(1, values.count("OBD2_Resp_Service"));
    EXPECT_EQ(6, values["OBD2_Resp_Pid"]);
    EXPECT_FLOAT_EQ(0, values["OBD2_STFT1"]);
    EXPECT_EQ(0, values.count("OBD2_STFT2"));

    f.Data[2] = 0x08;
    values = DecodeToMap(*dbc, f);
    EXPECT_EQ(0, values.count("OBD2_STFT1"));
    EXPECT_FLOAT_EQ(0, values["OBD2_STFT2"]);

    // No signal for this PID
    f.Data[2] = 0x0C;
    values = DecodeToMap(*dbc, f);
    EXPECT_EQ(0, values.count("OBD2_STFT1"));
    EXPECT_EQ(0, values.count("OBD2_STFT2"));
    EXPECT_EQ(3, values.size());
}

TEST(Decode, Multiplexed_MultiplexorNotInFrame)
{
    auto dbc = ParseDbcString(
        "BO_ 100 M: 8 ECM\n"
        " SG_ Always : 0|8@1+ (1,0) [0|0] \"\" X\n"
        " SG_ Sel M : 56|8@1+ (1,0) [0|0] \"\" X\n"
        " SG_ Zero m0 : 8|8@1+ (1,0) [0|0] \"\" X\n");
    ASSERT_NE(nullptr, dbc);

    // Frame is too short for the multiplexor, so nothing multiplexed is present, even m0
    auto values = DecodeToMap(*dbc, { 100, false, 2, { 1, 2 } });
    EXPECT_EQ(1, values.size());
    EXPECT_EQ(1, values.count("Always"));

    values = DecodeToMap(*dbc, { 100, false, 8, { 1, 2 } });
    EXPECT_EQ(3, values.size());
    EXPECT_EQ(2, values["Zero"]);
}

TEST(Decode, Float32)
{
    auto dbc = ParseDbcString(
        "BO_ 100 M: 8 ECM\n"
        " SG_ F : 0|32@1- (2,1) [0|0] \"\" X\n"
        " SG_ FBig : 39|32@0- (1,0) [0|0] \"\" X\n"
        "SIG_VALTYPE_ 100 F : 1;\n"
        "SIG_VALTYPE_ 100 FBig : 1;\n");
    ASSERT_NE(nullptr, dbc);

    uint32_t bits = std::bit_cast<uint32_t>(-12.25f);

    libdbc::CanFrame f{ 100, false, 8, {
        // Little endian
        uint8_t(bits), uint8_t(bits >> 8), uint8_t(bits >> 16), uint8_t(bits >> 24),
        // Big endian
        uint8_t(bits >> 24), uint8_t(bits >> 16), uint8_t(bits >> 8), uint8_t(bits) } };

    auto values = DecodeToMap(*dbc, f);
    EXPECT_FLOAT_EQ(-12.25f * 2 + 1, values["F"]);
    EXPECT_FLOAT_EQ(-12.25f, values["FBig"]);
}

TEST(Decode, Float64)
{
    auto dbc = ParseDbcString(
        "BO_ 100 M: 8 ECM\n"
        " SG_ D : 0|64@1- (0.5,0) [0|0] \"\" X\n"
        "SIG_VALTYPE_ 100 D : 2;\n");
    ASSERT_NE(nullptr, dbc);

    uint64_t bits = std::bit_cast<uint64_t>(1234.5);

    libdbc::CanFrame f{ 100, false, 8, {} };
    for (int i = 0; i < 8; i++)
    {
        f.Data[i] = uint8_t(bits >> (8 * i));
    }

    EXPECT_FLOAT_EQ(617.25f, DecodeToMap(*dbc, f)["D"]);
}

TEST(Decode, SixtyFourBitInteger)
{
    auto dbc = ParseDbcString("BO_ 100 M: 8 ECM\n SG_ I : 0|64@1- (1,0) [0|0] \"\" X\n");
    ASSERT_NE(nullptr, dbc);

    libdbc::CanFrame f{ 100, false, 8, { 0x85, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } };

    uint64_t rawBits = 0;
    dbc->Decode(f, [&](const libdbc::Signal&, uint64_t bits, float value)
    {
        rawBits = bits;
        EXPECT_EQ(-123, value);
    });

    EXPECT_EQ(0xFFFFFFFFFFFFFF85ULL, rawBits);
}

TEST(Decode, CanFd)
{
    auto dbc = ParseDbcString(
        "BO_ 100 Fd: 64 ECM\n"
        " SG_ Low : 0|8@1+ (1,0) [0|0] \"\" X\n"
        " SG_ High : 504|8@1+ (1,0) [0|0] \"\" X\n"
        " SG_ Mid : 327|16@0+ (1,0) [0|0] \"\" X\n");
    ASSERT_NE(nullptr, dbc);

    libdbc::CanFrame f{ 100, false, 64, {} };
    f.Data[0] = 1;
    f.Data[40] = 0x12;
    f.Data[41] = 0x34;
    f.Data[63] = 0xAB;

    auto values = DecodeToMap(*dbc, f);
    EXPECT_EQ(1, values["Low"]);
    EXPECT_EQ(0x1234, values["Mid"]);
    EXPECT_EQ(0xAB, values["High"]);

    // A classic frame with the same ID only carries the first signal
    f.Dlc = 8;
    values = DecodeToMap(*dbc, f);
    EXPECT_EQ(1, values.size());
    EXPECT_EQ(1, values.count("Low"));
}
