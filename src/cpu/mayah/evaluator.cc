#include "evaluator.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>

#include <glog/logging.h>

#include "base/time.h"
#include "core/algorithm/plan.h"
#include "core/algorithm/puyo_possibility.h"
#include "core/algorithm/rensa_detector.h"
#include "core/core_field.h"
#include "core/decision.h"
#include "core/field_checker.h"
#include "core/position.h"
#include "core/rensa_result.h"
#include "core/score.h"

#include "evaluation_parameter.h"
#include "gazer.h"
#include "pattern_rensa_detector.h"
#include "rensa_hand_tree.h"

using namespace std;

namespace {


}

template<typename ScoreCollector, typename FeatureKey>
static void calculateConnection(ScoreCollector* sc, const CoreField& field,
                                FeatureKey key2, FeatureKey key3)
{
    int count2, count3;
    field.countConnection(&count2, &count3);
    sc->addScore(key2, count2);
    sc->addScore(key3, count3);
}

template<typename ScoreCollector, typename SparseFeatureKey>
static void calculateValleyDepth(ScoreCollector* sc,
                                 SparseFeatureKey key,
                                 SparseFeatureKey edgeKey,
                                 const CoreField& field)
{
    for (int x = 1; x <= 6; ++x) {
        int currentHeight = field.height(x);
        int leftHeight = (x == 1) ? 14 : field.height(x - 1);
        int rightHeight = (x == 6) ? 14 : field.height(x + 1);

        int left = std::max(leftHeight - currentHeight, 0);
        int right = std::max(rightHeight - currentHeight, 0);
        int depth = std::min(left, right);
        DCHECK(0 <= depth && depth <= 14) << depth;
        if (x == 1 || x == 6)
            sc->addScore(edgeKey, depth, 1);
        else
            sc->addScore(key, depth, 1);
    }
}

template<typename ScoreCollector, typename SparseFeatureKey>
static void calculateRidgeHeight(ScoreCollector* sc, SparseFeatureKey key, const CoreField& field)
{
    for (int x = 1; x <= 6; ++x) {
        int currentHeight = field.height(x);
        int leftHeight = (x == 1) ? 14 : field.height(x - 1);
        int rightHeight = (x == 6) ? 14 : field.height(x + 1);

        int left = std::max(currentHeight - leftHeight, 0);
        int right = std::max(currentHeight - rightHeight, 0);
        int height = std::min(left, right);
        DCHECK(0 <= height && height <= 14) << height;
        sc->addScore(key, height, 1);
    }
}

template<typename ScoreCollector, typename FeatureKey>
static void calculateFieldUShape(ScoreCollector* sc,
                                 FeatureKey linearKey,
                                 FeatureKey squareKey,
                                 const CoreField& field)
{
    static const int DIFF[FieldConstant::MAP_WIDTH] = {
        0, -3, 0, 1, 1, 0, -3, 0,
    };

    static const double FIELD_USHAPE_HEIGHT_COEF[15] = {
        0.0, 0.1, 0.1, 0.3, 0.3,
        0.5, 0.5, 0.7, 0.7, 1.0,
        1.0, 1.0, 1.0, 1.0, 1.0,
    };

    double average = 0;
    for (int x = 1; x <= 6; ++x)
        average += (field.height(x) + DIFF[x]);
    average /= 6;

    double linearValue = 0;
    double squareValue = 0;

    for (int x = 1; x <= FieldConstant::WIDTH; ++x) {
        int h = field.height(x) + DIFF[x];
        double coef = FIELD_USHAPE_HEIGHT_COEF[field.height(x)];
        linearValue += std::abs(h - average) * coef;
        squareValue += (h - average) * (h - average) * coef;
    }

    sc->addScore(linearKey, linearValue);
    sc->addScore(squareKey, squareValue);
}

// ----------------------------------------------------------------------

PreEvalResult PreEvaluator::preEval(const CoreField& currentField)
{
    PreEvalResult preEvalResult;

    auto matchablePatternIds = preEvalResult.mutableMatchablePatternIds();
    for (size_t i = 0; i < patternBook().size(); ++i) {
        const PatternBookField& pbf = patternBook().patternBookField(i);
        if (pbf.ignitionColumn() == 0)
            continue;
        if (pbf.isMatchable(currentField))
            matchablePatternIds->push_back(static_cast<int>(i));
    }

    return preEvalResult;
}

MidEvalResult MidEvaluator::eval(const RefPlan& plan, const CoreField& currentField, double score)
{
    UNUSED_VARIABLE(currentField);

    MidEvalResult result;
    if (plan.isRensaPlan())
        result.add(MIDEVAL_ERASE, 1);

    result.add(MIDEVAL_RESULT, score);
    return result;
}

