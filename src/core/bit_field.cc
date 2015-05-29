#include "core/bit_field.h"

#include <smmintrin.h>
#include <glog/logging.h>

#include <sstream>

#include "core/frame.h"
#include "core/plain_field.h"
#include "core/position.h"
#include "core/score.h"

using namespace std;

BitField::BitField()
{
    // Sets WALL
    m_[1].setAll(_mm_set_epi16(0xFFFF, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0xFFFF));
}

#ifndef EXPERIMENTAL_CORE_FIELD_USES_BIT_FIELD
BitField::BitField(const PlainField& pf) : BitField()
{
    __m128i m1 = _mm_load_si128(reinterpret_cast<const __m128i*>(pf.column(1)));
    __m128i m2 = _mm_load_si128(reinterpret_cast<const __m128i*>(pf.column(2)));
    __m128i m3 = _mm_load_si128(reinterpret_cast<const __m128i*>(pf.column(3)));
    __m128i m4 = _mm_load_si128(reinterpret_cast<const __m128i*>(pf.column(4)));
    __m128i m5 = _mm_load_si128(reinterpret_cast<const __m128i*>(pf.column(5)));
    __m128i m6 = _mm_load_si128(reinterpret_cast<const __m128i*>(pf.column(6)));

    for (int i = 0; i < NUM_PUYO_COLORS; ++i) {
        PuyoColor c = static_cast<PuyoColor>(i);
        if (c == PuyoColor::EMPTY || c == PuyoColor::WALL) {
            continue;
        }

        __m128i mask = _mm_set1_epi8(static_cast<char>(c));

        union {
            __m128i m;
            std::int16_t s[8];
        } xmm;

        xmm.s[0] = 0;
        xmm.s[1] = _mm_movemask_epi8(_mm_cmpeq_epi8(m1, mask));
        xmm.s[2] = _mm_movemask_epi8(_mm_cmpeq_epi8(m2, mask));
        xmm.s[3] = _mm_movemask_epi8(_mm_cmpeq_epi8(m3, mask));
        xmm.s[4] = _mm_movemask_epi8(_mm_cmpeq_epi8(m4, mask));
        xmm.s[5] = _mm_movemask_epi8(_mm_cmpeq_epi8(m5, mask));
        xmm.s[6] = _mm_movemask_epi8(_mm_cmpeq_epi8(m6, mask));
        xmm.s[7] = 0;

        if (ordinal(c) & 1)
            m_[0].setAll(FieldBits(xmm.m));
        if (ordinal(c) & 2)
            m_[1].setAll(FieldBits(xmm.m));
        if (ordinal(c) & 4)
            m_[2].setAll(FieldBits(xmm.m));
    }
}
#endif

BitField::BitField(const string& str) : BitField()
{
    int counter = 0;
    for (int i = str.length() - 1; i >= 0; --i) {
        int x = 6 - (counter % 6);
        int y = counter / 6 + 1;
        PuyoColor c = toPuyoColor(str[i]);
        setColor(x, y, c);
        counter++;
    }
}

PuyoColor BitField::color(int x, int y) const
{
    int b0 = m_[0].get(x, y) ? 1 : 0;
    int b1 = m_[1].get(x, y) ? 2 : 0;
    int b2 = m_[2].get(x, y) ? 4 : 0;

    return static_cast<PuyoColor>(b0 | b1 | b2);
}

void BitField::setColor(int x, int y, PuyoColor c)
{
    int cc = static_cast<int>(c);
    for (int i = 0; i < 3; ++i) {
        if (cc & (1 << i))
            m_[i].set(x, y);
        else
            m_[i].unset(x, y);
    }
}

bool BitField::isConnectedPuyo(int x, int y) const
{
    FieldBits colorBits = bits(color(x, y)).maskedField12();
    FieldBits single(x, y);
    return !single.expandEdge().mask(colorBits).notmask(single).isEmpty();
}

int BitField::countConnectedPuyos(int x, int y) const
{
    FieldBits colorBits = bits(color(x, y)).maskedField12();
    return FieldBits(x, y).expand(colorBits).popcount();
}

int BitField::countConnectedPuyos(int x, int y, FieldBits* checked) const
{
    FieldBits colorBits = bits(color(x, y)).maskedField12();
    FieldBits connected = FieldBits(x, y).expand(colorBits);
    checked->setAll(connected);
    return connected.popcount();
}

int BitField::countConnectedPuyosMax4(int x, int y) const
{
    FieldBits colorBits = bits(color(x, y)).maskedField12();
    return FieldBits(x, y).expand4(colorBits).popcount();
}

bool BitField::hasEmptyNeighbor(int x, int y) const
{
    if (x + 1 <= 6 && isEmpty(x + 1, y))
        return true;
    if (x - 1 >= 1 && isEmpty(x - 1, y))
        return true;
    if (y - 1 >= 1 && isEmpty(x, y - 1))
        return true;
    if (y + 1 <= 12 && isEmpty(x, y + 1))
        return true;
    return false;
}

Position* BitField::fillSameColorPosition(int x, int y, PuyoColor c,
                                          Position* positionQueueHead, FieldBits* checked) const
{
    FieldBits bs = FieldBits(x, y).expand(bits(c).maskedField12());
    checked->setAll(bs);
    int len = bs.toPositions(positionQueueHead);
    return positionQueueHead + len;
}

