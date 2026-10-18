#pragma once
#include "StationBuffer.h"
#include "StationPrep.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace fmrx
{
    // Метаданные слота (только message-поток). Частота, уровень и т.п. - параметры VST3, здесь их нет.
    struct StationMeta
    {
        juce::String name;           // RDS-имя
        juce::File   file;
        bool   loaded = false;
        bool   missing = false;      // файл не найден
        bool   loadFailed = false;   // файл есть, но не читается
        bool   loading = false;
        double durationSec = 0.0;
    };

    // Владение и потоки (как в FREQUENCY-VST):
    //  * message-поток: метаданные, publish, reclaim, сериализация;
    //  * worker (ThreadPool): декодирование и подготовка -> публикация через callAsync;
    //  * аудиопоток: только acquire() в начале блока (атомарно, без аллокаций и блокировок).
    // Снимки освобождаются на message-потоке, когда аудиопоток подтвердил более новый serial.
    class StationManager
    {
    public:
        StationManager();
        ~StationManager();

        // --- message thread ---
        void loadFileAsync(int slot, const juce::File& file);
        void clearSlot(int slot);
        void setName(int slot, const juce::String& name) { meta(slot).name = name; notify(); }
        void setSampleRate(double newRate);          // из любого потока; пересчёт - на message-потоке
        void reclaim();
        StationMeta&       meta(int slot)       { return metas_[(size_t) slot]; }
        const StationMeta& meta(int slot) const { return metas_[(size_t) slot]; }
        bool hasAudio(int slot) const;               // message thread: слот содержит загруженный файл

        std::function<void()> onChange;              // UI подписывается (message thread)

        juce::ValueTree toValueTree() const;
        void fromValueTree(const juce::ValueTree&);

        // --- audio thread ---
        const StationSnapshot* acquire() noexcept
        {
            auto* s = current_.load(std::memory_order_acquire);
            if (s != nullptr)
                inUseSerial_.store(s->serial, std::memory_order_release);
            return s;
        }

    private:
        void publishSlot(int slot, std::shared_ptr<const StationBuffer> buf);
        void reloadAll();
        void notify() { if (onChange) onChange(); }

        std::array<StationMeta, kNumStations> metas_;
        std::vector<std::shared_ptr<const StationSnapshot>> retained_;
        std::atomic<const StationSnapshot*> current_ { nullptr };
        std::atomic<std::uint64_t> inUseSerial_ { 0 };
        std::uint64_t nextSerial_ = 1;
        std::array<std::uint64_t, kNumStations> requestId_ {};   // отбрасываем устаревшие результаты загрузки
        std::atomic<double> engineRate_ { 44100.0 };
        juce::AudioFormatManager formats_;
        juce::ThreadPool pool_ { 1 };
        std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
    };
}
