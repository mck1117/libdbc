#include <gtest/gtest.h>

#include <libdbc/libdbc.h>

#include <algorithm>
#include <fstream>
#include <sstream>

using libdbc::Endian;
using libdbc::MuxRole;
using libdbc::ValueType;

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

// Parses a DBC that should contain exactly one message, and returns its only signal
static const libdbc::Signal& OnlySignal(const std::unique_ptr<libdbc::Dbc>& dbc)
{
    EXPECT_EQ(1, dbc->Messages().size());
    const auto& signals = dbc->Messages().begin()->second.Signals;
    EXPECT_EQ(1, signals.size());
    return signals.at(0);
}

TEST(Parsing, OneMessage_OneSignal)
{
    auto dbc = ParseDbcFile("Test_OneMessage_OneSignal.dbc");
    ASSERT_NE(nullptr, dbc);

    const auto& msgs = dbc->Messages();

    EXPECT_EQ(1, msgs.size());

    auto findResult = msgs.find(123);
    ASSERT_NE(findResult, msgs.end());

    const auto& msg = (*findResult).second;

    // Check message header info
    EXPECT_EQ(123, msg.Id);
    EXPECT_FALSE(msg.Extended);
    EXPECT_EQ("TestMessage", msg.Name);
    EXPECT_EQ(4, msg.Dlc);

    // Check that our one signal is there
    ASSERT_EQ(1, msg.Signals.size());
    const auto& signal = msg.Signals[0];
    EXPECT_EQ("TestSignal", signal.Name);
    EXPECT_EQ(Endian::Little_Intel, signal.Endianness);
    EXPECT_FALSE(signal.Signed);
    EXPECT_EQ(5, signal.Bitpos);
    EXPECT_EQ(6, signal.Length);
    EXPECT_EQ(7, signal.Factor);
    EXPECT_EQ(8, signal.Offset);
    EXPECT_EQ(9, signal.Min);
    EXPECT_EQ(10, signal.Max);
    EXPECT_EQ("test unit", signal.Unit);
    EXPECT_EQ(MuxRole::None, signal.Mux);
    EXPECT_EQ(ValueType::Integer, signal.Type);
}

TEST(Parsing, RealVolvoDbc)
{
    auto dbc = ParseDbcFile("VolvoCanBus.dbc");
    ASSERT_NE(nullptr, dbc);

    EXPECT_EQ(dbc->Messages().size(), 50);
    EXPECT_EQ(dbc->SignalCount(), 179);
}

TEST(Parsing, MissingFile)
{
    EXPECT_EQ(nullptr, ParseDbcFile("does_not_exist.dbc"));
}

TEST(Parsing, EmptyFile)
{
    auto dbc = ParseDbcString("");
    ASSERT_NE(nullptr, dbc);
    EXPECT_EQ(0, dbc->Messages().size());
    EXPECT_EQ(0, dbc->SignalCount());
}

TEST(Parsing, LongLines)
{
    std::string longComment = "CM_ BO_ 100 \"" + std::string(1000, 'x') + "\";\n";
    std::string longReceivers = std::string(500, 'R') + "," + std::string(500, 'S');

    auto dbc = ParseDbcString(
        longComment +
        "BO_ 100 After: 8 ECM\n"
        " SG_ A : 0|8@1+ (1,0) [0|0] \"\" " + longReceivers + "\n"
        " SG_ B : 8|8@1+ (1,0) [0|0] \"\" X\n" +
        longComment);

    ASSERT_NE(nullptr, dbc);
    ASSERT_EQ(1, dbc->Messages().size());
    EXPECT_EQ(2, dbc->Messages().at(100).Signals.size());
}

