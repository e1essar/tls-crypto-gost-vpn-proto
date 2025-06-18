// include/engine/EngineLoader.h
#pragma once
#include "IEngineLoader.h"

namespace tls {
class EngineLoader : public IEngineLoader {
public:
    ENGINE* loadEngine(const std::string& engineId) override;
};
}