int BitField::dropAfterVanish(FieldBits erased)
{
    // TODO(mayah): If we can use AVX2, we have PDEP, PEXT instruction.
    // It would be really useful to improve this method, I believe.

    // bits   = b15 .. b9 b8 b7 .. b0

    // Consider y = 8.
    // v1 = and ( b15 .. b9 b8 b7 .. b0,
    //          (   0 ..  0  0   1 ..  1) = 0-0 b7-b0
    // v2 = and ( b15 .. b9 b8 b7 .. b0,
    //          (   1 ..  1  0  0 ..  0) = b15-b9 0-0
    // v3 = v2 >> 1 = 0 b15-b9 0-0
    // v4 = v1 | v3 = 0 b15-b9 b7-b0
    // new bits = BLEND(bits, v4, blender)
    const __m128i zero = _mm_setzero_si128();
    const __m128i ones = _mm_cmpeq_epi8(zero, zero);
    const __m128i whole = _mm_andnot_si128(erased.xmm(),
        _mm_or_si128(m_[0].xmm(), _mm_or_si128(m_[1].xmm(), m_[2].xmm())));

    int wholeErased = erased.horizontalOr16();
    int maxY = 31 - __builtin_clz(wholeErased);
    int minY = __builtin_ctz(wholeErased);

    DCHECK(1 <= minY && minY <= maxY && maxY <= 12)
        << "minY=" << minY << ' ' << "maxY=" << maxY << endl << erased.toString();

    //        MSB      y      LSB
    // line  : 0 ... 0 1 0 ... 0
    // right : 0 ... 0 0 1 ... 1
    // left  : 1 ... 1 0 0 ... 0

    __m128i line = _mm_set1_epi16(1 << (maxY + 1));
    __m128i rightOnes = _mm_set1_epi16((1 << (maxY + 1)) - 1);
    __m128i leftOnes = _mm_set1_epi16(~((1 << ((maxY + 1) + 1)) - 1));

    __m128i dropAmount = zero;
    // For each line, -1 if there exists dropping puyo, 0 otherwise.
    __m128i exists = _mm_cmpeq_epi16(_mm_and_si128(line, whole), line);

    for (int y = maxY; y >= minY; --y) {
        line = _mm_srli_epi16(line, 1);
        rightOnes = _mm_srai_epi16(rightOnes, 1);
        leftOnes = _mm_srai_epi16(leftOnes, 1);   // needs arithmetic shift.

        // for each line, -1 if drop, 0 otherwise.
        __m128i blender = _mm_xor_si128(_mm_cmpeq_epi16(_mm_and_si128(line, erased.xmm()), zero), ones);
        DCHECK(!FieldBits(blender).isEmpty());

        for (int i = 0; i < 3; ++i) {
            __m128i m = m_[i].xmm();
            __m128i v1 = _mm_and_si128(rightOnes, m);
            __m128i v2 = _mm_and_si128(leftOnes, m);
            __m128i v3 = _mm_srli_epi16(v2, 1);
            __m128i v4 = _mm_or_si128(v1, v3);
            // _mm_blend_epi16 takes const int for parameter, so let's use blendv_epi8.
            m = _mm_blendv_epi8(m, v4, blender);
            m_[i] = FieldBits(m);
        }

        // both blender and exists are -1, puyo will drop 1.
        // -(-1) = +1
        dropAmount = _mm_sub_epi16(dropAmount, _mm_and_si128(blender, exists));
        exists = _mm_or_si128(exists, _mm_cmpeq_epi16(_mm_and_si128(line, whole), line));
    }

    // We have _mm_minpos_epu16, but not _mm_maxpos_epu16. So, taking xor 1.
    int maxDropAmountNegative = _mm_cvtsi128_si32(_mm_minpos_epu16(_mm_xor_si128(ones, dropAmount)));
    return ~maxDropAmountNegative & 0xFF;
}

int BitField::vanish(int currentChain)
{
    FieldBits erased;
    RensaNonTracker tracker;
    int score = vanishForSimulation(currentChain, &erased, &tracker);
    for (auto& m : m_)
        m.unsetAll(erased);
    return score;
}

void BitField::drop()
{
    // TODO(mayah): slow!
    for (int x = 1; x <= 6; ++x) {
        int h = 1;
        for (int y = 1; y <= 13; ++y) {
            if (isEmpty(x, y))
                continue;
            setColor(x, h++, color(x, y));
        }
        for (; h <= 13; ++h) {
            setColor(x, h, PuyoColor::EMPTY);
        }
    }
}

std::string BitField::toString(char charIfEmpty) const
{
    ostringstream ss;
    for (int y = 14; y >= 1; --y) {
        for (int x = 1; x <= FieldConstant::WIDTH; ++x) {
            ss << toChar(color(x, y), charIfEmpty);
        }
    }

    return ss.str();
}

bool operator==(const BitField& lhs, const BitField& rhs)
{
    for (int i = 0; i < 3; ++i)
        if (lhs.m_[i] != rhs.m_[i])
            return false;
    return true;
}

std::ostream& operator<<(std::ostream& os, const BitField& bf)
{
    return os << bf.toString();
}
