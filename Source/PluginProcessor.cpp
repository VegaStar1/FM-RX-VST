#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace fmrx
{
    namespace
    {
        constexpr int kSchemaVersion = 2;
    }

    FmRxProcessor::FmRxProcessor()
        : AudioProcessor(BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          apvts_(*this, nullptr, "PARAMS", params::createLayout())
    {
        auto p = [this](const juce::String& id) { return apvts_.getRawParameterValue(id); };
        pTune_ = p(params::tune); pAfc_ = p(params::afc); pIfBw_ = p(params::ifBw); pDemod_ = p(params::demod);
        pDeemph_ = p(params::deemph); pStMode_ = p(params::stMode); pBlend_ = p(params::blend);
        pSoftMute_ = p(params::softMute); pHiCut_ = p(params::hiCut); pSpeaker_ = p(params::speaker);
        pOutput_ = p(params::output); pNoise_ = p(params::noise); pFading_ = p(params::fading);
        pFadeRate_ = p(params::fadeRate); pMultipath_ = p(params::multipath); pImpulse_ = p(params::impulse);
        pDrift_ = p(params::drift); pClean_ = p(params::cleanZone); pNoiseZone_ = p(params::noiseZone);
        pMute_ = p(params::mute); pClickVol_ = p(params::clickVol); pSeekSpeed_ = p(params::seekSpeed);
        pSeekSens_ = p(params::seekSens);
        for (int i = 0; i < kNumStations; ++i)
            sp_[(size_t) i] = { p(params::st(i, "on")), p(params::st(i, "freq")), p(params::st(i, "level")),
                                p(params::st(i, "stereo")), p(params::st(i, "src")), p(params::st(i, "mode")) };
        startTimerHz(20);
    }

    FmRxProcessor::~FmRxProcessor()
    {
        stopTimer();
    }

    bool FmRxProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
    {
        if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
        const auto in = layouts.getMainInputChannelSet();
        return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono() || in.isDisabled();
    }

    void FmRxProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
    {
        band_.prepare(sampleRate, samplesPerBlock);
        control_.prepare(sampleRate);
        click_.prepare(sampleRate);
        stations_.setSampleRate(sampleRate);
        scratchSize_ = juce::jmax(64, samplesPerBlock);
        for (auto& s : stScratch_) for (auto& c : s) c.assign((size_t) scratchSize_, 0.f);
        for (auto& c : inCopy_) c.assign((size_t) scratchSize_, 0.f);
        for (auto& c : out_) c.assign((size_t) scratchSize_, 0.f);
        for (auto& pl : players_) pl.reset();
        for (auto& p : playPos_) p.store(-1);
        muteKnown_ = false;
        setLatencySamples(band_.latencySamples());
    }

    void FmRxProcessor::readParams(BandParams& b, std::array<StationParams, kNumStations>& st,
                                   std::array<int, kNumStations>& src, std::array<int, kNumStations>& mode) const
    {
        b.tuneMHz = pTune_->load();
        b.afc = pAfc_->load() > 0.5f;
        b.drift = pDrift_->load();
        b.noiseDb = pNoise_->load();
        b.fading = pFading_->load();
        b.fadeRateHz = pFadeRate_->load();
        b.multipath = pMultipath_->load();
        b.impulse = pImpulse_->load();
        b.cleanZoneHz = pClean_->load() * 1000.f;
        b.noiseZoneHz = pNoiseZone_->load() * 1000.f;
        b.mute = pMute_->load() > 0.5f;
        b.rx.ifBandwidthHz = pIfBw_->load() * 1000.f;
        b.rx.demod = pDemod_->load() > 0.5f ? dsp::DemodType::Pll : dsp::DemodType::Quadrature;
        const int de = (int) pDeemph_->load();
        b.emphasisTau = de == 0 ? 50e-6 : (de == 1 ? 75e-6 : 0.0);
        b.rx.forceMono = pStMode_->load() > 0.5f;
        b.rx.blend = pBlend_->load();
        b.rx.softMute = pSoftMute_->load();
        b.rx.hiCut = pHiCut_->load();
        b.speaker = pSpeaker_->load();
        b.outputDb = pOutput_->load();
        for (int i = 0; i < kNumStations; ++i)
        {
            const auto& p = sp_[(size_t) i];
            st[(size_t) i].on = p.on->load() > 0.5f;
            st[(size_t) i].freqMHz = p.freq->load();
            st[(size_t) i].levelDb = p.level->load();
            st[(size_t) i].stereo = p.stereo->load() > 0.5f;
            src[(size_t) i] = (int) p.src->load();
            mode[(size_t) i] = (int) p.mode->load();
        }
    }

    void FmRxProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;
        const int total = buffer.getNumSamples();
        const int numIn = getTotalNumInputChannels();
        const int numOut = buffer.getNumChannels();
        if (scratchSize_ == 0 || numOut < 2) { buffer.clear(); return; }

        BandParams band;
        std::array<StationParams, kNumStations> st {};
        std::array<int, kNumStations> src {}, mode {};
        readParams(band, st, src, mode);
        const float clickLevel = pClickVol_->load();

        // MUTE: щелчок кнопки-фиксатора при каждом переключении (и из автоматизации тоже).
        if (! muteKnown_) { lastMute_ = band.mute; muteKnown_ = true; }
        else if (band.mute != lastMute_)
        {
            click_.trigger(band.mute ? dsp::ClickKind::LatchOn : dsp::ClickKind::LatchOff, clickLevel);
            lastMute_ = band.mute;
        }

        const StationSnapshot* snap = stations_.acquire();
        ControlParams cp;
        cp.tuneParam = band.tuneMHz;
        cp.noiseDb = band.noiseDb;
        cp.cleanZoneHz = band.cleanZoneHz;
        cp.noiseZoneHz = band.noiseZoneHz;
        cp.speed = pSeekSpeed_->load();
        cp.sensitivity = pSeekSens_->load();
        std::array<ControlStation, kNumStations> cs {};
        for (int i = 0; i < kNumStations; ++i)
        {
            const auto si = (size_t) i;
            const bool input = src[si] == (int) params::Source::Input;
            const bool hasAudio = input || (snap != nullptr && snap->buffers[si] != nullptr);
            cs[si] = { st[si].on && hasAudio, mode[si] == (int) params::Mode::OneShot && ! input, input, st[si].freqMHz, st[si].levelDb };
        }

        for (int pos = 0; pos < total; pos += scratchSize_)
        {
            const int n = juce::jmin(scratchSize_, total - pos);

            const auto cmd = control_.beginBlock(cp, cs, n);
            if (cmd != RadioCmd::None && cmd != RadioCmd::Restart)
                click_.trigger(dsp::ClickKind::Button, clickLevel);

            // Вход плагина копируем: буфер хоста одновременно служит выходом.
            for (int c = 0; c < 2; ++c)
            {
                const int srcCh = numIn > 0 ? juce::jmin(c, numIn - 1) : -1;
                if (srcCh >= 0) std::copy_n(buffer.getReadPointer(srcCh, pos), n, inCopy_[(size_t) c].data());
                else            std::fill_n(inCopy_[(size_t) c].data(), n, 0.f);
            }

            const float* chans[kNumStations][2];
            const float* const* perStation[kNumStations];
            std::array<StationParams, kNumStations> eff = st;
            for (int i = 0; i < kNumStations; ++i)
            {
                const auto si = (size_t) i;
                if (cs[si].input)
                {
                    chans[i][0] = inCopy_[0].data(); chans[i][1] = inCopy_[1].data();
                }
                else
                {
                    auto& pl = players_[si];
                    if (control_.consumeRestart(i)) pl.restart();
                    if (restartPending_[si].exchange(false, std::memory_order_acq_rel))
                    {
                        control_.requestPlay(i);                       // «▶ С НАЧАЛА» из редактора
                        pl.restart();
                    }
                    if (control_.slotState(i) == RadioControl::Slot::Parked) pl.park();
                    const StationBuffer* b = snap != nullptr ? snap->buffers[si].get() : nullptr;
                    const bool loop = ! cs[si].oneShot;
                    const StationRegion rg = snap != nullptr ? snap->regions[si] : StationRegion {};
                    if (pl.render(b, rg, stScratch_[si][0].data(), stScratch_[si][1].data(), n, loop))
                        control_.notifyEnded(i);
                    playPos_[si].store(b != nullptr && ! pl.parked() && ! pl.ended() ? pl.position() : -1, std::memory_order_relaxed);
                    chans[i][0] = stScratch_[si][0].data(); chans[i][1] = stScratch_[si][1].data();
                }
                eff[si].on = cs[si].usable && control_.carrierOn(i);   // пустой слот / доигравший one-shot - без несущей
                perStation[i] = chans[i];
            }

            band.tuneMHz = control_.tuneTarget();
            band_.process(perStation, out_[0].data(), out_[1].data(), n, band, eff);

            // Щелчки кнопок - поверх выхода (слышны и при включении MUTE).
            if (click_.active())
                for (int k = 0; k < n; ++k)
                {
                    const float c = click_.next();
                    out_[0][(size_t) k] += c;
                    out_[1][(size_t) k] += c;
                }
            buffer.copyFrom(0, pos, out_[0].data(), n);
            buffer.copyFrom(1, pos, out_[1].data(), n);
        }
        for (int c = 2; c < numOut; ++c) buffer.clear(c, 0, total);
    }

    // message-поток, 20 Гц: результат поиска/LOCK записывается в параметр частоты (ручка и автоматизация видят
    // то, что слышно), раз в ~0.5 с освобождаются старые снимки станций.
    void FmRxProcessor::timerCallback()
    {
        if (++timerTicks_ >= 10) { timerTicks_ = 0; stations_.reclaim(); }
        const int s = control_.state();
        using S = RadioControl::State;
        if (control_.owned() && (s == (int) S::Idle || s == (int) S::Dwell || s == (int) S::Locked))
        {
            const float f = control_.displayFreq();
            if (std::fabs(pTune_->load() - f) > 0.005f) setParam(params::tune, f);
        }
    }

    // ---------------- UI helpers (message thread) ----------------

    float FmRxProcessor::displayFreq() const
    {
        return control_.owned() ? control_.displayFreq() : pTune_->load();
    }

    float FmRxProcessor::stationFreq(int slot) const { return sp_[(size_t) slot].freq->load(); }

    double FmRxProcessor::playPositionSec(int slot) const
    {
        const int p = playPos_[(size_t) slot].load(std::memory_order_relaxed);
        auto buf = stations_.buffer(slot);
        if (p < 0 || buf == nullptr || buf->sampleRate <= 0) return -1.0;
        return p / buf->sampleRate;
    }

    void FmRxProcessor::playFromStart(int slot)
    {
        tuneTo(stationFreq(slot));
        restartPending_[(size_t) slot].store(true, std::memory_order_release);
    }

    bool FmRxProcessor::stationAudible(int slot) const
    {
        const auto& p = sp_[(size_t) slot];
        if (p.on->load() < 0.5f) return false;
        return (int) p.src->load() == (int) params::Source::Input || stations_.hasAudio(slot);
    }

    int FmRxProcessor::tunedStation() const
    {
        const double tuned = displayFreq();
        const double clean = pClean_->load() * 1000.0, noise = pNoiseZone_->load() * 1000.0;
        int best = -1;
        float bestG = -20.f;                        // станция «под стрелкой»: не глубже -20 дБ в зоне
        for (int i = 0; i < kNumStations; ++i)
        {
            if (! stationAudible(i) || (control_.doneMask() & (1u << i))) continue;
            const float g = FmBand::zoneGainDb((tuned - stationFreq(i)) * 1e6, clean, noise);
            if (g > bestG) { bestG = g; best = i; }
        }
        return best;
    }

    void FmRxProcessor::setParam(const char* id, float plain)
    {
        if (auto* prm = apvts_.getParameter(id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost(prm->convertTo0to1(plain));
            prm->endChangeGesture();
        }
    }

    void FmRxProcessor::tuneTo(float mhz) { setParam(params::tune, juce::jlimit(87.5f, 108.f, mhz)); }

    // ---------------- state ----------------

    void FmRxProcessor::getStateInformation(juce::MemoryBlock& dest)
    {
        juce::ValueTree root("FMRX_STATE");
        root.setProperty("schemaVersion", kSchemaVersion, nullptr);
        root.appendChild(apvts_.copyState(), nullptr);            // параметры + имя пресета + размер окна
        root.appendChild(stations_.toValueTree(), nullptr);
        if (auto xml = root.createXml())
            copyXmlToBinary(*xml, dest);
    }

    void FmRxProcessor::setStateInformation(const void* data, int size)
    {
        auto xml = getXmlFromBinary(data, size);
        if (xml == nullptr) return;
        const auto root = juce::ValueTree::fromXml(*xml);
        if (! root.hasType("FMRX_STATE")) return;
        // Схема 1 -> 2: новые параметры (зоны, режимы станций, mute...) отсутствуют и получают значения по умолчанию.
        const auto p = root.getChildWithName(apvts_.state.getType());
        if (p.isValid()) apvts_.replaceState(p);
        const auto s = root.getChildWithName("STATIONS");
        auto apply = [this, s] { stations_.fromValueTree(s.isValid() ? s : juce::ValueTree("STATIONS")); };
        if (juce::MessageManager::getInstance()->isThisTheMessageThread()) apply();
        else juce::MessageManager::callAsync(apply);
    }

    juce::AudioProcessorEditor* FmRxProcessor::createEditor() { return new FmRxEditor(*this); }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new fmrx::FmRxProcessor(); }
