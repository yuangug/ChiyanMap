#pragma once

#include <string>

namespace ExternalCompassSync {
    void Start();
    void Stop();
    void NotifyConfigChanged();
    void SetTargetPoint(const std::string& id, float x, float z, int dimensionId);
    void ClearTargetPoint();
    bool IsTargetPointing(const std::string& id);
}
