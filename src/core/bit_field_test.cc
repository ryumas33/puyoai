#include "core/bit_field.h"

#include <gtest/gtest.h>
#include "core/core_field.h"

using namespace std;

class BitFieldTest : public testing::Test {
protected:
    int vanish(BitField* bf, int chain, FieldBits* erased) {
        return bf->vanish(chain, erased);
    }

    void drop(BitField* bf, const FieldBits& erased) {
        bf->drop(erased);
    }
};

TEST_F(BitFieldTest, constructor1)
{
    BitField bf;

    for (int x = 1; x <= 6; ++x) {
        for (int y = 1; y <= 14; ++y) {
            EXPECT_TRUE(bf.isEmpty(x, y));
        }
    }

    EXPECT_TRUE(bf.isColor(0, 0, PuyoColor::WALL));
    EXPECT_TRUE(bf.isColor(7, 1, PuyoColor::WALL));
}

TEST_F(BitFieldTest, constructor2)
{
    PlainField pf(
        "OOOOOO"
        "GGGYYY"
        "RRRBBB");

    BitField bf(pf);

    for (int x = 1; x <= 6; ++x) {
        for (int y = 1; y <= 12; ++y) {
            PuyoColor c = pf.color(x, y);
            if (isNormalColor(c) || c == PuyoColor::OJAMA) {
                EXPECT_TRUE(bf.isColor(x, y, c)) << x << ' ' << y << ' ' << c;
                EXPECT_FALSE(bf.isEmpty(x, y));
            } else if (c == PuyoColor::EMPTY) {
                EXPECT_TRUE(bf.isEmpty(x, y));
            }
        }
    }

    EXPECT_TRUE(bf.isColor(0, 0, PuyoColor::WALL));
    EXPECT_TRUE(bf.isColor(7, 1, PuyoColor::WALL));
}

TEST_F(BitFieldTest, constructor3)
{
    BitField bf(
        "OOOOOO"
        "GGGYYY"
        "RRRBBB");

    EXPECT_TRUE(bf.isColor(1, 1, PuyoColor::RED));
    EXPECT_TRUE(bf.isColor(1, 2, PuyoColor::GREEN));
    EXPECT_TRUE(bf.isColor(1, 3, PuyoColor::OJAMA));
    EXPECT_TRUE(bf.isColor(4, 1, PuyoColor::BLUE));
    EXPECT_TRUE(bf.isColor(4, 2, PuyoColor::YELLOW));
    EXPECT_TRUE(bf.isColor(4, 3, PuyoColor::OJAMA));

    EXPECT_TRUE(bf.isColor(0, 0, PuyoColor::WALL));
    EXPECT_TRUE(bf.isColor(7, 1, PuyoColor::WALL));
}

TEST_F(BitFieldTest, setColor)
{
    static const PuyoColor colors[] = {
        PuyoColor::RED, PuyoColor::BLUE, PuyoColor::YELLOW, PuyoColor::GREEN,
        PuyoColor::OJAMA, PuyoColor::IRON
    };

    BitField bf;

    for (PuyoColor c : colors) {
        bf.setColor(1, 1, c);
        EXPECT_EQ(c, bf.color(1, 1)) << c;
        EXPECT_TRUE(bf.isColor(1, 1, c)) << c;
    }
}

TEST_F(BitFieldTest, isZenkeshi)
{
    BitField bf1;
    EXPECT_TRUE(bf1.isZenkeshi());

    BitField bf2("..O...");
    EXPECT_FALSE(bf2.isZenkeshi());

    BitField bf3("..R...");
    EXPECT_FALSE(bf3.isZenkeshi());
}

TEST_F(BitFieldTest, isConnectedPuyo)
{
    BitField bf(
        "B.B..Y"
        "RRRBBB");

    EXPECT_TRUE(bf.isConnectedPuyo(1, 1));
    EXPECT_TRUE(bf.isConnectedPuyo(2, 1));
    EXPECT_TRUE(bf.isConnectedPuyo(3, 1));
    EXPECT_TRUE(bf.isConnectedPuyo(4, 1));
    EXPECT_TRUE(bf.isConnectedPuyo(5, 1));
    EXPECT_TRUE(bf.isConnectedPuyo(6, 1));
    EXPECT_FALSE(bf.isConnectedPuyo(1, 2));
    EXPECT_FALSE(bf.isConnectedPuyo(3, 2));
    EXPECT_FALSE(bf.isConnectedPuyo(6, 2));
}

TEST_F(BitFieldTest, countConnectedPuyos)
{
    BitField bf(
        "RRRRRR"
        "BYBRRY"
        "RRRBBB");

    EXPECT_EQ(3, bf.countConnectedPuyos(1, 1));
    EXPECT_EQ(3, bf.countConnectedPuyos(4, 1));
    EXPECT_EQ(1, bf.countConnectedPuyos(1, 2));
    EXPECT_EQ(1, bf.countConnectedPuyos(3, 2));
    EXPECT_EQ(1, bf.countConnectedPuyos(6, 2));
    EXPECT_EQ(8, bf.countConnectedPuyos(4, 2));
}

