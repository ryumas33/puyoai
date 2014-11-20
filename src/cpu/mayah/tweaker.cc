#include "mayah_ai.h"

#include <iomanip>
#include <iostream>
#include <future>
#include <sstream>
#include <random>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "base/executor.h"
#include "core/algorithm/puyo_possibility.h"
#include "core/client/ai/endless/endless.h"
#include "core/sequence_generator.h"

#include "evaluation_parameter.h"

DECLARE_string(feature);
DECLARE_string(seq);
DECLARE_int32(seed);

DEFINE_bool(once, false, "true if running only once.");
DEFINE_int32(auto_count, 0, "run auto tweaker for this count.");
DEFINE_bool(show_field, false, "show field after each hand.");
DEFINE_int32(size, 100, "the number of case size.");

using namespace std;

struct Result {
    int score;
    string msg;
};

struct RunResult {
    int sumScore;
    int mainRensaCount;
    int aveMainRensaScore;
    int over40000Count;
    int over60000Count;
    int over70000Count;
    int over80000Count;
    int over100000Count;

    int resultScore() {
        return mainRensaCount * 20 + over60000Count * 6 + over70000Count + over80000Count;
    }
};

class ParameterTweaker {
public:
    ParameterTweaker() : mt_(random_device()())
    {
        for (const auto& ef : EvaluationFeature::all()) {
            if (ef.tweakable()) {
                tweakableFeatures_.push_back(ef);
            }
        }

        for (const auto& ef : EvaluationSparseFeature::all()) {
            if (ef.tweakable()) {
                tweakableSparseFeatures_.push_back(ef);
            }
        }
    }

    void tweakParameter(EvaluationParameter* parameter)
    {
        if (tweakableFeatures_.empty() && tweakableSparseFeatures_.empty())
            return;

        size_t N = tweakableFeatures_.size() + tweakableSparseFeatures_.size();

        // Currently consider only non sparse keys.
        std::uniform_int_distribution<> dist(0, N - 1);
        size_t i = dist(mt_);
        if (i < tweakableFeatures_.size()) {
            tweak(tweakableFeatures_[i], parameter);
        } else {
            tweak(tweakableSparseFeatures_[i - tweakableFeatures_.size()], parameter);
        }
    }

    void tweak(const EvaluationFeature& feature, EvaluationParameter* parameter)
    {
        double original = parameter->getValue(feature.key());
        normal_distribution<> dist(0, 10.0);
        double newValue = original + dist(mt_);

        cout << feature.str() << ": " << original << " -> " << newValue << endl;
        parameter->setValue(feature.key(), newValue);
    }

    void tweak(const EvaluationSparseFeature& feature, EvaluationParameter* parameter)
    {
        vector<double> values = parameter->getValues(feature.key());
        double d = normal_distribution<>(0, 10.0)(mt_);
        size_t k = uniform_int_distribution<>(0, values.size() - 1)(mt_);

        if ((feature.ascending() && d > 0) || (feature.descending() && d < 0)) {
            for (size_t i = 0; i < k; ++i) {
                values[i] += d;
            }
        } else {
            for (size_t i = k; i < values.size(); ++i) {
                values[i] += d;
            }
        }

        parameter->setValues(feature.key(), values);
    }

private:
    mt19937 mt_;
    vector<EvaluationFeature> tweakableFeatures_;
    vector<EvaluationSparseFeature> tweakableSparseFeatures_;
};

void removeNontokopuyoParameter(EvaluationParameter* parameter)
{
    for (const auto& ef : EvaluationFeature::all()) {
        if (ef.shouldIgnore())
            parameter->setValue(ef.key(), 0);
    }

    for (const auto& ef : EvaluationSparseFeature::all()) {
        if (ef.shouldIgnore()) {
            for (size_t i = 0; i < ef.size(); ++i) {
                parameter->setValue(ef.key(), i, 0);
            }
        }
    }
}

void runOnce(const EvaluationParameter& parameter)
{
    auto ai = new DebuggableMayahAI;
    ai->setEvaluationParameter(parameter);

    Endless endless(std::move(std::unique_ptr<AI>(ai)));
    endless.setVerbose(FLAGS_show_field);

    KumipuyoSeq seq = generateSequence();
    int score = endless.run(seq);

    cout << seq.toString() << endl;
    cout << "score = " << score << endl;
}

