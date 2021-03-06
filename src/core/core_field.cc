#include "core/core_field.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "core/column_puyo.h"
#include "core/column_puyo_list.h"
#include "core/decision.h"
#include "core/field_checker.h"
#include "core/kumipuyo.h"
#include "core/position.h"
#include "core/rensa_result.h"

using namespace std;

CoreField::CoreField(const std::string& url) :
    field_(url)
{
    heights_[0] = 0;
    for (int x = 1; x <= WIDTH; ++x) {
        heights_[x] = 13;
        for (int y = 1; y <= 13; ++y) {
            if (color(x, y) == PuyoColor::EMPTY) {
                heights_[x] = y - 1;
                break;
            }
        }
        for (int y = heights_[x] + 1; y <= 13; ++y)
            DCHECK(isEmpty(x, y));
    }
    heights_[MAP_WIDTH - 1] = 0;
}

CoreField::CoreField(const PlainField& f) :
    field_(f)
{
    heights_[0] = 0;
    for (int x = 1; x <= WIDTH; ++x) {
        heights_[x] = 13;
        for (int y = 1; y <= 13; ++y) {
            if (color(x, y) == PuyoColor::EMPTY) {
                heights_[x] = y - 1;
                break;
            }
        }
        for (int y = heights_[x] + 1; y <= 13; ++y)
            DCHECK(isEmpty(x, y));
    }
    heights_[MAP_WIDTH - 1] = 0;
}

PlainField CoreField::toPlainField() const
{
    PlainField pf;

    for (int x = 1; x <= 6; ++x) {
        for (int y = 1; y <= 14; ++y) {
            pf.setColor(x, y, color(x, y));
        }
    }

    return pf;
}

bool CoreField::isZenkeshi() const
{
    for (int x = 1; x <= WIDTH; ++x) {
        if (height(x) > 0)
            return false;
    }

    return true;
}

int CoreField::countPuyos() const
{
    int count = 0;
    for (int x = 1; x <= WIDTH; ++x)
        count += height(x);

    return count;
}

int CoreField::countColorPuyos() const
{
    return bitField().normalColorBits().maskedField13().popcount();
}

int CoreField::countColor(PuyoColor c) const
{
    return bitField().bits(c).maskedField13().popcount();
}

int CoreField::countUnreachableSpaces() const
{
    int count = 0;

    if (height(2) >= 12 && height(1) < 12)
        count += 12 - height(1);
    if (height(4) >= 12 && height(5) < 12)
        count += 12 - height(5);
    if ((height(4) >= 12 || height(5) >= 12) && height(6) < 12)
        count += 12 - height(6);

    return count;
}

int CoreField::countReachableSpaces() const
{
    if (height(3) >= 12)
        return 0;

    int count = 12 - height(3);
    if (height(2) < 12) {
        count += 12 - height(2);
        if (height(1) < 12) {
            count += 12 - height(1);
        }
    }

    if (height(4) < 12) {
        count += 12 - height(4);
        if (height(5) < 12) {
            count += 12 - height(5);
            if (height(6) < 12) {
                count += 12 - height(6);
            }
        }
    }

    return count;
}

bool CoreField::dropKumipuyo(const Decision& decision, const Kumipuyo& kumiPuyo)
{
    int x1 = decision.axisX();
    int x2 = decision.childX();
    PuyoColor c1 = kumiPuyo.axis;
    PuyoColor c2 = kumiPuyo.child;

    if (decision.r == 2) {
        if (!dropPuyoOnWithMaxHeight(x2, c2, 14))
            return false;
        if (!dropPuyoOnWithMaxHeight(x1, c1, 13)) {
            removePuyoFrom(x2);
            return false;
        }
        return true;
    }

    if (!dropPuyoOnWithMaxHeight(x1, c1, 13))
        return false;
    if (!dropPuyoOnWithMaxHeight(x2, c2, 14)) {
        removePuyoFrom(x1);
        return false;
    }
    return true;
}

int CoreField::framesToDropNext(const Decision& decision) const
{
    // TODO(mayah): This calculation should be more accurate. We need to compare this with
    // actual AC puyo2 and duel server algorithm. These must be much the same.

    // TODO(mayah): When "kabegoe" happens, we need more frames.
    const int KABEGOE_PENALTY = 6;

    // TODO(mayah): It looks drop animation is too short.

    int x1 = decision.axisX();
    int x2 = decision.childX();

    int dropFrames = FRAMES_TO_MOVE_HORIZONTALLY[abs(3 - x1)];

    if (decision.r == 0) {
        int dropHeight = HEIGHT - height(x1);
        if (dropHeight <= 0) {
            // TODO(mayah): We need to add penalty here. How much penalty is necessary?
            dropFrames += KABEGOE_PENALTY + FRAMES_GROUNDING;
        } else {
            dropFrames += FRAMES_TO_DROP_FAST[dropHeight] + FRAMES_GROUNDING;
        }
    } else if (decision.r == 2) {
        int dropHeight = HEIGHT - height(x1) - 1;
        // TODO: If puyo lines are high enough, rotation might take time. We should measure this later.
        // It looks we need 3 frames to waiting that each rotation has completed.
        if (dropHeight < 6)
            dropHeight = 6;

        dropFrames += FRAMES_TO_DROP_FAST[dropHeight] + FRAMES_GROUNDING;
    } else {
        if (height(x1) == height(x2)) {
            int dropHeight = HEIGHT - height(x1);
            if (dropHeight <= 0) {
                dropFrames += KABEGOE_PENALTY + FRAMES_GROUNDING;
            } else if (dropHeight < 3) {
                dropFrames += FRAMES_TO_DROP_FAST[3] + FRAMES_GROUNDING;
            } else {
                dropFrames += FRAMES_TO_DROP_FAST[dropHeight] + FRAMES_GROUNDING;
            }
        } else {
            int minHeight = min(height(x1), height(x2));
            int maxHeight = max(height(x1), height(x2));
            int diffHeight = maxHeight - minHeight;
            int dropHeight = HEIGHT - maxHeight;
            if (dropHeight <= 0) {
                dropFrames += KABEGOE_PENALTY;
            } else if (dropHeight < 3) {
                dropFrames += FRAMES_TO_DROP_FAST[3];
            } else {
                dropFrames += FRAMES_TO_DROP_FAST[dropHeight];
            }
            dropFrames += FRAMES_GROUNDING;
            dropFrames += FRAMES_TO_DROP[diffHeight];
            dropFrames += FRAMES_GROUNDING;
        }
    }

    CHECK(dropFrames >= 0);
    return dropFrames;
}

