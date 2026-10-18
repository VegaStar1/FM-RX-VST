#pragma once
// Без зависимостей от JUCE (схема из FREQUENCY-VST / RTVD).
#include "Engine/FmBand.h"
#include <algorithm>
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

    // Отрезок файла, который звучит (неразрушающая обрезка), в сэмплах буфера.
    struct StationRegion
    {
        int start = 0;
        int end = 0;          // не включительно; 0 = до конца файла
        int fadeIn = 0;       // сэмплы
        int fadeOut = 0;

        // Привести к границам буфера: start < end <= length.
        StationRegion clampedTo(int length) const noexcept
        {
            StationRegion r = *this;
            if (length <= 0) return {};
            r.end = (r.end <= 0 || r.end > length) ? length : r.end;
            r.start = std::clamp(r.start, 0, std::max(0, r.end - 1));
            if (r.end - r.start < 2) { r.start = 0; r.end = length; }
            const int len = r.end - r.start;
            r.fadeIn = std::clamp(r.fadeIn, 0, len / 2);
            r.fadeOut = std::clamp(r.fadeOut, 0, len / 2);
            return r;
        }
    };

    // Снимок файлов всех станций, который видит аудиопоток: читается без блокировок и без refcount.
    struct StationSnapshot
    {
        std::uint64_t serial = 0;
        std::array<std::shared_ptr<const StationBuffer>, kNumStations> buffers;
        std::array<StationRegion, kNumStations> regions {};
    };
}