TEST(Parsing, MultiLineComment)
{
    // The comment's second line must not be parsed as a message
    auto dbc = ParseDbcString(
        "BO_ 100 Real: 8 ECM\n"
        " SG_ A : 0|8@1+ (1,0) [0|0] \"\" X\n"
        "CM_ BO_ 100 \"first line\n"
        "BO_ 200 Fake: 8 ECM\n"
        "with a \\\"quote\\\" and a ; in it\";\n"
        "CM_ SG_ 100 A \"another\";\n");

    ASSERT_NE(nullptr, dbc);
    EXPECT_EQ(1, dbc->Messages().size());
    EXPECT_EQ(1, dbc->Messages().count(100));
}

TEST(Parsing, CrLfLineEndings)
{
    auto dbc = ParseDbcString("BO_ 100 M: 8 ECM\r\n SG_ T : 0|8@1+ (1,0) [0|0] \"u\" X\r\n\r\n");
    ASSERT_NE(nullptr, dbc);
    EXPECT_EQ("u", OnlySignal(dbc).Unit);
}

TEST(Parsing, WhitespaceVariations)
{
    // Tabs, extra spaces, and spaces around punctuation are all legal
    auto dbc = ParseDbcString("BO_\t100   M :  8\tECM\n\t SG_  T  :  0 | 8 @ 1+  ( 1 , -40 )  [ -40 | 215 ]  \"C\"  X , Y\n");
    ASSERT_NE(nullptr, dbc);

    const auto& msg = dbc->Messages().at(100);
    EXPECT_EQ("M", msg.Name);
    EXPECT_EQ(8, msg.Dlc);

    const auto& s = OnlySignal(dbc);
    EXPECT_EQ("T", s.Name);
    EXPECT_EQ(1, s.Factor);
    EXPECT_EQ(-40, s.Offset);
    EXPECT_EQ(-40, s.Min);
    EXPECT_EQ(215, s.Max);
    EXPECT_EQ("C", s.Unit);
}

TEST(Parsing, NoWhitespaceAroundPunctuation)
{
    auto dbc = ParseDbcString("BO_ 100 M:8 ECM\n SG_ T:0|8@1+(1,0)[0|0]\"\"X\n");
    ASSERT_NE(nullptr, dbc);
    EXPECT_EQ("T", OnlySignal(dbc).Name);
}

TEST(Parsing, NumberFormats)
{
    auto dbc = ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1- (+2.5,-1E-005) [-1.5e2|+3] \"\" X\n");
    ASSERT_NE(nullptr, dbc);

    const auto& s = OnlySignal(dbc);
    EXPECT_TRUE(s.Signed);
    EXPECT_FLOAT_EQ(2.5f, s.Factor);
    EXPECT_FLOAT_EQ(-1e-5f, s.Offset);
    EXPECT_FLOAT_EQ(-150, s.Min);
    EXPECT_FLOAT_EQ(3, s.Max);
}

TEST(Parsing, BadNumbers)
{
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1+ (abc,0) [0|0] \"\" X\n"));
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1+ (1,0x10) [0|0] \"\" X\n"));
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1+ (1,+-1) [0|0] \"\" X\n"));
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : -1|8@1+ (1,0) [0|0] \"\" X\n"));
    EXPECT_EQ(nullptr, ParseDbcString("BO_ abc M: 8 ECM\n"));
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8x ECM\n"));
}

TEST(Parsing, BadBitSpec)
{
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@2+ (1,0) [0|0] \"\" X\n"));
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1 (1,0) [0|0] \"\" X\n"));
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1* (1,0) [0|0] \"\" X\n"));
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0 8@1+ (1,0) [0|0] \"\" X\n"));
}

TEST(Parsing, MalformedSignal)
{
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ junk\n"));
    // Missing unit
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1+ (1,0) [0|0] X\n"));
    // Missing min/max
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1+ (1,0) \"\" X\n"));
    // Trailing junk
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1+ (1,0) [0|0] \"\" X ) \n"));
}

TEST(Parsing, MalformedMessage)
{
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M 8 ECM\n"));
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: ECM\n"));
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100\n"));
}

TEST(Parsing, UnterminatedString)
{
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\nCM_ BO_ 100 \"never ends;\n"));
}