bool CoreField::isChigiriDecision(const Decision& decision) const
{
    DCHECK(decision.isValid()) << "decision " << decision.toString() << " should be valid.";

    if (decision.axisX() == decision.childX())
        return false;

    return height(decision.axisX()) != height(decision.childX());
}

int CoreField::fallOjama(int lines)
{
    if (lines == 0)
        return 0;

    int dropHeight = 0;
    for (int x = 1; x <= WIDTH; ++x) {
        dropHeight = std::max(dropHeight, 12 - height(x));
        for (int i = 0; i < lines; ++i) {
            (void)dropPuyoOnWithMaxHeight(x, PuyoColor::OJAMA, 13);
        }
    }

    // TODO(mayah): When more ojamas are dropped, grounding frames is necessary more.
    // See FieldRealtime::onStateOjamaDropping() also.
    return FRAMES_TO_DROP[dropHeight] + framesGroundingOjama(6 * lines);
}

bool CoreField::dropPuyoOnWithMaxHeight(int x, PuyoColor c, int maxHeight)
{
    DCHECK_NE(c, PuyoColor::EMPTY) << toDebugString();
    DCHECK_LE(maxHeight, 14);

    if (height(x) >= std::min(13, maxHeight))
        return false;

    DCHECK_EQ(color(x, height(x) + 1), PuyoColor::EMPTY)
        << "maxHeight=" << maxHeight << '\n'
        << toDebugString();

    unsafeSet(x, ++heights_[x], c);
    return true;
}

bool CoreField::dropPuyoListWithMaxHeight(const ColumnPuyoList& cpl, int maxHeight)
{
    for (int x = 1; x <= 6; ++x) {
        int s = cpl.sizeOn(x);
        for (int i = 0; i < s; ++i) {
            PuyoColor pc = cpl.get(x, i);
            if (!dropPuyoOnWithMaxHeight(x, pc, maxHeight))
                return false;
        }
    }

    return true;
}

FieldBits CoreField::ignitionPuyoBits() const
{
    return field_.ignitionPuyoBits();
}

int CoreField::fillErasingPuyoPositions(Position* eraseQueue) const
{
    return field_.fillErasingPuyoPositions(eraseQueue);
}

vector<Position> CoreField::erasingPuyoPositions() const
{
    // All the positions of erased puyos will be stored here.
    Position eraseQueue[MAP_WIDTH * MAP_HEIGHT];
    int n = fillErasingPuyoPositions(eraseQueue);
    return vector<Position>(eraseQueue, eraseQueue + n);
}

bool CoreField::rensaWillOccurWhenLastDecisionIs(const Decision& decision) const
{
    Position p1 = Position(decision.x, height(decision.x));
    if (countConnectedPuyos(p1.x, p1.y) >= 4)
        return true;

    Position p2;
    switch (decision.r) {
    case 0:
    case 2:
        p2 = Position(decision.x, height(decision.x) - 1);
        break;
    case 1:
        p2 = Position(decision.x + 1, height(decision.x + 1));
        break;
    case 3:
        p2 = Position(decision.x - 1, height(decision.x - 1));
        break;
    default:
        DCHECK(false) << decision.toString();
        return false;
    }

    if (countConnectedPuyosMax4(p2.x, p2.y) >= 4)
        return true;

    return false;
}

std::string CoreField::toDebugString() const
{
    std::ostringstream s;
    for (int y = MAP_HEIGHT - 1; y >= 0; y--) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            s << toChar(color(x, y)) << ' ';
        }
        s << std::endl;
    }
    s << ' ';
    for (int x = 1; x <= WIDTH; ++x)
        s << setw(2) << height(x);
    s << std::endl;
    return s.str();
}

// friend static
bool operator==(const CoreField& lhs, const CoreField& rhs)
{
    for (int x = 1; x <= FieldConstant::WIDTH; ++x) {
        if (lhs.height(x) != rhs.height(x))
            return false;

        for (int y = 1; y <= lhs.height(x); ++y) {
            if (lhs.color(x, y) != rhs.color(x, y))
                return false;
        }
    }

    return true;
}

// friend static
bool operator!=(const CoreField& lhs, const CoreField& rhs)
{
    return !(lhs == rhs);
}
