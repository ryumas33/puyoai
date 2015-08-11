#ifndef CORE_RENSA_TRACKER_H_
#define CORE_RENSA_TRACKER_H_

#include "base/unit.h"
#include "core/field_bits.h"

// RensaTracker tracks how rensa is vanished.
// For example, a puyo is vanished in what-th chain, coefficient of each chain, etc.
// You can pass a RensaTracker to CoreField::simulate() to track the rensa.
// You can also define your own RensaTracker, and pass it to CoreField::simulate().
//
// RensaTracker must define several interface. CoreField::simulate() has several hook points
// that calls the corresponding Tracker methods. If you'd like to add a new hook point,
// you need to define a hook point in CoreField.
//
// Here, we define only RensaNonTracker. The other implementations are located on core/rensa_tracker/.

class RensaTrackerBase {
public:
    void track(int /*nthChain*/, int /*numErasedPuyo*/, int /*longBonusCoef*/, int /*colorBonusCoef*/,
               const FieldBits& /*vanishedColorPuyoBits*/, const FieldBits& /*vanishedOjamaPuyoBits*/) {}
    void track(int /*nthChain*/, const FieldBits& /*vanishedColorPuyoBits*/, const FieldBits& /*vanishedOjamaPuyoBits*/) {}

    void trackDrop(FieldBits /*blender*/, FieldBits /*leftOnes*/, FieldBits /*rightOnes*/) {}
};

// ----------------------------------------------------------------------

template<typename TrackResult>
class RensaTracker;

// ----------------------------------------------------------------------

// RensaTracker<Unit> is a tracker that does not track anything.
template<>
class RensaTracker<Unit> : public RensaTrackerBase {
public:
};
typedef RensaTracker<Unit> RensaNonTracker;

#endif // CORE_RENSA_TRACKER_H_
