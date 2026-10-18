#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace fmrx
{
    namespace
    {
        constexpr int kSchemaVersion = 1;
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
        pDrift_ = p(params::drift);
        for (int i = 0; i < kNumStations; ++i)
            sp_[(size_t) i] = { p(params::st(i, "on")), p(params::st(i, "freq")), p(params::st(i, "level")),
                                p(params::st(i, "stereo")), p(params::st(i, "src")) };
        startTimerHz(2);
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
        stations_.setSampleRate(sampleRate);
        scratchSize_ = juce::jmax(64, samplesPerBlock);
        for (auto& s : stScratch_) for (auto& c : s) c.assign((size_t) scratchSize_, 0.f);
        for (auto& c : inCopy_) c.assign((size_t) scratchSize_, 0.f);
        for (auto& c : out_) c.assign((size_t) scratchSize_, 0.f);
        for (auto& pl : players_) pl.reset();
        setLatencySamples(band_.latencySamples());
    }

    void FmRxProcessor::readParams(BandParams& b, std::array<StationParams, kNumStations>& st,
                                   std::array<int, kNumStations>& src) const
    {
        b.tuneMHz = pTune_->load();
        b.afc = pAfc_->load() > 0.5f;
        b.drift = pDrift_->load();
        b.noiseDb = pNoise_->load();
        b.fading = pFading_->load();
        b.fadeRateHz = pFadeRate_->load();
        b.multipath = pMultipath_->load();
        b.impulse = pImpulse_->load();
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
        std::array<int, kNumStations> src {};
        readParams(band, st, src);

        const StationSnapshot* snap = stations_.acquire();

        for (int pos = 0; pos < total; pos += scratchSize_)
        {
            const int n = juce::jmin(scratchSize_, total - pos);

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
                if (src[si] == (int) params::Source::Input)
                {
                    chans[i][0] = inCopy_[0].data(); chans[i][1] = inCopy_[1].data();
                }
                else
                {
                    const StationBuffer* b = snap != nullptr ? snap->buffers[si].get() : nullptr;
                    players_[si].render(b, stScratch_[si][0].data(), stScratch_[si][1].data(), n);
                    chans[i][0] = stScratch_[si][0].data(); chans[i][1] = stScratch_[si][1].data();
                    if (b == nullptr) eff[si].on = false;      // пустой слот - нет и несущей
                }
                perStation[i] = chans[i];
            }

            band_.process(perStation, out_[0].data(), out_[1].data(), n, band, eff);
            buffer.copyFrom(0, pos, out_[0].data(), n);
            buffer.copyFrom(1, pos, out_[1].data(), n);
        }
        for (int c = 2; c < numOut; ++c) buffer.clear(c, 0, total);
    }

    // ---------------- UI helpers (message thread) ----------------

    float FmRxProcessor::stationFreq(int slot) const { return sp_[(size_t) slot].freq->load(); }

    bool FmRxProcessor::stationAudible(int slot) const
    {
        const auto& p = sp_[(size_t) slot];
        if (p.on->load() < 0.5f) return false;
        return (int) p.src->load() == (int) params::Source::Input
            || stations_.hasAudio(slot);
    }

    int FmRxProcessor::tunedStation() const
    {
        const double tuned = band_.meters().tunedMHz;
        int best = -1;
        double bestD = 0.06;                        // в пределах 60 кГц
        for (int i = 0; i < kNumStations; ++i)
        {
            if (! stationAudible(i)) continue;
            const double d = std::fabs(stationFreq(i) - tuned);
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    void FmRxProcessor::tuneTo(float mhz)
    {
        if (auto* prm = apvts_.getParameter(params::tune))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost(prm->convertTo0to1(mhz));
            prm->endChangeGesture();
        }
    }

    void FmRxProcessor::seek(int direction)
    {
        const float t = pTune_->load();
        float best = 0.f, wrap = 0.f;
        bool found = false, haveWrap = false;
        for (int i = 0; i < kNumStations; ++i)
        {
            if (! stationAudible(i)) continue;
            const float f = stationFreq(i);
            if (direction > 0 ? f > t + 0.03f : f < t - 0.03f)
                if (! found || (direction > 0 ? f < best : f > best)) { best = f; found = true; }
            if (! haveWrap || (direction > 0 ? f < wrap : f > wrap)) { wrap = f; haveWrap = true; }
        }
        if (found) tuneTo(best);
        else if (haveWrap) tuneTo(wrap);            // по кругу
    }

    // ---------------- state ----------------

    void FmRxProcessor::getStateInformation(juce::MemoryBlock& dest)
    {
        juce::ValueTree root("FMRX_STATE");
        root.setProperty("schemaVersion", kSchemaVersion, nullptr);
        root.appendChild(apvts_.copyState(), nullptr);
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