template<typename ScoreCollector>
void Evaluator<ScoreCollector>::evalFrameFeature(int totalFrames, int numChigiri)
{
    sc_->addScore(TOTAL_FRAMES, totalFrames);
    sc_->addScore(NUM_CHIGIRI, numChigiri);
}

template<typename ScoreCollector>
void Evaluator<ScoreCollector>::evalConnection(const CoreField& field)
{
    calculateConnection(sc_, field, CONNECTION_2, CONNECTION_3);
}

template<typename ScoreCollector>
void Evaluator<ScoreCollector>::evalRestrictedConnectionHorizontalFeature(const CoreField& f)
{
    const int MAX_HEIGHT = 3; // instead of FieldConstant::HEIGHT
    for (int y = 1; y <= MAX_HEIGHT; ++y) {
        for (int x = 1; x < FieldConstant::WIDTH; ++x) {
            if (!isNormalColor(f.color(x, y)))
                continue;

            int len = 1;
            while (f.color(x, y) == f.color(x + len, y))
                ++len;

            EvaluationMoveFeatureKey key;
            if (len == 1) {
                continue;
            } else if (len == 2) {
                if (x <= 3 && 4 < x + len) {
                    key = CONNECTION_HORIZONTAL_CROSSED_2;
                } else {
                    key = CONNECTION_HORIZONTAL_2;
                }
            } else if (len == 3) {
                if (x <= 3 && 4 < x + len) {
                    key = CONNECTION_HORIZONTAL_CROSSED_3;
                } else {
                    key = CONNECTION_HORIZONTAL_3;
                }
            } else {
                CHECK(false) << "shouldn't happen: " << len;
            }

            sc_->addScore(key, 1);
            x += len - 1;
        }
    }
}

template<typename ScoreCollector>
void Evaluator<ScoreCollector>::evalThirdColumnHeightFeature(const CoreField& field)
{
    sc_->addScore(THIRD_COLUMN_HEIGHT, field.height(3), 1);
}

template<typename ScoreCollector>
void Evaluator<ScoreCollector>::evalValleyDepth(const CoreField& field)
{
    calculateValleyDepth(sc_, VALLEY_DEPTH, VALLEY_DEPTH_EDGE, field);
}

template<typename ScoreCollector>
void Evaluator<ScoreCollector>::evalRidgeHeight(const CoreField& field)
{
    calculateRidgeHeight(sc_, RIDGE_HEIGHT, field);
}

template<typename ScoreCollector>
void Evaluator<ScoreCollector>::evalFieldUShape(const CoreField& field)
{
    calculateFieldUShape(sc_, FIELD_USHAPE_LINEAR, FIELD_USHAPE_SQUARE, field);
}

template<typename ScoreCollector>
void Evaluator<ScoreCollector>::evalFieldRightBias(const CoreField& field)
{
    static const int DIFF[FieldConstant::MAP_WIDTH] = {
        0, 2, 2, 2, -8, -6, -6, 0
    };

    double average = 0;
    for (int x = 1; x <= 6; ++x)
        average += (field.height(x) + DIFF[x]);
    average /= 6;

    double linearValue = 0;
    double squareValue = 0;

    for (int x = 1; x <= FieldConstant::WIDTH; ++x) {
        int h = field.height(x) + DIFF[x];
        linearValue += std::abs(h - average);
        squareValue += (h - average) * (h - average);
    }

    sc_->addScore(FIELD_RIGHT_BIAS_LINEAR, linearValue);
    sc_->addScore(FIELD_RIGHT_BIAS_SQUARE, squareValue);
}

template<typename ScoreCollector>
void Evaluator<ScoreCollector>::evalUnreachableSpace(const CoreField& cf)
{
    sc_->addScore(NUM_UNREACHABLE_SPACE, cf.countUnreachableSpaces());
}

template<typename ScoreCollector>
void Evaluator<ScoreCollector>::evalFallenOjama(int fallenOjama)
{
    sc_->addScore(FALLEN_OJAMA, fallenOjama);
}

