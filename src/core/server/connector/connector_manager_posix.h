#ifndef CORE_SERVER_CONNECTOR_CONNECTOR_MANAGER_POSIX_H_
#define CORE_SERVER_CONNECTOR_CONNECTOR_MANAGER_POSIX_H_

#include <memory>
#include <vector>

#include "base/base.h"
#include "core/player.h"
#include "core/server/connector/connector.h"
#include "core/server/connector/connector_manager.h"

class ConnectorManagerPosix : public ConnectorManager {
public:
    ConnectorManagerPosix(std::unique_ptr<Connector> p1, std::unique_ptr<Connector> p2);

    virtual bool receive(int frameId, std::vector<ConnectorFrameResponse> cfr[NUM_PLAYERS]) override;

    virtual Connector* connector(int i) override { return connectors_[i].get(); }

    virtual void setWaitTimeout(bool flag) override { waitTimeout_ = flag; }

private:
    std::unique_ptr<Connector> connectors_[NUM_PLAYERS];
    bool waitTimeout_;
};

#endif
