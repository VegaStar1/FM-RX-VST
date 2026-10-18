#include "StationManager.h"
#include <cmath>

namespace fmrx
{
    StationManager::StationManager()
    {
        formats_.registerBasicFormats();   // WAV, AIFF, FLAC, Ogg Vorbis, MP3 (JUCE_USE_MP3AUDIOFORMAT=1)
        auto empty = std::make_shared<StationSnapshot>();
        empty->serial = nextSerial_++;
        current_.store(empty.get(), std::memory_order_release);
        retained_.push_back(std::move(empty));
    }

    StationManager::~StationManager()
    {
        alive_->store(false);
        pool_.removeAllJobs(true, 5000);
    }

    bool StationManager::hasAudio(int slot) const
    {
        return slot >= 0 && slot < kNumStations && retained_.back()->buffers[(size_t) slot] != nullptr;
    }

    std::shared_ptr<const StationBuffer> StationManager::buffer(int slot) const
    {
        if (slot < 0 || slot >= kNumStations) return nullptr;
        return retained_.back()->buffers[(size_t) slot];
    }

    StationRegion StationManager::regionFor(int slot, const StationBuffer* buf) const
    {
        if (buf == nullptr) return {};
        const auto& m = metas_[(size_t) slot];
        const double fs = buf->sampleRate;
        StationRegion r;
        r.start = (int) std::lround(m.trimStart * fs);
        r.end = m.trimEnd > 0.0 ? (int) std::lround(m.trimEnd * fs) : 0;
        r.fadeIn = (int) std::lround(m.fadeInMs * 0.001 * fs);
        r.fadeOut = (int) std::lround(m.fadeOutMs * 0.001 * fs);
        return r.clampedTo(buf->length);
    }

    void StationManager::setTrim(int slot, double startSec, double endSec)
    {
        if (slot < 0 || slot >= kNumStations) return;
        auto& m = meta(slot);
        const double dur = m.durationSec > 0 ? m.durationSec : 1e9;
        startSec = juce::jlimit(0.0, dur, startSec);
        endSec = endSec <= 0.0 ? 0.0 : juce::jlimit(0.0, dur, endSec);
        const double effEnd = endSec <= 0.0 ? dur : endSec;
        if (effEnd - startSec < 0.05) return;                     // не короче 50 мс
        m.trimStart = startSec;
        m.trimEnd = (endSec <= 0.0 || endSec >= dur - 1e-4) ? 0.0 : endSec;
        publishSlot(slot, retained_.back()->buffers[(size_t) slot]);
        notify();
    }

    void StationManager::setFades(int slot, float inMs, float outMs)
    {
        if (slot < 0 || slot >= kNumStations) return;
        auto& m = meta(slot);
        m.fadeInMs = juce::jlimit(0.f, 2000.f, inMs);
        m.fadeOutMs = juce::jlimit(0.f, 2000.f, outMs);
        publishSlot(slot, retained_.back()->buffers[(size_t) slot]);
        notify();
    }

    bool StationManager::autoTrim(int slot, float thresholdDb)
    {
        auto buf = buffer(slot);
        if (buf == nullptr) return false;
        const auto t = findSilenceTrim(*buf, thresholdDb);
        if (! t.found) return false;
        setTrim(slot, t.startSec, t.endSec);
        return true;
    }

    void StationManager::resetTrim(int slot)
    {
        if (slot < 0 || slot >= kNumStations) return;
        auto& m = meta(slot);
        m.trimStart = 0.0; m.trimEnd = 0.0; m.fadeInMs = 10.f; m.fadeOutMs = 20.f;
        publishSlot(slot, retained_.back()->buffers[(size_t) slot]);
        notify();
    }

    double StationManager::snapSec(int slot, double sec) const
    {
        auto buf = buffer(slot);
        if (buf == nullptr || buf->sampleRate <= 0) return sec;
        const int p = snapToZeroCrossing(*buf, (int) std::lround(sec * buf->sampleRate), (int) (0.002 * buf->sampleRate));
        return p / buf->sampleRate;
    }

    void StationManager::publishSlot(int slot, std::shared_ptr<const StationBuffer> buf)
    {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        auto next = std::make_shared<StationSnapshot>(*retained_.back());
        next->serial = nextSerial_++;
        next->regions[(size_t) slot] = regionFor(slot, buf.get());
        next->buffers[(size_t) slot] = std::move(buf);
        const auto* raw = next.get();
        retained_.push_back(std::move(next));
        current_.store(raw, std::memory_order_release);
    }

    void StationManager::reclaim()
    {
        const auto inUse = inUseSerial_.load(std::memory_order_acquire);
        while (retained_.size() > 1 && retained_.front()->serial < inUse
               && retained_.front().get() != current_.load(std::memory_order_acquire))
            retained_.erase(retained_.begin());
    }