// Returns true If we don't need to evaluate other features.
template<typename ScoreCollector>
void Evaluator<ScoreCollector>::evalStrategy(const RefPlan& plan,
                                             int currentFrameId,
                                             int rensaTreeValue,
                                             const PlayerState& me,
                                             const PlayerState& enemy,
                                             const GazeResult& gazeResult,
                                             const MidEvalResult& midEvalResult)
{
    sc_->addScore(STRATEGY_RENSA_TREE, rensaTreeValue);
    if (rensaTreeValue < 0)
        sc_->addScore(STRATEGY_RENSA_TREE_NEGATIVE, rensaTreeValue);

    if (!plan.isRensaPlan())
        return;

#if 0
    if (plan.fallenOjama() > 0)
        return;
#endif

    int rensaEndingFrameId = currentFrameId + plan.totalFrames();
    int estimatedMaxScore = gazeResult.estimateMaxScore(rensaEndingFrameId, enemy);

    if (!enemy.isRensaOngoing() && plan.chains() <= 4) {
        int h = 12 - enemy.field.height(3);
        if (plan.score() >= estimatedMaxScore + scoreForOjama(6 * h + plan.totalOjama())) {
            sc_->addScore(STRATEGY_KILL, 1);
            sc_->addScore(STRATEGY_FRAMES, plan.totalFrames());
            return;
        }
    }

    // --- If the rensa is large enough, fire it.
    if (!enemy.isRensaOngoing()) {
        if (plan.score() >= estimatedMaxScore + scoreForOjama(60 + plan.totalOjama())) {
            sc_->addScore(STRATEGY_LARGE_ENOUGH, 1.0);
            sc_->addScore(STRATEGY_FRAMES, plan.totalFrames());
            return;
        }
    }

#if 0
    // TAIOU
    if (me.totalOjama(enemy) > 0 && rensaTreeValue >= 6) {
        if (plan.totalOjama() == 0) {
            sc_->addScore(STRATEGY_TAIOU, 1);
            sc_->addScore(STRATEGY_FRAMES, plan.totalFrames());
            return;
        } else if (plan.totalOjama() < 3) {
            sc_->addScore(STRATEGY_TAIOU, 0.9);
            sc_->addScore(STRATEGY_FRAMES, plan.totalFrames());
            return;
        }
    }

    if (me.totalOjama(enemy) >= 9 && rensaTreeValue < 0) {
        if (plan.totalOjama() == 0) {
            sc_->addScore(STRATEGY_TAIOU_RELUCTANT, 1);
            sc_->addScore(STRATEGY_FRAMES, plan.totalFrames());
            return;
        } else if (plan.totalOjama() < 3) {
            sc_->addScore(STRATEGY_TAIOU_RELUCTANT, 0.9);
            sc_->addScore(STRATEGY_FRAMES, plan.totalFrames());
            return;
        }
    }

#else
    if (me.totalOjama(enemy) >= 3) {
        if (plan.score() >= scoreForOjama(std::max(0, me.totalOjama(enemy) - 3))) {
            sc_->addScore(STRATEGY_TAIOU, 1.0);
            sc_->addScore(STRATEGY_FRAMES, plan.totalFrames());
            return;
        }
    }

    if (plan.totalOjama() >= 6) {
        if (plan.score() >= scoreForOjama(std::max(0, plan.totalOjama() - 3))) {
            sc_->addScore(STRATEGY_TAIOU, 0.9);
            sc_->addScore(STRATEGY_FRAMES, plan.totalFrames());
            return;
        }
    }
#endif

    if (plan.field().isZenkeshi()) {
        sc_->addScore(STRATEGY_SCORE, plan.score());
        sc_->addScore(STRATEGY_ZENKESHI, 1);
        // Don't consider frames.
        return;
    }

    // not plan.hasZenekshi, since it's already consumed.
    if (me.hasZenkeshi && !enemy.hasZenkeshi) {
        if (!enemy.isRensaOngoing()) {
            sc_->addScore(STRATEGY_SCORE, plan.score());
            sc_->addScore(STRATEGY_SOLO_ZENKESHI_CONSUME, 1);
            sc_->addScore(STRATEGY_FRAMES, plan.totalFrames());
            return;
        }
        if (plan.pendingOjama() + plan.fixedOjama() <= 36) {
            sc_->addScore(STRATEGY_SCORE, plan.score());
            sc_->addScore(STRATEGY_SOLO_ZENKESHI_CONSUME, 1);
            sc_->addScore(STRATEGY_FRAMES, plan.totalFrames());
            return;
        }
    }

#if 1
    if (plan.chains() == 2 && plan.totalOjama() <= 3 && midEvalResult.feature(MIDEVAL_ERASE) == 0 && rensaTreeValue > 0) {
        if (plan.score() >= scoreForOjama(24)) {
            sc_->addScore(STRATEGY_FIRE_SIDE_CHAIN_2_LARGE, 1);
        } else if (plan.score() >= scoreForOjama(18)) {
            sc_->addScore(STRATEGY_FIRE_SIDE_CHAIN_2_MEDIUM, 1);
        } else if (plan.score() >= scoreForOjama(15)) {
            sc_->addScore(STRATEGY_FIRE_SIDE_CHAIN_2_SMALL, 1);
        }
        return ;
    }

    if (plan.chains() == 3 && plan.totalOjama() == 0 && midEvalResult.feature(MIDEVAL_ERASE) == 0 && rensaTreeValue > 0) {
        if (plan.score() >= scoreForOjama(30)) {
            sc_->addScore(STRATEGY_FIRE_SIDE_CHAIN_3_LARGE, 1);
        } else if (plan.score() >= scoreForOjama(18)) {
            sc_->addScore(STRATEGY_FIRE_SIDE_CHAIN_3_MEDIUM, 1);
        } else if (plan.score() >= scoreForOjama(15)) {
            sc_->addScore(STRATEGY_FIRE_SIDE_CHAIN_3_SMALL, 1);
        }
        return ;
    }
#endif

    // If IBARA found, we always consider it.
    // TODO(mayah): Don't consider IBARA if we don't have enough puyos. Better not to fire IBARA in that case.
    if (plan.chains() == 1 && plan.score() >= scoreForOjama(10) && rensaTreeValue >= 10) {
        sc_->addScore(STRATEGY_IBARA, 1);
        sc_->addScore(STRATEGY_FRAMES, plan.totalFrames());
        return;
    }

    // TSUBUSHI / SAISOKU
    if (!enemy.isRensaOngoing()) {
        if (plan.chains() <= 3 && plan.score() >= scoreForOjama(15) && rensaTreeValue >= 5) {
            sc_->addScore(STRATEGY_TSUBUSHI, 1);
            sc_->addScore(STRATEGY_FRAMES, plan.totalFrames());
            return;
        }

        if (plan.chains() == 2 && plan.score() >= scoreForOjama(15) && rensaTreeValue >= -5) {
            sc_->addScore(STRATEGY_TSUBUSHI, 1);
            sc_->addScore(STRATEGY_FRAMES, plan.totalFrames());
            return;
        }
    }

    if (me.hasZenkeshi && enemy.hasZenkeshi) {
        sc_->addScore(STRATEGY_ZENKESHI_CONSUME, 1);
        sc_->addScore(STRATEGY_FRAMES, plan.totalFrames());
        return;
    }

    sc_->addScore(STRATEGY_SAKIUCHI, 1.0);

    // TODO(mayah): Check land leveling.
    // TODO(mayah): OIUCHI?
    // TODO(mayah): KILL when enemy has a lot of puyos?
}

