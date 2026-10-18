#pragma once
// FmBand - весь FM-диапазон: 8 передатчиков -> эфир -> приёмник. Чистый C++ (без JUCE): тестируется отдельно.
//
// Модель: эфир считается в комплексной полосе шириной Fs_air (~384 кГц) вокруг частоты настройки.
// Станция участвует, только если её спектр не «заворачивается» в полосу ПЧ (|отстройка| < ~140 кГц);
// дальние станции не считаются вовсе - звук у них продолжает идти, но DSP не тратится.
#include "Dsp/AirChannel.h"
#include "Dsp/FmTransmitter.h"
#include "Dsp/Receiver.h"
#include <array>
#include <atomic>

namespace fmrx
{
    inline constexpr int kNumStations = 8;

    struct StationParams
    {
        bool  on = true;
        double freqMHz = 98.0;
        float levelDb = -6.f;     // мощность передатчика: влияет на SNR и захват, но не на громкость
        bool  stereo = true;
    };

    struct BandParams
    {
        double tuneMHz = 98.0;
        bool   afc = true;
        float  drift = 0.f;        // 0..1
        // эфир
        float  noiseDb = -60.f;    // шум в полосе 200 кГц относительно несущей 0 дБ
        float  fading = 0.2f;      // 0..1
        float  fadeRateHz = 1.f;
        float  multipath = 0.f;    // 0..1
        float  impulse = 0.f;      // 0..1
        // приёмник
        dsp::ReceiverParams rx;
        double emphasisTau = 50e-6; // общий для передатчиков и приёмника
        float  speaker = 0.f;      // 0..1
        float  outputDb = 0.f;
    };

    struct BandMeters
    {
        float rssiDb = -120.f, snrDb = 0.f, stereo = 0.f, blend = 0.f;
        float afcHz = 0.f;
        double tunedMHz = 98.0;    // эффективная частота (с AFC и дрейфом)
        std::array<float, kNumStations> strength {};   // 0..1 сила станции в точке приёма (для спектра)
        std::array<float, kNumStations> activity {};   // огибающая звука станции
    };

    class FmBand
    {
    public:
        void prepare(double hostFs, int maxBlock);
        void reset();

        // stationIn[i][0/1] - звук станции i (частота хоста); может быть nullptr = тишина.
        void process(const float* const* const* stationIn, float* outL, float* outR, int numSamples,
                     const BandParams& band, const std::array<StationParams, kNumStations>& stations) noexcept;

        // Снимок индикаторов (запись - аудиопоток, чтение - UI; поля по отдельности атомарны по смыслу).
        BandMeters meters() const noexcept;
        int latencySamples() const noexcept { return latency_; }
        int oversampling() const noexcept { return os_; }
        double airRate() const noexcept { return hostFs_ * os_; }

        // Вес станции по отстройке: 1 в центре, плавно -> 0 к краю зоны maxOffsetHz (чистая функция).
        static float stationWeight(double offsetHz, double maxOffsetHz) noexcept;
        double maxOffsetHz() const noexcept { return maxOffset_; }

    private:
        void updateSpeaker(float amount);

        double hostFs_ = 48000.0, maxOffset_ = 140000.0;
        int os_ = 8, latency_ = 0;

        std::array<dsp::FmTransmitter, kNumStations> tx_;
        std::array<dsp::RayleighFader, kNumStations> fader_;
        std::array<float, kNumStations> env_ {};
        dsp::AirChannel air_;
        dsp::Receiver rx_;
        dsp::DriftGenerator drift_;

        double tuneSmooth_ = 98.0, afcHz_ = 0.0;
        double tuneK_ = 0.0, afcK_ = 0.0, lastTarget_ = 98.0;
        int stillSamples_ = 0, afcHoldSamples_ = 12000;
        bool first_ = true;
        double emphasis_ = -1.0;

        // динамик
        float speakerAmt_ = -1.f;
        dsp::Biquad spkHp_[2], spkLp_[2];
        float outGain_ = 1.f;

        // индикаторы
        std::atomic<float> mRssi_ { -120.f }, mSnr_ { 0.f }, mStereo_ { 0.f }, mBlend_ { 0.f }, mAfc_ { 0.f };
        std::atomic<double> mTuned_ { 98.0 };
        std::array<std::atomic<float>, kNumStations> mStrength_ {}, mActivity_ {};
    };
}