TEST(Parsing, SignalWithoutMessage)
{
    EXPECT_EQ(nullptr, ParseDbcString(" SG_ T : 0|8@1+ (1,0) [0|0] \"\" X\n"));

    // Signals must directly follow their message
    EXPECT_EQ(nullptr, ParseDbcString(
        "BO_ 100 M: 8 ECM\n"
        "CM_ BO_ 100 \"comment\";\n"
        " SG_ T : 0|8@1+ (1,0) [0|0] \"\" X\n"));
}

TEST(Parsing, BlankLinesBetweenSignals)
{
    auto dbc = ParseDbcString("BO_ 100 M: 8 ECM\n SG_ A : 0|8@1+ (1,0) [0|0] \"\" X\n\n SG_ B : 8|8@1+ (1,0) [0|0] \"\" X\n");
    ASSERT_NE(nullptr, dbc);
    EXPECT_EQ(2, dbc->Messages().at(100).Signals.size());
}

TEST(Parsing, DuplicateMessage)
{
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 A: 8 ECM\nBO_ 100 B: 8 ECM\n"));
}

TEST(Parsing, DuplicateSignal)
{
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1+ (1,0) [0|0] \"\" X\n SG_ T : 8|8@1+ (1,0) [0|0] \"\" X\n"));
}

TEST(Parsing, SameSignalNameInDifferentMessages)
{
    auto dbc = ParseDbcString(
        "BO_ 100 A: 8 ECM\n SG_ T : 0|8@1+ (1,0) [0|0] \"\" X\n"
        "BO_ 200 B: 8 ECM\n SG_ T : 0|8@1+ (1,0) [0|0] \"\" X\n");
    ASSERT_NE(nullptr, dbc);
    EXPECT_EQ(2, dbc->SignalCount());
}

TEST(Parsing, CanFdDlc)
{
    auto dbc = ParseDbcString("BO_ 100 Fd: 64 ECM\n SG_ T : 300|8@1+ (1,0) [0|0] \"\" X\n");
    ASSERT_NE(nullptr, dbc);

    EXPECT_EQ(64, dbc->Messages().at(100).Dlc);
    EXPECT_EQ(300, OnlySignal(dbc).Bitpos);

    auto dbc12 = ParseDbcString("BO_ 100 Fd: 12 ECM\n");
    ASSERT_NE(nullptr, dbc12);
    EXPECT_EQ(12, dbc12->Messages().at(100).Dlc);
}

TEST(Parsing, DlcTooLarge)
{
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 65 ECM\n"));
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 256 ECM\n"));
}

TEST(Parsing, ExtendedId)
{
    // 0x9AE0092C = extended flag | 0x1AE0092C
    auto dbc = ParseDbcString("BO_ 2598373676 Ext: 8 BCM\nBO_ 291 Std: 8 BCM\n");
    ASSERT_NE(nullptr, dbc);

    const auto* ext = dbc->FindMessage(0x1AE0092C, true);
    ASSERT_NE(nullptr, ext);
    EXPECT_EQ(0x1AE0092C, ext->Id);
    EXPECT_TRUE(ext->Extended);
    EXPECT_EQ("Ext", ext->Name);

    // Key in the map is the DBC's ID, with the flag
    EXPECT_EQ(1, dbc->Messages().count(0x9AE0092C));

    const auto* standard = dbc->FindMessage(291, false);
    ASSERT_NE(nullptr, standard);
    EXPECT_FALSE(standard->Extended);

    // Standard and extended IDs are distinct
    EXPECT_EQ(nullptr, dbc->FindMessage(0x1AE0092C, false));
    EXPECT_EQ(nullptr, dbc->FindMessage(291, true));
}

TEST(Parsing, StandardAndExtendedWithSameId)
{
    auto dbc = ParseDbcString("BO_ 256 Std: 8 BCM\nBO_ 2147483904 Ext: 8 BCM\n");
    ASSERT_NE(nullptr, dbc);

    EXPECT_EQ("Std", dbc->FindMessage(256, false)->Name);
    EXPECT_EQ("Ext", dbc->FindMessage(256, true)->Name);
}