template<typename ScoreCollector>
void RensaEvaluator<ScoreCollector>::evalRensaStrategy(const RefPlan& plan,
                                                       const RensaResult& rensaResult,
                                                       const ColumnPuyoList& cpl,
                                                       int currentFrameId,
                                                       const PlayerState& me,
                                                       const PlayerState& enemy)
{
    UNUSED_VARIABLE(currentFrameId);
    UNUSED_VARIABLE(me);

    if (plan.field().countPuyos() >= 36 && plan.score() >= scoreForOjama(15) &&
        plan.chains() <= 3 && rensaResult.chains >= 7 &&
        cpl.size() <= 3 && !enemy.isRensaOngoing()) {
        sc_->addScore(STRATEGY_SAISOKU, 1);
    }
}

template<typename ScoreCollector>
void RensaEvaluator<ScoreCollector>::evalRensaChainFeature(const RensaResult& rensaResult,
                                                           const PuyoSet& totalPuyoSet)
{
    sc_->addScore(MAX_CHAINS, rensaResult.chains, 1);

    int totalNecessaryPuyos = PuyoPossibility::necessaryPuyos(totalPuyoSet, 0.5);
    sc_->addScore(NECESSARY_PUYOS_LINEAR, totalNecessaryPuyos);
    sc_->addScore(NECESSARY_PUYOS_SQUARE, totalNecessaryPuyos * totalNecessaryPuyos);
}

template<typename ScoreCollector>
void RensaEvaluator<ScoreCollector>::evalFirePointTabooFeature(const CoreField& field,
                                                               const FieldBits& ignitionPuyoBits)
{
    // A_A is taboo generally. Allow this from x == 1 or x == 4.
    // TODO(mayah):
    // ..A...
    // A_A... should be taboo?

    for (int x = 2; x <= 3; ++x) {
        for (int y = 1; y <= 12; ++y) {
            if (!ignitionPuyoBits.get(x, y) || !ignitionPuyoBits.get(x + 1, y) || !ignitionPuyoBits.get(x + 2, y))
                continue;

            if (isNormalColor(field.color(x, y)) && field.color(x, y) == field.color(x + 2, y) && field.color(x + 1, y) == PuyoColor::EMPTY) {
                sc_->addScore(FIRE_POINT_TABOO, 1);
            }
        }
    }
}

template<typename ScoreCollector>
void RensaEvaluator<ScoreCollector>::evalRensaIgnitionHeightFeature(const CoreField& complementedField,
                                                                    const FieldBits& ignitionPuyoBits)
{
    FieldBits ignition = ignitionPuyoBits & complementedField.bitField().normalColorBits();
    int height = ignition.highestHeight();
    if (height >= 0)
        sc_->addScore(IGNITION_HEIGHT, height, 1);
}

