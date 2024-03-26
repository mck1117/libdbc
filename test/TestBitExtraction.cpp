#include <gtest/gtest.h>

#include <libdbc/libdbc.h>

TEST(Decode, SingleByte_LE_Aligned)
{
    union
    {
        uint8_t data8[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
        uint64_t data64;
    };

    EXPECT_EQ(0x11, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 0, 8));
    EXPECT_EQ(0x22, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 8, 8));
    EXPECT_EQ(0x33, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 16, 8));
    EXPECT_EQ(0x44, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 24, 8));
    EXPECT_EQ(0x55, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 32, 8));
    EXPECT_EQ(0x66, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 40, 8));
    EXPECT_EQ(0x77, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 48, 8));
    EXPECT_EQ(0x88, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 56, 8));
}

TEST(Decode, SingleByte_BE_Aligned)
{
    union
    {
        uint8_t data8[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
        uint64_t data64;
    };

    EXPECT_EQ(0x11, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 7, 8));
    EXPECT_EQ(0x22, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 15, 8));
    EXPECT_EQ(0x33, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 23, 8));
    EXPECT_EQ(0x44, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 31, 8));
    EXPECT_EQ(0x55, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 39, 8));
    EXPECT_EQ(0x66, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 47, 8));
    EXPECT_EQ(0x77, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 55, 8));
    EXPECT_EQ(0x88, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 63, 8));
}

TEST(Decode, TwoBytes_LE_Aligned)
{
    union
    {
        uint8_t data8[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
        uint64_t data64;
    };

    EXPECT_EQ(0x2211, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 0, 16));
    EXPECT_EQ(0x3322, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 8, 16));
    EXPECT_EQ(0x4433, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 16, 16));
    EXPECT_EQ(0x5544, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 24, 16));
    EXPECT_EQ(0x6655, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 32, 16));
    EXPECT_EQ(0x7766, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 40, 16));
    EXPECT_EQ(0x8877, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 48, 16));
}

TEST(Decode, TwoBytes_BE_Aligned)
{
    union
    {
        uint8_t data8[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
        uint64_t data64;
    };

    EXPECT_EQ(0x1122, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 7, 16));
    EXPECT_EQ(0x2233, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 15, 16));
    EXPECT_EQ(0x3344, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 23, 16));
    EXPECT_EQ(0x4455, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 31, 16));
    EXPECT_EQ(0x5566, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 39, 16));
    EXPECT_EQ(0x6677, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 47, 16));
    EXPECT_EQ(0x7788, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 55, 16));
}

TEST(Decode, FourBytes_LE_Aligned)
{
    union
    {
        uint8_t data8[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
        uint64_t data64;
    };

    EXPECT_EQ(0x44332211, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 0, 32));
    EXPECT_EQ(0x55443322, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 8, 32));
    EXPECT_EQ(0x66554433, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 16, 32));
    EXPECT_EQ(0x77665544, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 24, 32));
    EXPECT_EQ(0x88776655, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 32, 32));
}

TEST(Decode, FourBytes_BE_Aligned)
{
    union
    {
        uint8_t data8[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
        uint64_t data64;
    };

    EXPECT_EQ(0x11223344, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 7, 32));
    EXPECT_EQ(0x22334455, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 15, 32));
    EXPECT_EQ(0x33445566, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 23, 32));
    EXPECT_EQ(0x44556677, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 31, 32));
    EXPECT_EQ(0x55667788, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 39, 32));
}

TEST(Decode, SingleByte_LE_Misaligned)
{
    union
    {
        uint8_t data8[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
        uint64_t data64;
    };

    EXPECT_EQ(0x21, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 4, 8));
    EXPECT_EQ(0x32, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 12, 8));
    EXPECT_EQ(0x43, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 20, 8));
    EXPECT_EQ(0x54, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 28, 8));
    EXPECT_EQ(0x65, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 36, 8));
}

TEST(Decode, SingleByte_BE_Misaligned)
{
    union
    {
        uint8_t data8[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
        uint64_t data64;
    };

    EXPECT_EQ(0x12, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 3, 8));
    EXPECT_EQ(0x23, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 11, 8));
    EXPECT_EQ(0x34, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 19, 8));
    EXPECT_EQ(0x45, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 27, 8));
    EXPECT_EQ(0x56, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 35, 8));
}