    void StationManager::loadFileAsync(int slot, const juce::File& file)
    {
        if (slot < 0 || slot >= kNumStations) return;
        auto& m = meta(slot);
        // Другой файл - обрезка сбрасывается. Исключение - перепривязка пропавшего файла (его переместили):
        // обрезка и имя станции сохраняются.
        const bool relink = m.missing && m.file != file;
        if (m.file != file && ! relink)
        {
            m.trimStart = 0.0; m.trimEnd = 0.0; m.fadeInMs = 10.f; m.fadeOutMs = 20.f;
        }
        if ((m.file != file && ! relink) || m.name.isEmpty())
            m.name = file.getFileNameWithoutExtension().toUpperCase().substring(0, 16);
        m.file = file;
        m.loading = true; m.missing = false; m.loadFailed = false;
        const auto req = ++requestId_[(size_t) slot];
        notify();

        auto alive = alive_;
        pool_.addJob([this, alive, slot, file, req]
        {
            std::shared_ptr<StationBuffer> buf;
            std::shared_ptr<const Peaks> peaks;
            double durationSec = 0.0;
            std::unique_ptr<juce::AudioFormatReader> reader(formats_.createReaderFor(file));
            if (reader != nullptr && reader->lengthInSamples > 0 && reader->numChannels > 0)
            {
                durationSec = reader->sampleRate > 0 ? (double) reader->lengthInSamples / reader->sampleRate : 0.0;
                PrepareOptions opt;
                opt.targetRate = engineRate_.load();
                const auto maxLen = (juce::int64) (opt.maxSeconds * reader->sampleRate);
                const int len = (int) juce::jmin(reader->lengthInSamples, maxLen);
                const int chans = (int) juce::jmin(2u, reader->numChannels);
                juce::AudioBuffer<float> tmp(chans, len);
                if (reader->read(&tmp, 0, len, 0, true, chans > 1))
                {
                    const float* ptrs[2] = { tmp.getReadPointer(0), chans > 1 ? tmp.getReadPointer(1) : nullptr };
                    buf = prepareStation(ptrs, chans, len, reader->sampleRate, opt);
                    if (buf != nullptr) peaks = computePeaks(*buf);          // волна - тоже в фоне
                }
            }

            juce::MessageManager::callAsync([this, alive, slot, buf, peaks, durationSec, file, req]
            {
                if (! alive->load() || req != requestId_[(size_t) slot]) return;   // слот уже перезагружен
                auto& mm = meta(slot);
                mm.loading = false;
                mm.loaded = (buf != nullptr);
                mm.missing = (buf == nullptr) && ! file.existsAsFile();
                mm.loadFailed = (buf == nullptr) && file.existsAsFile();
                mm.durationSec = buf != nullptr ? buf->length / buf->sampleRate : durationSec;
                mm.peaks = peaks;
                publishSlot(slot, buf);
                notify();
            });
        });
    }

    void StationManager::clearSlot(int slot)
    {
        if (slot < 0 || slot >= kNumStations) return;
        ++requestId_[(size_t) slot];
        meta(slot) = StationMeta{};
        publishSlot(slot, nullptr);
        notify();
    }

    void StationManager::setSampleRate(double newRate)
    {
        if (! (newRate > 0.0)) return;
        const double old = engineRate_.exchange(newRate);
        if (std::fabs(old - newRate) < 0.5) return;
        auto alive = alive_;
        juce::MessageManager::callAsync([this, alive] { if (alive->load()) reloadAll(); });
    }

    void StationManager::reloadAll()
    {
        for (int i = 0; i < kNumStations; ++i)
        {
            auto& m = meta(i);
            if (m.file != juce::File{} && m.file.existsAsFile()) loadFileAsync(i, m.file);
        }
    }

    juce::ValueTree StationManager::toValueTree() const
    {
        juce::ValueTree v("STATIONS");
        for (int i = 0; i < kNumStations; ++i)
        {
            const auto& m = metas_[(size_t) i];
            if (m.file == juce::File{} && m.name.isEmpty()) continue;
            juce::ValueTree s("STATION");
            s.setProperty("slot", i, nullptr);
            s.setProperty("name", m.name, nullptr);
            s.setProperty("path", m.file.getFullPathName(), nullptr);
            s.setProperty("trimStart", m.trimStart, nullptr);
            s.setProperty("trimEnd", m.trimEnd, nullptr);
            s.setProperty("fadeIn", m.fadeInMs, nullptr);
            s.setProperty("fadeOut", m.fadeOutMs, nullptr);
            v.appendChild(s, nullptr);
        }
        return v;
    }

    void StationManager::fromValueTree(const juce::ValueTree& v)
    {
        for (int i = 0; i < kNumStations; ++i)
            if (metas_[(size_t) i].file != juce::File{} || hasAudio(i)) clearSlot(i);

        for (auto s : v)
        {
            const int slot = s.getProperty("slot", -1);
            if (slot < 0 || slot >= kNumStations) continue;
            auto& m = meta(slot);
            m.name = s.getProperty("name").toString();
            m.trimStart = juce::jmax(0.0, (double) s.getProperty("trimStart", 0.0));
            m.trimEnd = juce::jmax(0.0, (double) s.getProperty("trimEnd", 0.0));
            m.fadeInMs = (float) (double) s.getProperty("fadeIn", 10.0);
            m.fadeOutMs = (float) (double) s.getProperty("fadeOut", 20.0);
            const auto path = s.getProperty("path").toString();
            if (path.isEmpty()) continue;
            const juce::File f(path);
            m.file = f;                                  // имя из состояния не перезаписывается именем файла
            if (f.existsAsFile()) loadFileAsync(slot, f);
            else { m.file = f; m.missing = true; }
        }
        notify();
    }
}