template<typename ScoreCollector>
void RensaEvaluator<ScoreCollector>::evalRensaConnectionFeature(const CoreField& fieldAfterDrop)
{
    calculateConnection(sc_, fieldAfterDrop, CONNECTION_AFTER_DROP_2, CONNECTION_AFTER_DROP_3);
}

template<typename ScoreCollector>
void RensaEvaluator<ScoreCollector>::evalRensaScore(double score, double virtualScore)
{
    sc_->addScore(SCORE, score);
    sc_->addScore(VIRTUAL_SCORE, virtualScore);
}

template<typename ScoreCollector>
void RensaEvaluator<ScoreCollector>::evalRensaRidgeHeight(const CoreField& field)
{
    calculateRidgeHeight(sc_, RENSA_RIDGE_HEIGHT, field);
}

template<typename ScoreCollector>
void RensaEvaluator<ScoreCollector>::evalRensaValleyDepth(const CoreField& field)
{
    calculateValleyDepth(sc_, RENSA_VALLEY_DEPTH, RENSA_VALLEY_DEPTH_EDGE, field);
}

template<typename ScoreCollector>
void RensaEvaluator<ScoreCollector>::evalRensaFieldUShape(const CoreField& field)
{
    calculateFieldUShape(sc_,
                         RENSA_FIELD_USHAPE_LINEAR,
                         RENSA_FIELD_USHAPE_SQUARE,
                         field);
}

template<typename ScoreCollector>
void RensaEvaluator<ScoreCollector>::evalPatternScore(const ColumnPuyoList& cpl, double patternScore, int chains)
{
    if (chains > 0)
        sc_->addScore(PATTERN_BOOK_DIV_RENSA, patternScore / chains);
    sc_->addScore(PATTERN_BOOK, patternScore);

    int numPlaceHolders = 0;
    for (int x = 1; x <= 6; ++x) {
        int h = cpl.sizeOn(x);
        for (int i = 0; i < h; ++i) {
            if (!isNormalColor(cpl.get(x, i)))
                ++numPlaceHolders;
        }
    }

    sc_->addScore(NUM_PLACE_HOLDERS, numPlaceHolders);
}

template<typename ScoreCollector>
void RensaEvaluator<ScoreCollector>::evalComplementationBias(const ColumnPuyoList& cpl)
{
    for (int x = 1; x <= 6; ++x) {
        PuyoSet ps;
        int h = cpl.sizeOn(x);
        for (int i = 0; i < h; ++i)
            ps.add(cpl.get(x, i));

        if (ps.count() < 3)
            continue;
        if (ps.count() >= 4)
            sc_->addScore(COMPLEMENTATION_BIAS_MUCH, 1);
        if (ps.red() >= 3 || ps.blue() >= 3 || ps.yellow() >= 3 || ps.green() >= 3) {
            EvaluationRensaFeatureKey key = (x == 1 || x == 6) ? COMPLEMENTATION_BIAS_EDGE : COMPLEMENTATION_BIAS;
            sc_->addScore(key, 1);
        }
    }
}

template<typename ScoreCollector>
void RensaEvaluator<ScoreCollector>::evalRensaGarbage(const CoreField& fieldAfterDrop)
{
    sc_->addScore(NUM_GARBAGE_PUYOS, fieldAfterDrop.countPuyos());
    sc_->addScore(NUM_SIDE_GARBAGE_PUYOS, fieldAfterDrop.height(1) + fieldAfterDrop.height(6));
}

template<typename ScoreCollector>
void Evaluator<ScoreCollector>::evalCountPuyoFeature(const CoreField& field)
{
    sc_->addScore(NUM_COUNT_PUYOS, field.countColorPuyos(), 1);
    sc_->addScore(NUM_COUNT_OJAMA, field.countColor(PuyoColor::OJAMA));
}

template<typename ScoreCollector>
void Evaluator<ScoreCollector>::evalMidEval(const MidEvalResult& midEvalResult)
{
    // Copy midEvalResult.
    for (const auto& entry : midEvalResult.collectedFeatures()) {
        sc_->addScore(entry.first, entry.second);
    }
}

