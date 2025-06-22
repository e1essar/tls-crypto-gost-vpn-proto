#pragma once
#include "IEngineLoader.h" // Подключает интерфейс

namespace tls {
class EngineLoader : public IEngineLoader { // Наследуется от IEngineLoader
public:
    ENGINE* loadEngine(const std::string& engineId) override; // Реализация загрузки движка
};
}