TEST_F(BitFieldTest, countConnectedPuyosWithChecked)
{
    BitField bf(
        "RRRRRR"
        "BYBRRY"
        "RRRBBB");

    FieldBits checked;
    EXPECT_EQ(8, bf.countConnectedPuyos(1, 3, &checked));
    EXPECT_TRUE(checked.get(1, 3));
    EXPECT_TRUE(checked.get(4, 2));
    EXPECT_TRUE(checked.get(5, 2));
    EXPECT_TRUE(checked.get(6, 3));
    EXPECT_FALSE(checked.get(1, 1));
    EXPECT_FALSE(checked.get(3, 1));
    EXPECT_FALSE(checked.get(1, 2));
    EXPECT_FALSE(checked.get(6, 2));
}

TEST_F(BitFieldTest, countConnectedPuyosMax4)
{
    BitField bf(
        "RRRRRR"
        "BYBRRY"
        "RRRBBB");

    EXPECT_EQ(3, bf.countConnectedPuyosMax4(1, 1));
    EXPECT_EQ(3, bf.countConnectedPuyosMax4(4, 1));
    EXPECT_EQ(1, bf.countConnectedPuyosMax4(1, 2));
    EXPECT_EQ(1, bf.countConnectedPuyosMax4(3, 2));
    EXPECT_EQ(1, bf.countConnectedPuyosMax4(6, 2));
    EXPECT_LE(4, bf.countConnectedPuyosMax4(4, 2));
}

TEST_F(BitFieldTest, hasEmptyNeighbor)
{
    BitField bf(
        "RR..RR"
        "BYBRRY"
        "RRRBBB");

    EXPECT_TRUE(bf.hasEmptyNeighbor(2, 3));
    EXPECT_TRUE(bf.hasEmptyNeighbor(3, 2));

    EXPECT_FALSE(bf.hasEmptyNeighbor(1, 1));
    EXPECT_FALSE(bf.hasEmptyNeighbor(2, 1));
    EXPECT_FALSE(bf.hasEmptyNeighbor(6, 1));
}

TEST_F(BitFieldTest, fillSameColorPosition)
{
    BitField bf(
        "RRRRRR"
        "BYBRRY"
        "RRRBBB");
    Position ps[128];
    FieldBits checked;
    Position* head = bf.fillSameColorPosition(1, 3, PuyoColor::RED, ps, &checked);

    std::sort(ps, head);
    EXPECT_EQ(8, head - ps);

    EXPECT_EQ(Position(1, 3), ps[0]);
    EXPECT_EQ(Position(2, 3), ps[1]);
    EXPECT_EQ(Position(3, 3), ps[2]);
    EXPECT_EQ(Position(4, 2), ps[3]);
    EXPECT_EQ(Position(4, 3), ps[4]);
    EXPECT_EQ(Position(5, 2), ps[5]);
    EXPECT_EQ(Position(5, 3), ps[6]);
    EXPECT_EQ(Position(6, 3), ps[7]);
}

TEST_F(BitFieldTest, simulate1)
{
    BitField bf(
        ".BBBB.");

    RensaResult result = bf.simulate();
    EXPECT_EQ(1, result.chains);
    EXPECT_EQ(40, result.score);
    EXPECT_EQ(FRAMES_VANISH_ANIMATION, result.frames);
    EXPECT_TRUE(result.quick);

    for (int x = 0; x < FieldConstant::MAP_WIDTH; ++x) {
        EXPECT_TRUE(bf.isColor(x, 0, PuyoColor::WALL));
        EXPECT_TRUE(bf.isColor(x, 15, PuyoColor::WALL));
    }
}

TEST_F(BitFieldTest, simulate2)
{
    CoreField cf(
        "BBBBBB");
    BitField bf(cf);

    RensaResult result = bf.simulate();
    EXPECT_EQ(1, result.chains);
    EXPECT_EQ(60 * 3, result.score);
    EXPECT_EQ(FRAMES_VANISH_ANIMATION, result.frames);
    EXPECT_TRUE(result.quick);
}

TEST_F(BitFieldTest, simulate3)
{
    CoreField cf(
        "YYYY.."
        "BBBB..");
    BitField bf(cf);

    RensaResult result = bf.simulate();
    EXPECT_EQ(1, result.chains);
    EXPECT_EQ(80 * 3, result.score);
    EXPECT_EQ(FRAMES_VANISH_ANIMATION, result.frames);
    EXPECT_TRUE(result.quick);
}

TEST_F(BitFieldTest, simulate4)
{
    CoreField cf(
        "YYYYYY"
        "BBBBBB");
    BitField bf(cf);

    RensaResult result = bf.simulate();
    EXPECT_EQ(1, result.chains);
    EXPECT_EQ(120 * (3 + 3 + 3), result.score);
    EXPECT_EQ(FRAMES_VANISH_ANIMATION, result.frames);
    EXPECT_TRUE(result.quick);
}

