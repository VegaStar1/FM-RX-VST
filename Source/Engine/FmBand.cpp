#include "FmBand.h"
#include <algorithm>
#include <cmath>

namespace fmrx
{
    using namespace dsp;

    namespace
    {
        // Мягкий потолок выхода: линеен до 0.9, затем плавно к 1.0.
        inline float softClip(float x) noexcept
        {
            if (! std::isfinite(x)) return 0.f;
            const float a = std::fabs(x);
            if (a <= 0.9f) return x;
            const float y = 0.9f + 0.1f * std::tanh((a - 0.9f) * 10.f);
            return x < 0 ? -y : y;
        }
    }

    void FmBand::prepare(double hostFs, int /*maxBlock*/)
    {
        hostFs_ = hostFs;
        os_ = chooseOversampling(hostFs);
        const double airFs = hostFs * os_;
        maxOffset_ = maxStationOffsetHz(airFs);

        for (int i = 0; i < kNumStations; ++i)
        {
            tx_[(size_t) i].prepare(hostFs, os_, (std::uint32_t) (i + 1));
            fader_[(size_t) i].prepare(hostFs, 0x51ED27u + (std::uint32_t) i * 7919u);
        }
        air_.prepare(airFs, 0xA5A5u);
        rx_.prepare(hostFs, os_);
        drift_.prepare(hostFs, 0xD21Fu);

        tuneK_ = 1.0 - std::exp(-1.0 / (0.04 * hostFs));
        afcK_  = 1.0 - std::exp(-1.0 / (0.6 * hostFs));
        afcHoldSamples_ = (int) (0.25 * hostFs);
        muteK_ = (float) (1.0 - std::exp(-1.0 / (0.008 * hostFs)));
        ncK_ = (float) (1.0 - std::exp(-1.0 / (0.03 * hostFs)));
        ncTone_ = -9.f; ncFreq_ = -1.f; ncReso_ = -1.f;
        latency_ = (int) std::lround(tx_[0].latencyHostSamples() + rx_.latencyHostSamples());
        emphasis_ = -1.0;
        speakerAmt_ = -1.f;
        reset();
    }

    void FmBand::reset()
    {
        for (auto& t : tx_) t.reset();
        for (auto& f : fader_) f.reset();
        air_.reset();
        rx_.reset();
        for (int c = 0; c < 2; ++c) { spkHp_[c].reset(); spkLp_[c].reset(); ncLow_[c].reset(); ncHigh_[c].reset(); ncPeak_[c].reset(); }
        ncW_ = 0.f;
        env_.fill(0.f);
        first_ = true;
        afcHz_ = 0.0;
        stillSamples_ = 0;
    }

    void FmBand::updateSpeaker(float amount)
    {
        if (std::fabs(amount - speakerAmt_) < 0.005f) return;
        speakerAmt_ = amount;
        // От «полного диапазона» к маленькому динамику: срез снизу 20->320 Гц, сверху 18->3.5 кГц.
        const double hp = 20.0 * std::pow(16.0, amount);
        const double lp = 18000.0 * std::pow(3500.0 / 18000.0, amount);
        for (int c = 0; c < 2; ++c)
        {
            spkHp_[c].setCoeffs(BiquadCoeffs::highpass(hostFs_, hp, 0.7071 + 0.6 * amount));
            spkLp_[c].setCoeffs(BiquadCoeffs::lowpass(hostFs_, lp, 0.7071 + 0.5 * amount));
        }
    }

    void FmBand::updateNoiseColour(float tone, float freqHz, float reso)
    {
        tone = std::clamp(tone, -1.f, 1.f);
        freqHz = std::clamp(freqHz, 100.f, 12000.f);
        reso = std::clamp(reso, 0.f, 1.f);
        ncActive_ = std::fabs(tone) > 0.005f || reso > 0.005f;
        if (std::fabs(tone - ncTone_) < 0.002f && std::fabs(freqHz - ncFreq_) < 1.f && std::fabs(reso - ncReso_) < 0.002f) return;
        ncTone_ = tone; ncFreq_ = freqHz; ncReso_ = reso;
        // Наклон: высокие +g, низкие -g вокруг 1 кГц; колокол - на частоте шума, подъём до +15 дБ.
        const double g = 12.0 * tone;
        const auto lo = BiquadCoeffs::lowShelf(hostFs_, 1000.0, -g);
        const auto hi = BiquadCoeffs::highShelf(hostFs_, 1000.0, g);
        const auto pk = BiquadCoeffs::peaking(hostFs_, freqHz, 0.7 + 7.3 * reso, 15.0 * reso);
        for (int c = 0; c < 2; ++c) { ncLow_[c].setCoeffs(lo); ncHigh_[c].setCoeffs(hi); ncPeak_[c].setCoeffs(pk); }
    }