template<typename ScoreCollector>
CollectedCoef Evaluator<ScoreCollector>::calculateDefaultCoef(const PlayerState& me, const PlayerState& enemy) const
{
    const int INITIAL_THRESHOLD = 16;
    const int EARLY_THRESHOLD = 24;
    const int MIDDLE_THRESHOLD = 36;
    const int LATE_THRESHOLD = 54;

    const int count = me.field.countPuyos();

    CollectedCoef coef;

    if (enemy.hasZenkeshi) {
        if (count <= EARLY_THRESHOLD)
            coef.setCoef(EvaluationMode::ENEMY_HAS_ZENKESHI, 1.0);
        else
            coef.setCoef(EvaluationMode::ENEMY_HAS_ZENKESHI_MIDDLE, 1.0);
        return coef;
    }

    if (count <= INITIAL_THRESHOLD && me.hand <= 8) {
        coef.setCoef(EvaluationMode::INITIAL, 1.0);
        return coef;
    }

    if (count <= EARLY_THRESHOLD) {
        coef.setCoef(EvaluationMode::EARLY, 1.0);
        return coef;
    }

    if (count <= MIDDLE_THRESHOLD) {
        double ratio = static_cast<double>(count - EARLY_THRESHOLD) / (MIDDLE_THRESHOLD - EARLY_THRESHOLD);
        coef.setCoef(EvaluationMode::EARLY, 1 - ratio);
        coef.setCoef(EvaluationMode::MIDDLE, ratio);
        return coef;
    }

    if (count <= LATE_THRESHOLD) {
        double ratio = static_cast<double>(count - MIDDLE_THRESHOLD) / (LATE_THRESHOLD - MIDDLE_THRESHOLD);
        coef.setCoef(EvaluationMode::MIDDLE, 1 - ratio);
        coef.setCoef(EvaluationMode::LATE, ratio);
        return coef;
    }

    coef.setCoef(EvaluationMode::LATE, 1.0);
    return coef;
}

