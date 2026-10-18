#pragma once
// Без зависимостей от JUCE (схема из FREQUENCY-VST).
#include "Engine/FmBand.h"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace fmrx
{
    // Неизменяемые подготовленные аудиоданные станции (ресемплированы к частоте хоста, нормализованы, без DC).
    // Создаются вне аудиопотока (StationPrep), после публикации не модифицируются.
    struct StationBuffer
    {
        std::uint64_t id = 0;                         // уникален для каждого буфера
        std::array<std::vector<float>, 2> ch;         // ch[1] пуст для моно
        int numChannels = 1;
        int length = 0;
        double sampleRate = 44100.0;
        float normalizeGain = 1.0f;
    };

    // Снимок файлов всех станций, который видит аудиопоток: читается без блокировок и без refcount.
    struct StationSnapshot
    {
        std::uint64_t serial = 0;
        std::array<std::shared_ptr<const StationBuffer>, kNumStations> buffers;
    };
}