    void FmBand::process(const float* const* const* stationIn, float* outL, float* outR, int numSamples,
                         const BandParams& band, const std::array<StationParams, kNumStations>& st) noexcept
    {
        // --- параметры блока ---
        if (std::fabs(band.emphasisTau - emphasis_) > 1e-9)
        {
            emphasis_ = band.emphasisTau;
            for (auto& t : tx_) t.setEmphasis(emphasis_);
        }
        ReceiverParams rp = band.rx;
        rp.deemphasisTau = band.emphasisTau;
        air_.setParams(band.noiseDb, band.multipath, band.impulse);
        rp.noiseSigma = air_.noiseSigma();
        rx_.setParams(rp);
        updateSpeaker(std::clamp(band.speaker, 0.f, 1.f));
        updateNoiseColour(band.noiseTone, band.noiseFreqHz, band.noiseReso);
        const float outTarget = dbToGain(std::clamp(band.outputDb, -48.f, 24.f));
        const float drive = 1.f + 2.f * std::clamp(band.speaker, 0.f, 1.f);

        std::array<float, kNumStations> level {};
        for (int i = 0; i < kNumStations; ++i)
        {
            level[(size_t) i] = st[(size_t) i].on ? dbToGain(std::clamp(st[(size_t) i].levelDb, -80.f, 6.f)) : 0.f;
            tx_[(size_t) i].setStereo(st[(size_t) i].stereo);
            fader_[(size_t) i].setRate(band.fadeRateHz);
        }
        const float fading = std::clamp(band.fading, 0.f, 1.f);
        const double cleanHz = std::clamp((double) band.cleanZoneHz, 0.0, 3.0e6);
        const double noiseHz = std::clamp((double) band.noiseZoneHz, 1000.0, 5.0e6);
        // Если зона шире полосы эфира - масштабируем отстройку, чтобы станция не «заворачивалась».
        const double rfScale = std::max(1.0, (cleanHz + noiseHz) / (maxOffset_ * 0.95));
        const float muteTarget = band.mute ? 0.f : 1.f;
        if (first_) { tuneSmooth_ = band.tuneMHz; lastTarget_ = band.tuneMHz; first_ = false; }

        std::array<float, kNumStations> strengthAcc {};
        const float envK = 1.0f - std::exp(-1.0f / (0.08f * (float) hostFs_));

        for (int n = 0; n < numSamples; ++n)
        {
            tuneSmooth_ += tuneK_ * (band.tuneMHz - tuneSmooth_);
            const double lo = tuneSmooth_ * 1.0e6 + drift_.next(band.drift) + afcHz_;

            // --- передатчики (только те, что попадают в полосу эфира) ---
            std::array<Cplx, kNumStations> gain {};
            std::array<bool, kNumStations> active {};
            for (int i = 0; i < kNumStations; ++i)
            {
                const auto si = (size_t) i;
                const float inL = stationIn != nullptr && stationIn[i] != nullptr && stationIn[i][0] != nullptr ? stationIn[i][0][n] : 0.f;
                const float inR = stationIn != nullptr && stationIn[i] != nullptr && stationIn[i][1] != nullptr ? stationIn[i][1][n] : inL;
                env_[si] += envK * (0.5f * (std::fabs(inL) + std::fabs(inR)) - env_[si]);

                const Cplx g = fader_[si].next(fading);
                const float amp = level[si];
                strengthAcc[si] += amp * std::sqrt(g.norm2());
                if (amp <= 0.f) continue;

                const double offset = st[si].freqMHz * 1.0e6 - lo;
                const float zdb = zoneGainDb(offset, cleanHz, noiseHz);
                if (zdb <= kZoneOffDb + 0.01f) continue;

                active[si] = true;
                gain[si] = g * (amp * dbToGain(zdb));
                tx_[si].setOffsetHz(offset / rfScale);
                tx_[si].pushHost(inL, inR);
            }

            // --- эфир и приём на частоте Fs_air ---
            for (int p = 0; p < os_; ++p)
            {
                Cplx sum {};
                for (int i = 0; i < kNumStations; ++i)
                    if (active[(size_t) i])
                        sum += tx_[(size_t) i].nextAir(p) * gain[(size_t) i];
                rx_.processAir(air_.process(sum));
            }

            float l, r;
            rx_.processHost(l, r);

            // --- AFC: подтягиваем гетеродин к сильной станции ---
            const auto& m = rx_.meters();
            // Пока ручку крутят, AFC не копит поправку (иначе после остановки он «отматывает» её с искажением).
            if (std::fabs(band.tuneMHz - lastTarget_) > 1e-7 || std::fabs(band.tuneMHz - tuneSmooth_) > 0.002)
                stillSamples_ = 0;
            else if (stillSamples_ < afcHoldSamples_)
                ++stillSamples_;
            lastTarget_ = band.tuneMHz;
            const bool knobStill = stillSamples_ >= afcHoldSamples_;
            if (! knobStill)
                afcHz_ *= (1.0 - 20.0 * afcK_);
            else if (band.afc && m.snrDb > 14.f)
                afcHz_ = std::clamp(afcHz_ + afcK_ * (double) m.freqOffsetHz * rfScale, -50000.0 * rfScale, 50000.0 * rfScale);
            else
                afcHz_ *= (1.0 - afcK_);

            // --- цвет шума: x + w * (F(x) - x), w - «зашумлённость» по SNR ---
            ncW_ += ncK_ * (noiseWeight(m.snrDb) - ncW_);
            if (ncActive_)
            {
                const float fl = ncPeak_[0].process(ncHigh_[0].process(ncLow_[0].process(l)));
                const float fr = ncPeak_[1].process(ncHigh_[1].process(ncLow_[1].process(r)));
                l += ncW_ * (fl - l);
                r += ncW_ * (fr - r);
            }

            // --- динамик и выход ---
            if (speakerAmt_ > 0.001f)
            {
                l = spkLp_[0].process(spkHp_[0].process(l));
                r = spkLp_[1].process(spkHp_[1].process(r));
                l = std::tanh(l * drive) / drive;
                r = std::tanh(r * drive) / drive;
            }
            outGain_ += 0.002f * (outTarget - outGain_);
            muteGain_ += muteK_ * (muteTarget - muteGain_);
            if (muteGain_ < 1e-4f && band.mute) muteGain_ = 0.f;
            outL[n] = softClip(l * outGain_) * muteGain_;
            outR[n] = softClip(r * outGain_) * muteGain_;
        }

        // --- индикаторы ---
        const auto& m = rx_.meters();
        mRssi_.store(m.rssiDb, std::memory_order_relaxed);
        mSnr_.store(m.snrDb, std::memory_order_relaxed);
        mStereo_.store(m.stereo, std::memory_order_relaxed);
        mBlend_.store(m.blend, std::memory_order_relaxed);
        mAfc_.store((float) afcHz_, std::memory_order_relaxed);
        mNoisy_.store(ncW_, std::memory_order_relaxed);
        mTuned_.store(tuneSmooth_ + afcHz_ * 1e-6, std::memory_order_relaxed);
        const float invN = numSamples > 0 ? 1.f / (float) numSamples : 0.f;
        for (int i = 0; i < kNumStations; ++i)
        {
            mStrength_[(size_t) i].store(strengthAcc[(size_t) i] * invN, std::memory_order_relaxed);
            mActivity_[(size_t) i].store(env_[(size_t) i], std::memory_order_relaxed);
        }
    }

    BandMeters FmBand::meters() const noexcept
    {
        BandMeters b;
        b.rssiDb = mRssi_.load(std::memory_order_relaxed);
        b.snrDb = mSnr_.load(std::memory_order_relaxed);
        b.stereo = mStereo_.load(std::memory_order_relaxed);
        b.blend = mBlend_.load(std::memory_order_relaxed);
        b.afcHz = mAfc_.load(std::memory_order_relaxed);
        b.noisiness = mNoisy_.load(std::memory_order_relaxed);
        b.tunedMHz = mTuned_.load(std::memory_order_relaxed);
        for (int i = 0; i < kNumStations; ++i)
        {
            b.strength[(size_t) i] = mStrength_[(size_t) i].load(std::memory_order_relaxed);
            b.activity[(size_t) i] = mActivity_[(size_t) i].load(std::memory_order_relaxed);
        }
        return b;
    }
}