template<typename ScoreCollector>
void Evaluator<ScoreCollector>::eval(const RefPlan& plan,
                                     const KumipuyoSeq& restSeq,
                                     int currentFrameId,
                                     int maxIteration,
                                     const PlayerState& me,
                                     const PlayerState& enemy,
                                     const PreEvalResult& preEvalResult,
                                     const MidEvalResult& midEvalResult,
                                     bool fast,
                                     const GazeResult& gazeResult)
{
    typedef typename ScoreCollector::RensaScoreCollector RensaScoreCollector;
    typedef typename RensaScoreCollector::CollectedScore RensaCollectedScore;

    CollectedCoef coef = calculateDefaultCoef(me, enemy);
    sc_->setCoef(coef);

    const CoreField& fieldBeforeRensa = plan.field();

    // We'd like to evaluate frame feature always.
    evalFrameFeature(plan.totalFrames(), plan.numChigiri());
    evalMidEval(midEvalResult);

    evalCountPuyoFeature(fieldBeforeRensa);
    evalConnection(fieldBeforeRensa);
    evalRestrictedConnectionHorizontalFeature(fieldBeforeRensa);
    evalThirdColumnHeightFeature(fieldBeforeRensa);
    evalValleyDepth(fieldBeforeRensa);
    evalRidgeHeight(fieldBeforeRensa);
    evalFieldUShape(fieldBeforeRensa);
    evalFieldRightBias(fieldBeforeRensa);
    evalUnreachableSpace(fieldBeforeRensa);
    evalFallenOjama(plan.fallenOjama());

    const int numReachableSpace = fieldBeforeRensa.countConnectedPuyos(3, 12);
    int maxChain = 0;
    int rensaCounts[20] {};

    int sideChainMaxScore = 0;
    int fastChain6MaxScore = 0;
    int fastChain10MaxScore = 0;
    int maxVirtualRensaResultScore = 0;

    // TODO(mayah): MainRensaInfo should be array for each mode?
    struct MainRensaInfo {
        double score = -100000000;
        RensaCollectedScore collectedScore;
        int maxChains = 0;
    } mainRensa;

    // TODO(mayah): fill this.
    struct SideRensaInfo {
        double score = -100000000;
        RensaCollectedScore collectedScore;
    } sideRensa;

    RensaHandNodeMaker handTreeMaker(2, restSeq);
    auto evalCallback = [&](const CoreField& fieldAfterRensa,
                            const RensaResult& rensaResult,
                            const ColumnPuyoList& puyosToComplement,
                            PuyoColor /*firePuyoColor*/,
                            const string& patternName,
                            double patternScore) {
        CoreField complementedField(fieldBeforeRensa);
        if (!complementedField.dropPuyoList(puyosToComplement))
            return;

        FieldBits ignitionPuyoBits = complementedField.ignitionPuyoBits();

        ++rensaCounts[rensaResult.chains];

        const PuyoSet necessaryPuyoSet(puyosToComplement);
        const double possibility = PuyoPossibility::possibility(necessaryPuyoSet, std::max(0, numReachableSpace));
        const double virtualRensaScore = rensaResult.score * possibility;

        RensaScoreCollector rensaScoreCollector(sc_->mainRensaParamSet(), sc_->sideRensaParamSet());
        RensaEvaluator<RensaScoreCollector> rensaEvaluator(patternBook(), &rensaScoreCollector);

        rensaEvaluator.evalRensaRidgeHeight(complementedField);
        rensaEvaluator.evalRensaValleyDepth(complementedField);
        rensaEvaluator.evalRensaFieldUShape(complementedField);
        rensaEvaluator.evalRensaIgnitionHeightFeature(complementedField, ignitionPuyoBits);
        rensaEvaluator.evalRensaChainFeature(rensaResult, necessaryPuyoSet);
        rensaEvaluator.evalRensaGarbage(fieldAfterRensa);
        rensaEvaluator.evalPatternScore(puyosToComplement, patternScore, rensaResult.chains);
        rensaEvaluator.evalFirePointTabooFeature(fieldBeforeRensa, ignitionPuyoBits); // fieldBeforeRensa is correct.
        rensaEvaluator.evalRensaConnectionFeature(fieldAfterRensa);
        rensaEvaluator.evalComplementationBias(puyosToComplement);
        rensaEvaluator.evalRensaScore(rensaResult.score, virtualRensaScore);
        rensaEvaluator.evalRensaStrategy(plan, rensaResult, puyosToComplement, currentFrameId, me, enemy);

        // TODO(mayah): need to set a better mode here.
        if (mainRensa.score < rensaScoreCollector.mainRensaScore().score(coef)) {
            rensaScoreCollector.setBookname(patternName);
            rensaScoreCollector.setPuyosToComplement(puyosToComplement);
            mainRensa.score = rensaScoreCollector.mainRensaScore().score(coef);
            mainRensa.collectedScore = rensaScoreCollector.mainRensaScore();
        }

        if (sideRensa.score < rensaScoreCollector.sideRensaScore().score(coef)) {
            rensaScoreCollector.setPuyosToComplement(puyosToComplement);
            sideRensa.score = rensaScoreCollector.sideRensaScore().score(coef);
            sideRensa.collectedScore = rensaScoreCollector.sideRensaScore();
        }

        if (maxChain < rensaResult.chains) {
            maxChain = rensaResult.chains;
        }

        if (maxVirtualRensaResultScore < virtualRensaScore) {
            maxVirtualRensaResultScore = virtualRensaScore;
        }

        if (puyosToComplement.size() <= 1 && rensaResult.chains == 2) {
            sideChainMaxScore = std::max<int>(sideChainMaxScore, rensaResult.score);
        }
        if (puyosToComplement.size() <= 2 && rensaResult.chains == 2) {
            sideChainMaxScore = std::max<int>(sideChainMaxScore, rensaResult.score * 0.9);
        }
#if 0
        if (puyosToComplement.size() <= 3 && rensaResult.chains == 2) {
            sideChainMaxScore = std::max<int>(sideChainMaxScore, rensaResult.score * 0.8);
        }
        if (puyosToComplement.size() <= 4 && rensaResult.chains == 2) {
            sideChainMaxScore = std::max<int>(sideChainMaxScore, rensaResult.score * 0.75);
        }
#endif

        int nessesaryPuyos = PuyoPossibility::necessaryPuyos(necessaryPuyoSet, restSeq, 0.5);
        if (nessesaryPuyos <= 6 && fastChain6MaxScore < rensaResult.score) {
            fastChain6MaxScore = rensaResult.score;
        }
        if (nessesaryPuyos <= 10 && fastChain10MaxScore < rensaResult.score) {
            fastChain10MaxScore = rensaResult.score;
        }

        // Now, we can simulate complementedField.
        if (!fast) {
            handTreeMaker.add(std::move(complementedField), puyosToComplement, 0, PuyoSet());
        }
    };

    PatternRensaDetector detector(patternBook(), fieldBeforeRensa, evalCallback);
    detector.iteratePossibleRensas(preEvalResult.matchablePatternIds(), maxIteration);

    RensaDetector::detectSideChain(fieldBeforeRensa, RensaDetectorStrategy::defaultDropStrategy(),
                                   [&](CoreField&& cf, const ColumnPuyoList& cpl) {
        // TODO(mayah): fireColor is not PuyoColor::EMPTY.
        RensaResult rensaResult = cf.simulate();
        evalCallback(cf, rensaResult, cpl, PuyoColor::EMPTY, string(), 0.0);
    });

    int rensaHandValue = 0;
    if (!fast) {
        RensaHandTree myRensaTree(vector<RensaHandNode>{ handTreeMaker.makeNode() });
        // TODO(mayah): num ojama is correct? frame id is correct? not sure...
        int myOjama = plan.totalOjama();
        int myOjamaCommittingFrameId = plan.ojamaCommittingFrameId();
        int enemyOjama = 0;
        int enemyOjamaCommittingFrameId = 0;
        if (plan.isRensaPlan()) {
            int ojama = plan.rensaResult().score / 70;
            if (me.totalOjama(enemy) - ojama > 0) {
                myOjama = me.totalOjama(enemy) - ojama;
                myOjamaCommittingFrameId = plan.ojamaCommittingFrameId();
                enemyOjama = 0;
                enemyOjamaCommittingFrameId = 0;
            } else {
                int restOjama = ojama - me.totalOjama(enemy);
                myOjama = 0;
                myOjamaCommittingFrameId = 0;
                enemyOjama = enemy.totalOjama(me) + restOjama;
                if (enemyOjama > 0)
                    enemyOjamaCommittingFrameId = currentFrameId + plan.totalFrames();
            }
        } else {
            myOjama = plan.totalOjama();
            if (myOjama > 0)
                myOjamaCommittingFrameId = plan.ojamaCommittingFrameId();
            enemyOjama = enemy.totalOjama(me);
            if (enemyOjama > 0)
                enemyOjamaCommittingFrameId = currentFrameId;
        }

        double feasibleBeginTime = currentTime();
        int feasibleRensaHandValue =
            RensaHandTree::eval(myRensaTree, currentFrameId + plan.totalFrames(), 0, myOjama, myOjamaCommittingFrameId,
                                gazeResult.feasibleRensaHandTree(), gazeResult.frameIdToStartNextMove(), 0,
                                enemyOjama, enemyOjamaCommittingFrameId);
        double feasibleEndTime = currentTime();

        int possibleBeginTime = currentTime();
        int possibleRensaHandValue =
            RensaHandTree::eval(myRensaTree, currentFrameId + plan.totalFrames(), 0, myOjama, myOjamaCommittingFrameId,
                                gazeResult.possibleRensaHandTree(), gazeResult.frameIdToStartNextMove(), 0, enemyOjama, enemyOjamaCommittingFrameId);
        int possibleEndTime = currentTime();

        VLOG_IF(1, !sc_->isSimple())
            << "######################################################################\n"
            << fieldBeforeRensa.toDebugString()
            << restSeq.toString();

        VLOG(1) << "RensaHandTree::eval"
                << " feasible_score=" << feasibleRensaHandValue
                << " possible_score=" << possibleRensaHandValue
                << " myFrameId=" << currentFrameId + plan.totalFrames()
                << " myOjama=" << plan.totalOjama()
                << " myOjamaCommittingFrameId=" << myOjamaCommittingFrameId
                << " enemyFrameId=" << gazeResult.frameIdToStartNextMove()
                << " enemyOjama=" << enemyOjama
                << " enemyOjamaCommittingFrameId=" << enemyOjamaCommittingFrameId
                << " feasible_time=" << (1000 * (feasibleEndTime - feasibleBeginTime))
                << " possible_time=" << (1000 * (possibleEndTime - possibleBeginTime));

        VLOG_IF(1, !sc_->isSimple()) << "RensaHandTree trees:\n"
            << " feasible_score=" << feasibleRensaHandValue << "\n"
            << " possible_score=" << possibleRensaHandValue << "\n"
            << "MyTree:\n"
            << myRensaTree.toString()
            << "----------------------------------------------------------------------\n"
            << "Feasible EnemyTree:\n"
            << gazeResult.feasibleRensaHandTree().toString()
            << "----------------------------------------------------------------------\n"
            << "Possible EnemyTree:\n"
            << gazeResult.possibleRensaHandTree().toString()
            << "----------------------------------------------------------------------\n";

        rensaHandValue = std::min(feasibleRensaHandValue, possibleRensaHandValue);
    }

    evalStrategy(plan, currentFrameId, rensaHandValue, me, enemy, gazeResult, midEvalResult);

    // max chain
    sc_->addScore(RENSA_KIND, rensaCounts[maxChain]);

#if 0
    // side chain
    if (sideChainMaxScore >= scoreForOjama(21)) {
        sc_->addScore(HOLDING_SIDE_CHAIN_LARGE, 1);
    } else if (sideChainMaxScore >= scoreForOjama(15)) {
        sc_->addScore(HOLDING_SIDE_CHAIN_MEDIUM, 1);
    } else if (sideChainMaxScore >= scoreForOjama(12)) {
        sc_->addScore(HOLDING_SIDE_CHAIN_SMALL, 1);
    }
#endif

#if 1
    // fast chain
    if (fastChain6MaxScore >= scoreForOjama(18)) {
        sc_->addScore(KEEP_FAST_6_CHAIN, 1);
    }
    if (fastChain10MaxScore >= scoreForOjama(30)) {
        sc_->addScore(KEEP_FAST_10_CHAIN, 1);
    }
#endif

    // finalize.
    sc_->mergeMainRensaScore(mainRensa.collectedScore);
    sc_->mergeSideRensaScore(sideRensa.collectedScore);
    sc_->setEstimatedRensaScore(maxVirtualRensaResultScore);
}

template class Evaluator<FeatureScoreCollector>;
template class Evaluator<SimpleScoreCollector>;
template class RensaEvaluator<FeatureRensaScoreCollector>;
template class RensaEvaluator<SimpleRensaScoreCollector>;
