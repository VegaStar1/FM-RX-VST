#pragma once
// FmBand - весь FM-диапазон: 8 передатчиков -> эфир -> приёмник. Чистый C++ (без JUCE): тестируется отдельно.
//
// Модель: эфир считается в комплексной полосе шириной Fs_air (~384 кГц) вокруг частоты настройки.
// Зона приёма станции (регулируется): в «чистой» зоне несущая приходит полностью, в «шумовой» её уровень
// плавно падает (до -80 дБ) и станция тонет в шуме эфира. Если зона шире, чем помещается в полосу эфира,
// отстройка масштабируется (станция остаётся внутри ПЧ, характер расстройки сохраняется, но мягче).
// Станции вне зоны не считаются вовсе - звук у них продолжает идти, но DSP не тратится.
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
        // зона приёма станции
        float  cleanZoneHz = 110000.f;  // половина ширины «чистой» зоны
        float  noiseZoneHz = 30000.f;   // ширина перехода «шум -> станция» с каждой стороны
        bool   mute = false;            // полное выключение выхода (с затуханием ~8 мс)
        // цвет шума: действует пропорционально «зашумлённости» сигнала (по SNR), чистую станцию не трогает
        float  noiseTone = 0.f;         // -1 глухой ... +1 звонкий (наклон ±12 дБ вокруг 1 кГц)
        float  noiseFreqHz = 2500.f;    // центр резонансной полосы
        float  noiseReso = 0.f;         // 0..1: резкость (подъём до +15 дБ, Q до 8)
        // приёмник
        dsp::ReceiverParams rx;
        double emphasisTau = 50e-6; // общий для передатчиков и приёмника
        float  speaker = 0.f;      // 0..1
        float  outputDb = 0.f;
    };

    struct BandMeters
    {
        float rssiDb = -120.f, snrDb = 0.f, stereo = 0.f, blend = 0.f;
        float noisiness = 0.f;     // 0 - чистый приём, 1 - один шум (вес цвета шума)
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

        // Уровень несущей станции в точке приёма в дБ по отстройке (0 в чистой зоне, до -80 на краю шумовой);
        // <= kZoneOffDb - станции нет. Чистая функция: её же используют поиск и шкала UI.
        static constexpr float kZoneOffDb = -80.f;
        static float zoneGainDb(double offsetHz, double cleanHz, double noiseHz) noexcept
        {
            const double a = offsetHz < 0 ? -offsetHz : offsetHz;
            cleanHz = cleanHz > 0.0 ? cleanHz : 0.0;
            noiseHz = noiseHz > 1000.0 ? noiseHz : 1000.0;
            if (a <= cleanHz) return 0.f;
            if (a >= cleanHz + noiseHz) return kZoneOffDb;
            const double t = (a - cleanHz) / noiseHz;
            return (float) (kZoneOffDb * t * t * (3.0 - 2.0 * t));   // плавно у обоих краёв перехода
        }
        double maxOffsetHz() const noexcept { return maxOffset_; }

        // Вес окраски шума по SNR: 1 при SNR <= 6 дБ, 0 при SNR >= 28 дБ (чистая функция).
        static float noiseWeight(float snrDb) noexcept
        {
            const float t = (28.f - snrDb) / 22.f;
            return t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
        }

    private:
        void updateSpeaker(float amount);
        void updateNoiseColour(float tone, float freqHz, float reso);

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
        float outGain_ = 1.f, muteGain_ = 1.f, muteK_ = 0.01f;

        // цвет шума
        float ncTone_ = -9.f, ncFreq_ = -1.f, ncReso_ = -1.f, ncW_ = 0.f, ncK_ = 0.01f;
        bool  ncActive_ = false;
        dsp::Biquad ncLow_[2], ncHigh_[2], ncPeak_[2];

        // индикаторы
        std::atomic<float> mRssi_ { -120.f }, mSnr_ { 0.f }, mStereo_ { 0.f }, mBlend_ { 0.f }, mAfc_ { 0.f }, mNoisy_ { 0.f };
        std::atomic<double> mTuned_ { 98.0 };
        std::array<std::atomic<float>, kNumStations> mStrength_ {}, mActivity_ {};
    };
}