TEST(Decode, FewBits_MidByte_LE)
{
    union
    {
        uint8_t data8[8] = { 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        uint64_t data64;
    };

    // Bottom half of 2nd byte
    EXPECT_EQ(0xC, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 8, 4));
    // Top half of 2nd byte
    EXPECT_EQ(0x3, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 12, 4));

    // Middle of 2nd byte
    EXPECT_EQ(0xF, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 10, 4));
}

TEST(Decode, FewBits_MidByte_BE)
{
    union
    {
        uint8_t data8[8] = { 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        uint64_t data64;
    };

    // Bottom half of 2nd byte
    EXPECT_EQ(0xC, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 11, 4));
    // Top half of 2nd byte
    EXPECT_EQ(0x3, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 15, 4));

    // Middle of 2nd byte
    EXPECT_EQ(0xF, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 13, 4));
}

TEST(Decode, FewBits_CrossByte_LE)
{
    union
    {
        uint8_t data8[8] = { 0x00, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 };
        uint64_t data64;
    };

    // Grab the 8 bits with two set in the middle: 00011000
    EXPECT_EQ(0x18, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 12, 8));

    // Grab the middle 4 bits: 0110
    EXPECT_EQ(0x6, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 14, 4));

    // Grab the middle 2 bits: 11
    EXPECT_EQ(0x3, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 15, 2));

    // Grab each single bit
    EXPECT_EQ(0, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 14, 1));
    EXPECT_EQ(1, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 15, 1));
    EXPECT_EQ(1, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 16, 1));
    EXPECT_EQ(0, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 17, 1));
}

TEST(Decode, FewBits_CrossByte_BE)
{
    union
    {
        uint8_t data8[8] = { 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        uint64_t data64;
    };

    // Grab the 8 bits with two set in the middle: 00011000
    EXPECT_EQ(0x18, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 3, 8));

    // Grab the middle 4 bits: 0110
    EXPECT_EQ(0x6, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 1, 4));

    // Grab the middle 2 bits: 11
    EXPECT_EQ(0x3, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 0, 2));

    // Grab each single bit
    EXPECT_EQ(0, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 14, 1));
    EXPECT_EQ(1, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 15, 1));
    EXPECT_EQ(1, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 0, 1));
    EXPECT_EQ(0, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 1, 1));
}

TEST(Decode, SingleBits)
{
    union
    {
        uint8_t data8[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        uint64_t data64;
    };

    for (size_t byte = 0; byte < 8; byte++)
    {
        for (size_t bit = 0; bit < 8; bit++)
        {
            data8[byte] = 1 << bit;

            size_t bitIdx = 8 * byte + bit;

            // Big and little endian should give identical results for single bits
            EXPECT_EQ(1, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, bitIdx, 1)) << " byte " << byte << " bit " << bit << " index " << bitIdx;
            EXPECT_EQ(1, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, bitIdx, 1)) << " byte " << byte << " bit " << bit << " index " << bitIdx;
        }

        data8[byte] = 0;
    }
}

TEST(Decode, MultiByte_LE)
{
    union
    {
        uint8_t data8[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
        uint64_t data64;
    };

    // Aligned
    EXPECT_EQ(0x443322, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 8, 24));

    // Misaligned by 4 bits
    EXPECT_EQ(0x433221, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 4, 24));

    // Misaligned by 3 bits
    EXPECT_EQ(0x866442, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Little_Intel, 3, 24));
}

TEST(Decode, MultiByte_BE)
{
    union
    {
        uint8_t data8[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
        uint64_t data64;
    };

    // Aligned
    EXPECT_EQ(0x223344, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 15, 24));

    // Misaligned by 4 bits
    EXPECT_EQ(0x122334, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 3, 24));

    // Misaligned by 3 bits
    EXPECT_EQ(0x122334, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 2, 24));
}

TEST(Decode, Gm_Rpm)
{
    union
    {
        uint8_t data8[8] = { 0x84, 0x15, 0x4d, 0x0d, 0x00, 0x01, 0x18, 0x00 };
        uint64_t data64;
    };

    // shift 40

    EXPECT_EQ(0x154d, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 15, 16));
}

TEST(Decode, Gm_Torque)
{
    union
    {
        uint8_t data8[8] = { 0x46, 0xab, 0x06, 0xab, 0xff, 0x00, 0x00, 0x00 };
        uint64_t data64;
    };

    // shift 48

    // 46 AB 06 AB FF 00 00 00

    EXPECT_EQ(0x6ab, libdbc::Dbc::GetSignalBits(data64, libdbc::Endian::Big_Motorola, 3, 12));
}