TEST(Parsing, InvalidIds)
{
    // Standard ID larger than 11 bits
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 2048 M: 8 ECM\n"));
    // Bits set above the 29 bit extended range
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 2684354560 M: 8 ECM\n"));
    // Doesn't fit in 32 bits
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 4294967296 M: 8 ECM\n"));
}

TEST(Parsing, IndependentSignalsMessageSkipped)
{
    auto dbc = ParseDbcString(
        "BO_ 100 A: 8 ECM\n SG_ First : 0|8@1+ (1,0) [0|0] \"\" X\n"
        "BO_ 3221225472 VECTOR__INDEPENDENT_SIG_MSG: 0 Vector__XXX\n"
        " SG_ Orphan : 0|8@1+ (1,0) [0|0] \"\" X\n"
        " SG_ Orphan2 : 8|8@1+ (1,0) [0|0] \"\" X\n"
        "BO_ 200 B: 8 ECM\n SG_ Second : 0|8@1+ (1,0) [0|0] \"\" X\n"
        "SIG_VALTYPE_ 3221225472 Orphan : 1;\n");

    ASSERT_NE(nullptr, dbc);
    EXPECT_EQ(2, dbc->Messages().size());

    // Signal IDs stay contiguous
    EXPECT_EQ(2, dbc->SignalCount());
    EXPECT_EQ(0, dbc->Messages().at(100).Signals[0].Id);
    EXPECT_EQ(1, dbc->Messages().at(200).Signals[0].Id);
}

TEST(Parsing, SignalFitsMessage)
{
    auto parse = [](const char* bitspec, int dlc)
    {
        return ParseDbcString("BO_ 100 M: " + std::to_string(dlc) + " ECM\n SG_ T : " + bitspec + " (1,0) [0|0] \"\" X\n");
    };

    EXPECT_NE(nullptr, parse("0|8@1+", 1));
    EXPECT_EQ(nullptr, parse("0|8@1+", 0));
    EXPECT_NE(nullptr, parse("56|8@1+", 8));
    EXPECT_EQ(nullptr, parse("57|8@1+", 8));
    EXPECT_EQ(nullptr, parse("60|8@1+", 8));
    EXPECT_EQ(nullptr, parse("0|8@1+", 0));

    // Motorola: MSB at bit 3 of byte 0, 12 bits long ends in byte 1
    EXPECT_NE(nullptr, parse("3|12@0+", 2));
    EXPECT_EQ(nullptr, parse("3|12@0+", 1));
    EXPECT_NE(nullptr, parse("63|8@0+", 8));
    // MSB at bit 0 of the last byte, runs off the end
    EXPECT_EQ(nullptr, parse("56|2@0+", 8));

    EXPECT_NE(nullptr, parse("0|64@1+", 8));
    EXPECT_NE(nullptr, parse("7|64@0+", 8));
    EXPECT_EQ(nullptr, parse("1|64@1+", 8));
    EXPECT_EQ(nullptr, parse("6|64@0+", 8));

    EXPECT_EQ(nullptr, parse("0|0@1+", 8));
    EXPECT_EQ(nullptr, parse("0|65@1+", 8));
    EXPECT_EQ(nullptr, parse("512|8@1+", 64));
}

TEST(Parsing, Multiplexed)
{
    auto dbc = ParseDbcString(
        "BO_ 100 M: 8 ECM\n"
        " SG_ Always : 56|8@1+ (1,0) [0|0] \"\" X\n"
        " SG_ Sel M : 0|8@1+ (1,0) [0|0] \"\" X\n"
        " SG_ A m1 : 8|8@1+ (1,0) [0|0] \"\" X\n"
        " SG_ B m200 : 8|16@1+ (1,0) [0|0] \"\" X\n");

    ASSERT_NE(nullptr, dbc);
    const auto& s = dbc->Messages().at(100).Signals;
    ASSERT_EQ(4, s.size());

    EXPECT_EQ(MuxRole::None, s[0].Mux);
    EXPECT_EQ(MuxRole::Multiplexor, s[1].Mux);
    EXPECT_EQ(MuxRole::Multiplexed, s[2].Mux);
    EXPECT_EQ(1, s[2].MuxValue);
    EXPECT_EQ(MuxRole::Multiplexed, s[3].Mux);
    EXPECT_EQ(200, s[3].MuxValue);
}