TEST_F(BitFieldTest, simulate5)
{
    CoreField cf(
        ".YYYG."
        "BBBBY.");
    BitField bf(cf);

    RensaResult result = bf.simulate();
    EXPECT_EQ(2, result.chains);
    EXPECT_EQ(40 + 40 * 8, result.score);
    EXPECT_EQ(FRAMES_VANISH_ANIMATION * 2 +
              FRAMES_TO_DROP_FAST[1] * 2 +
              FRAMES_GROUNDING * 2, result.frames);
    EXPECT_FALSE(result.quick);
}

TEST_F(BitFieldTest, simulate6)
{
    CoreField cf(".RBRB."
                 "RBRBR."
                 "RBRBR."
                 "RBRBRR");
    BitField bf(cf);

    RensaResult cfResult = cf.simulate();
    RensaResult bfResult = bf.simulate();
    EXPECT_EQ(cfResult, bfResult);
}

TEST_F(BitFieldTest, simulate7)
{
    CoreField cf(
        ".YGGY."
        "BBBBBB"
        "GYBBYG"
        "BBBBBB");
    BitField bf(cf);

    RensaResult cfResult = cf.simulate();
    RensaResult bfResult = bf.simulate();
    EXPECT_EQ(cfResult, bfResult);
}

TEST_F(BitFieldTest, simulate8)
{
    CoreField cf(
        "BBBBBB"
        "GYBBYG"
        "BBBBYB");
    BitField bf(cf);

    RensaResult cfResult = cf.simulate();
    RensaResult bfResult = bf.simulate();
    EXPECT_EQ(cfResult, bfResult);
}

TEST_F(BitFieldTest, vanish1)
{
    PlainField pf(
        "RRBBBB"
        "RGRRBB");
    BitField bf(pf);

    FieldBits erased;
    int score = vanish(&bf, 2, &erased);

    EXPECT_EQ(60 * 11, score);
    EXPECT_EQ(FieldBits(pf, PuyoColor::BLUE), erased);
}

TEST_F(BitFieldTest, vanish2)
{
    PlainField pf(
        "RRBB.B"
        "RGRRBB");
    BitField bf(pf);

    FieldBits erased;
    int score = vanish(&bf, 2, &erased);

    EXPECT_EQ(0, score);
}

TEST_F(BitFieldTest, vanish3)
{
    PlainField pf(
        "ROOOOR"
        "OBBBBO"
        "ROOOOR");
    BitField bf(pf);

    FieldBits erased;
    int score = vanish(&bf, 1, &erased);

    EXPECT_EQ(40, score);
    EXPECT_TRUE(erased.get(1, 2));
    EXPECT_TRUE(erased.get(2, 2));
    EXPECT_TRUE(erased.get(3, 2));
    EXPECT_TRUE(erased.get(4, 2));
    EXPECT_TRUE(erased.get(5, 2));
    EXPECT_TRUE(erased.get(6, 2));
}

TEST_F(BitFieldTest, vanish4)
{
    PlainField pf(
        "RR.RRR" // 13
        "RRRRRR" // 12
        "OOOOOO"
        "OOOOOO"
        "OOOOOO"
        "OOOOOO" // 8
        "OOOOOO"
        "OOOOOO"
        "OOOOOO"
        "OOOOOO" // 4
        "OOOOOO"
        "OOOOOO"
        "OOOOOO");
    BitField bf(pf);

    FieldBits erased;
    int score = vanish(&bf, 1, &erased);
    EXPECT_EQ(60 * 3, score);

    EXPECT_TRUE(erased.get(1, 12));
    EXPECT_TRUE(erased.get(2, 12));
    EXPECT_TRUE(erased.get(3, 12));
    EXPECT_TRUE(erased.get(4, 12));
    EXPECT_TRUE(erased.get(5, 12));
    EXPECT_TRUE(erased.get(6, 12));

    EXPECT_FALSE(erased.get(1, 13));
    EXPECT_FALSE(erased.get(2, 13));
    EXPECT_FALSE(erased.get(3, 13));
    EXPECT_FALSE(erased.get(4, 13));
    EXPECT_FALSE(erased.get(5, 13));
    EXPECT_FALSE(erased.get(6, 13));
}

TEST_F(BitFieldTest, vanishdrop1)
{
    PlainField pf(
        "RGBBBB"
        "BBBGRB"
        "GRBBBB"
        "BBBGRB");
    BitField bf(pf);

    FieldBits erased;
    vanish(&bf, 2, &erased);
    drop(&bf, erased);

    FieldBits red = bf.bits(PuyoColor::RED);
    EXPECT_TRUE(red.get(1, 2));
    EXPECT_TRUE(red.get(2, 1));
    EXPECT_TRUE(red.get(5, 1));
    EXPECT_TRUE(red.get(5, 2));
    EXPECT_FALSE(red.get(1, 1));
    EXPECT_FALSE(red.get(1, 4));

    FieldBits green = bf.bits(PuyoColor::GREEN);
    EXPECT_TRUE(green.get(1, 1));
    EXPECT_TRUE(green.get(2, 2));
    EXPECT_TRUE(green.get(4, 1));
    EXPECT_TRUE(green.get(4, 2));
    EXPECT_FALSE(green.get(5, 1));
    EXPECT_FALSE(green.get(6, 1));
}