RunResult run(Executor* executor, const EvaluationParameter& parameter)
{
    const int N = FLAGS_size;
    vector<promise<Result>> ps(N);

    for (int i = 0; i < N; ++i) {
        auto f = [i, &parameter, &ps]() {
            auto ai = new DebuggableMayahAI;
            ai->setEvaluationParameter(parameter);
            Endless endless(std::move(std::unique_ptr<AI>(ai)));
            stringstream ss;
            KumipuyoSeq seq = generateRandomSequenceWithSeed(i);
            int score = endless.run(seq);
            ss << "case " << i << ": "
               << "score = " << score << endl;

            ps[i].set_value(Result{score, ss.str()});
        };
        executor->submit(f);
    }

    int sumScore = 0;
    int sumMainRensaScore = 0;
    int mainRensaCount = 0;
    int over40000Count = 0;
    int over60000Count = 0;
    int over70000Count = 0;
    int over80000Count = 0;
    int over100000Count = 0;
    for (int i = 0; i < N; ++i) {
        Result r = ps[i].get_future().get();
        sumScore += r.score;
        if (r.score >= 10000) {
            mainRensaCount++;
            sumMainRensaScore += r.score;
        }
        if (r.score >= 40000) { over40000Count++; }
        if (r.score >= 60000) { over60000Count++; }
        if (r.score >= 70000) { over70000Count++; }
        if (r.score >= 80000) { over80000Count++; }
        if (r.score >= 100000) { over100000Count++; }
        cout << r.msg;
    }

    int aveMainRensaScore = mainRensaCount > 0 ? sumMainRensaScore / mainRensaCount : 0;
    cout << "sum score  = " << sumScore << endl;
    cout << "ave score  = " << (sumScore / N) << endl;
    cout << "main rensa = " << mainRensaCount << endl;
    cout << "ave main rensa = " << aveMainRensaScore << endl;
    cout << "over  40000 = " << over40000Count << endl;
    cout << "over  60000 = " << over60000Count << endl;
    cout << "over  70000 = " << over70000Count << endl;
    cout << "over  80000 = " << over80000Count << endl;
    cout << "over 100000 = " << over100000Count << endl;

    return RunResult { sumScore, mainRensaCount, aveMainRensaScore,
            over40000Count, over60000Count, over70000Count, over80000Count, over100000Count };
}

void runAutoTweaker(Executor* executor, const EvaluationParameter& original, int num)
{
    cout << "Run with the original parameter." << endl;
    EvaluationParameter currentBestParameter(original);
    RunResult currentBestResult = run(executor, original);

    cout << "original score = " << currentBestResult.resultScore() << endl;

    ParameterTweaker tweaker;

    for (int i = 0; i < num; ++i) {
        EvaluationParameter parameter(currentBestParameter);
        tweaker.tweakParameter(&parameter);

        RunResult result = run(executor, parameter);
        cout << "score = " << result.resultScore() << endl;

        if (currentBestResult.resultScore() < result.resultScore()) {
            currentBestResult = result;
            currentBestParameter = parameter;

            cout << "Best parameter is updated." << endl;
            cout << currentBestParameter.toString() << endl;
            currentBestParameter.save("best-parameter.txt");
        }
    }
}

int main(int argc, char* argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();

    TsumoPossibility::initialize();

    unique_ptr<Executor> executor = Executor::makeDefaultExecutor();

    EvaluationParameter parameter(FLAGS_feature);
    removeNontokopuyoParameter(&parameter);

    if (!FLAGS_seq.empty() || FLAGS_seed >= 0) {
        runOnce(parameter);
    } else if (FLAGS_once) {
        run(executor.get(), parameter);
    } else if (FLAGS_auto_count > 0) {
        runAutoTweaker(executor.get(), parameter, FLAGS_auto_count);
    } else {
        map<double, RunResult> scoreMap;
        for (double x = -200; x <= -50; x += 50) {
            cout << "current x = " << x << endl;
            parameter.setValue(HAND_WIDTH_3, 3, x);
            scoreMap[x] = run(executor.get(), parameter);
        }
        for (const auto& m : scoreMap) {
            cout << setw(5) << m.first << " -> " << m.second.sumScore
                 << " / " << m.second.mainRensaCount
                 << " / " << m.second.aveMainRensaScore
                 << " / " << m.second.over40000Count
                 << " / " << m.second.over60000Count
                 << " / " << m.second.over70000Count
                 << " / " << m.second.over80000Count
                 << " / " << m.second.over100000Count
                 << endl;
        }
    }

    executor->stop();
    return 0;
}