TEST(Parsing, MultiplexorAfterMultiplexed)
{
    auto dbc = ParseDbcString(
        "BO_ 100 M: 8 ECM\n"
        " SG_ A m1 : 8|8@1+ (1,0) [0|0] \"\" X\n"
        " SG_ Sel M : 0|8@1+ (1,0) [0|0] \"\" X\n");
    EXPECT_NE(nullptr, dbc);
}

TEST(Parsing, BadMultiplexing)
{
    // Multiplexed signal with no multiplexor
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ A m1 : 8|8@1+ (1,0) [0|0] \"\" X\n"));
    // Two multiplexors
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ A M : 0|8@1+ (1,0) [0|0] \"\" X\n SG_ B M : 8|8@1+ (1,0) [0|0] \"\" X\n"));
    // Extended multiplexing isn't supported
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ A M : 0|8@1+ (1,0) [0|0] \"\" X\n SG_ B m1M : 8|8@1+ (1,0) [0|0] \"\" X\n"));
    // Junk in the multiplexer position
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ A M : 0|8@1+ (1,0) [0|0] \"\" X\n SG_ B mx : 8|8@1+ (1,0) [0|0] \"\" X\n"));
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ A M : 0|8@1+ (1,0) [0|0] \"\" X\n SG_ B m : 8|8@1+ (1,0) [0|0] \"\" X\n"));
    EXPECT_EQ(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ A Q : 0|8@1+ (1,0) [0|0] \"\" X\n"));
}

TEST(Parsing, ValueTypes)
{
    auto dbc = ParseDbcString(
        "BO_ 100 M: 16 ECM\n"
        " SG_ F : 0|32@1- (1,0) [0|0] \"\" X\n"
        " SG_ D : 32|64@1- (1,0) [0|0] \"\" X\n"
        " SG_ I : 96|32@1- (1,0) [0|0] \"\" X\n"
        "\n"
        "SIG_VALTYPE_ 100 F : 1;\n"
        "SIG_VALTYPE_ 100 D : 2 ;\n"
        "SIG_VALTYPE_ 100 I : 0;\n");

    ASSERT_NE(nullptr, dbc);
    const auto& s = dbc->Messages().at(100).Signals;
    EXPECT_EQ(ValueType::Float32, s[0].Type);
    EXPECT_EQ(ValueType::Float64, s[1].Type);
    EXPECT_EQ(ValueType::Integer, s[2].Type);
}

TEST(Parsing, BadValueTypes)
{
    const std::string msg = "BO_ 100 M: 8 ECM\n SG_ S : 0|16@1+ (1,0) [0|0] \"\" X\n SG_ F : 32|32@1+ (1,0) [0|0] \"\" X\n";

    // Float must be 32 bits, double must be 64
    EXPECT_EQ(nullptr, ParseDbcString(msg + "SIG_VALTYPE_ 100 S : 1;\n"));
    EXPECT_EQ(nullptr, ParseDbcString(msg + "SIG_VALTYPE_ 100 F : 2;\n"));
    // Unknown type
    EXPECT_EQ(nullptr, ParseDbcString(msg + "SIG_VALTYPE_ 100 F : 3;\n"));
    // Unknown message or signal
    EXPECT_EQ(nullptr, ParseDbcString(msg + "SIG_VALTYPE_ 101 F : 1;\n"));
    EXPECT_EQ(nullptr, ParseDbcString(msg + "SIG_VALTYPE_ 100 G : 1;\n"));
    // Missing semicolon
    EXPECT_EQ(nullptr, ParseDbcString(msg + "SIG_VALTYPE_ 100 F : 1\n"));
}

