#ifndef WII_WII_CONNECTOR_SERVER_H_
#define WII_WII_CONNECTOR_SERVER_H_

#include <array>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <pthread.h>

#include "base/base.h"
#include "base/lock.h"
#include "capture/analyzer_result_drawer.h"
#include "core/decision.h"
#include "core/field/core_field.h"
#include "core/kumipuyo.h"
#include "core/puyo_color.h"
#include "core/real_color.h"
#include "core/server/connector/connector_manager.h"
#include "gui/drawer.h"
#include "gui/unique_sdl_surface.h"

class Analyzer;
class AnalyzerResult;
class KeySender;
class PlayerLog;
class Source;

class WiiConnectServer : public Drawer, public AnalyzerResultRetriever {
public:
    WiiConnectServer(Source*, Analyzer*, KeySender*, const std::string& p1, const std::string& p2);
    ~WiiConnectServer();

    virtual void draw(Screen*) OVERRIDE;

    bool start();
    void stop();

    virtual std::unique_ptr<AnalyzerResult> analyzerResult() const OVERRIDE;

private:
    static void* runLoopCallback(void*);

    void reset();
    void runLoop();

    bool playForUnknown(int frameId);
    bool playForLevelSelect(int frameId, const AnalyzerResult&);
    bool playForPlaying(int frameId, const AnalyzerResult&);
    bool playForFinished(int frameId);

    std::string makeMessageFor(int playerId, int frameId, const AnalyzerResult&);
    std::string makeStateString(int playerId, const AnalyzerResult&);
    void outputKeys(int playerId, const AnalyzerResult&, const PlayerLog&);

    PuyoColor toPuyoColor(RealColor, bool allowAllocation = false);

    pthread_t th_;
    volatile bool shouldStop_;
    std::unique_ptr<ConnectorManager> connector_;

    // These 3 field should be used for only drawing.
    mutable Mutex mu_;
    UniqueSDLSurface surface_;
    std::deque<std::unique_ptr<AnalyzerResult>> analyzerResults_;

    Source* source_;
    Analyzer* analyzer_;
    KeySender* keySender_;

    std::map<RealColor, PuyoColor> colorMap_;
    std::array<bool, 4> colorsUsed_;

    bool isAi_[2];
    Decision lastDecision_[2];
    double estimatedKeySec_[2];
    PuyoColor invisiblePuyoColors_[2][6];
    Kumipuyo currentPuyo_[2];

};

#endif