TEST(Parsing, NewSymbolsList)
{
    // Keywords listed in NS_ (including ones we parse) must not be treated as statements
    auto dbc = ParseDbcString(
        "VERSION \"\"\n"
        "\n"
        "NS_ : NS_DESC_\n"
        "\tCM_\n"
        "\tSIG_VALTYPE_\n"
        "\tBO_TX_BU_\n"
        "\n"
        "\tSG_MUL_VAL_\n"
        "\n"
        "BS_:\n"
        "BU_: ECM BCM\n"
        "BO_ 100 M: 8 ECM\n"
        " SG_ T : 0|8@1+ (1,0) [0|0] \"\" X\n");

    ASSERT_NE(nullptr, dbc);
    EXPECT_EQ("T", OnlySignal(dbc).Name);
}

TEST(Parsing, IgnoredStatements)
{
    auto dbc = ParseDbcString(
        "BO_ 100 M: 8 ECM\n"
        " SG_ T : 0|8@1+ (1,0) [0|0] \"\" X\n"
        "BO_TX_BU_ 100 : ECM,BCM;\n"
        "BA_DEF_ BO_ \"GenMsgCycleTime\" INT 0 65535;\n"
        "BA_ \"GenMsgCycleTime\" BO_ 100 10;\n"
        "VAL_ 100 T 0 \"Off\" 1 \"On\" ;\n"
        "VAL_TABLE_ Table 1 \"BO_ 1 X: 8 Y\" 0 \"SG_\" ;\n"
        "SG_MUL_VAL_ 100 T Sel 1-1;\n");

    ASSERT_NE(nullptr, dbc);
    EXPECT_EQ(1, dbc->Messages().size());
}

TEST(Parsing, UnitEscapes)
{
    auto dbc = ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1+ (1,0) [0|0] \"a \\\"b\\\" \\\\c\" X\n");
    ASSERT_NE(nullptr, dbc);
    EXPECT_EQ("a \"b\" \\c", OnlySignal(dbc).Unit);
}

TEST(Parsing, ReceiverFormats)
{
    EXPECT_NE(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1+ (1,0) [0|0] \"\" A,B,C\n"));
    EXPECT_NE(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1+ (1,0) [0|0] \"\"  A, B ,C\n"));
    EXPECT_NE(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1+ (1,0) [0|0] \"\" A B\n"));
    EXPECT_NE(nullptr, ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1+ (1,0) [0|0] \"\"\n"));
}

TEST(Parsing, NoTrailingNewline)
{
    auto dbc = ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1+ (1,0) [0|0] \"\" X");
    ASSERT_NE(nullptr, dbc);
    EXPECT_EQ("T", OnlySignal(dbc).Name);
}

TEST(Parsing, OnSignalOrder)
{
    std::istringstream s(
        "BO_ 300 C: 8 ECM\n SG_ C0 : 0|8@1+ (1,0) [0|0] \"\" X\n SG_ C1 : 8|8@1+ (1,0) [0|0] \"\" X\n"
        "BO_ 100 A: 8 ECM\n SG_ A0 : 0|8@1+ (1,0) [0|0] \"\" X\n"
        "BO_ 200 B: 8 ECM\n SG_ B0 : 0|8@1+ (1,0) [0|0] \"\" X\n");

    std::vector<std::string> names;
    auto dbc = libdbc::Dbc::Parse(s, [&](const libdbc::Signal& sig)
    {
        EXPECT_EQ(names.size(), sig.Id);
        names.push_back(sig.Name);
    });

    ASSERT_NE(nullptr, dbc);
    EXPECT_EQ((std::vector<std::string>{ "C0", "C1", "A0", "B0" }), names);
}

TEST(Parsing, OnSignalNotCalledOnFailure)
{
    std::istringstream s("BO_ 100 A: 8 ECM\n SG_ A0 : 0|8@1+ (1,0) [0|0] \"\" X\nBO_ 100 A: 8 ECM\n");

    bool called = false;
    auto dbc = libdbc::Dbc::Parse(s, [&](const libdbc::Signal&) { called = true; });

    EXPECT_EQ(nullptr, dbc);
    EXPECT_FALSE(called);
}

TEST(Parsing, ValueDescriptions)
{
    auto dbc = ParseDbcString(
        "BO_ 100 M: 8 ECM\n"
        " SG_ State : 0|2@1+ (1,0) [0|3] \"\" X\n"
        " SG_ Other : 8|8@1+ (1,0) [0|0] \"\" X\n"
        "VAL_ 100 State 0 \"Off\" 1 \"On\" 3 \"a \\\"quoted\\\" name\" ;\n");
    ASSERT_NE(nullptr, dbc);

    const auto& state = dbc->Messages().at(100).Signals[0];
    EXPECT_EQ(3, state.ValueNames.size());
    ASSERT_NE(nullptr, state.ValueName(0));
    EXPECT_EQ("Off", *state.ValueName(0));
    EXPECT_EQ("On", *state.ValueName(1));
    EXPECT_EQ(nullptr, state.ValueName(2));
    EXPECT_EQ("a \"quoted\" name", *state.ValueName(3));

    EXPECT_TRUE(dbc->Messages().at(100).Signals[1].ValueNames.empty());
}

TEST(Parsing, ValueDescriptions_Signed)
{
    // Tools disagree on whether a signed signal's values are written signed or unsigned: accept both
    auto dbc = ParseDbcString(
        "BO_ 100 M: 8 ECM\n"
        " SG_ Speed : 0|16@1- (0.1,0) [0|0] \"km/h\" X\n"
        " SG_ Small : 16|4@1- (1,0) [0|0] \"\" X\n"
        "VAL_ 100 Speed -32768 \"Invalid\" -1 \"Error\" ;\n"
        "VAL_ 100 Small 8 \"Unsigned form\" -2 \"Signed form\" ;\n");
    ASSERT_NE(nullptr, dbc);

    const auto& speed = dbc->Messages().at(100).Signals[0];
    EXPECT_EQ("Invalid", *speed.ValueName(0x8000));
    EXPECT_EQ("Error", *speed.ValueName(0xFFFF));

    const auto& small = dbc->Messages().at(100).Signals[1];
    EXPECT_EQ("Unsigned form", *small.ValueName(0x8));
    EXPECT_EQ("Signed form", *small.ValueName(0xE));
}

TEST(Parsing, ValueDescriptions_OutOfRangeDropped)
{
    auto dbc = ParseDbcString(
        "BO_ 100 M: 8 ECM\n"
        " SG_ U : 0|4@1+ (1,0) [0|0] \"\" X\n"
        " SG_ S : 8|4@1- (1,0) [0|0] \"\" X\n"
        "VAL_ 100 U 15 \"Max\" 16 \"Too big\" ;\n"
        "VAL_ 100 S -8 \"Min\" -9 \"Too small\" ;\n");
    ASSERT_NE(nullptr, dbc);

    const auto& u = dbc->Messages().at(100).Signals[0];
    EXPECT_EQ(1, u.ValueNames.size());
    EXPECT_EQ("Max", *u.ValueName(15));

    const auto& s = dbc->Messages().at(100).Signals[1];
    EXPECT_EQ(1, s.ValueNames.size());
    EXPECT_EQ("Min", *s.ValueName(8));
}

TEST(Parsing, ValueDescriptions_SixtyFourBit)
{
    auto dbc = ParseDbcString(
        "BO_ 100 M: 8 ECM\n"
        " SG_ Big : 0|64@1+ (1,0) [0|0] \"\" X\n"
        "VAL_ 100 Big 18446744073709551615 \"All ones\" -2 \"Signed\" 0 \"Zero\" ;\n");
    ASSERT_NE(nullptr, dbc);

    const auto& big = OnlySignal(dbc);
    EXPECT_EQ("All ones", *big.ValueName(~0ULL));
    EXPECT_EQ("Signed", *big.ValueName(~1ULL));
    EXPECT_EQ("Zero", *big.ValueName(0));
}

TEST(Parsing, ValueDescriptions_Wrapped)
{
    auto dbc = ParseDbcString(
        "BO_ 100 M: 8 ECM\n"
        " SG_ T : 0|8@1+ (1,0) [0|0] \"\" X\n"
        "VAL_ 100 T 0 \"Zero\"\n"
        "  1 \"One\"\n"
        "  2 \"Two\" ;\n");
    ASSERT_NE(nullptr, dbc);
    EXPECT_EQ(3, OnlySignal(dbc).ValueNames.size());
}

TEST(Parsing, ValueDescriptions_Empty)
{
    auto dbc = ParseDbcString("BO_ 100 M: 8 ECM\n SG_ T : 0|8@1+ (1,0) [0|0] \"\" X\nVAL_ 100 T ;\n");
    ASSERT_NE(nullptr, dbc);
    EXPECT_TRUE(OnlySignal(dbc).ValueNames.empty());
}

TEST(Parsing, ValueDescriptions_Skipped)
{
    auto dbc = ParseDbcString(
        "BO_ 100 M: 8 ECM\n"
        " SG_ T : 0|8@1+ (1,0) [0|0] \"\" X\n"
        "BO_ 3221225472 VECTOR__INDEPENDENT_SIG_MSG: 0 Vector__XXX\n"
        " SG_ Orphan : 0|8@1+ (1,0) [0|0] \"\" X\n"
        // Environment variable
        "VAL_ SomeEnvVar 0 \"Off\" 1 \"On\" ;\n"
        // Signal in the independent signals message
        "VAL_ 3221225472 Orphan 0 \"Off\" ;\n"
        // Value tables are only definitions
        "VAL_TABLE_ OnOff 1 \"On\" 0 \"Off\" ;\n");
    ASSERT_NE(nullptr, dbc);
    EXPECT_TRUE(OnlySignal(dbc).ValueNames.empty());
}

TEST(Parsing, BadValueDescriptions)
{
    const std::string msg = "BO_ 100 M: 8 ECM\n SG_ T : 0|8@1+ (1,0) [0|0] \"\" X\n";

    // Unknown message or signal
    EXPECT_EQ(nullptr, ParseDbcString(msg + "VAL_ 101 T 0 \"Off\" ;\n"));
    EXPECT_EQ(nullptr, ParseDbcString(msg + "VAL_ 100 U 0 \"Off\" ;\n"));
    // Not an integer
    EXPECT_EQ(nullptr, ParseDbcString(msg + "VAL_ 100 T 1.5 \"Off\" ;\n"));
    EXPECT_EQ(nullptr, ParseDbcString(msg + "VAL_ 100 T x \"Off\" ;\n"));
    // Missing description
    EXPECT_EQ(nullptr, ParseDbcString(msg + "VAL_ 100 T 0 1 \"On\" ;\n"));
    // Missing semicolon
    EXPECT_EQ(nullptr, ParseDbcString(msg + "VAL_ 100 T 0 \"Off\"\n"));
}

TEST(Parsing, RealDbcValueDescriptions)
{
    auto dbc = ParseDbcFile("MK60e5_CAN.dbc");
    ASSERT_NE(nullptr, dbc);

    const auto& signals = dbc->FindMessage(206, false)->Signals;
    auto fl = std::find_if(signals.begin(), signals.end(), [](const libdbc::Signal& s) { return s.Name == "WheelSpeedFL"; });
    ASSERT_NE(fl, signals.end());
    ASSERT_NE(nullptr, fl->ValueName(0x8000));
    EXPECT_EQ("Invalid", *fl->ValueName(0x8000));
}
