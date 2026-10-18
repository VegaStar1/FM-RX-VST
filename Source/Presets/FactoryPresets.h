#pragma once
// Заводские пресеты: только «характер» эфира и приёмника. Станции, частота настройки, MUTE и щелчки
// пресетом не меняются. Без JUCE.
#include <array>
#include <iterator>

namespace fmrx::preset
{
    // Параметры, которые входят в пресет (порядок = порядок значений в kFactory).
    inline constexpr const char* kIds[] = {
        "noise", "fading", "faderate", "multipath", "impulse", "drift", "cleanzone", "noisezone",
        "ifbw", "demod", "deemph", "stmode", "blend", "softmute", "hicut", "speaker", "output", "afc",
        "noisetone", "noisefreq", "noisereso",
    };
    inline constexpr int kNumIds = (int) std::size(kIds);

    struct Factory
    {
        const char* name;
        const char* descEn;
        const char* descRu;
        std::array<float, kNumIds> v;
    };

    //   noise  fade rate  mpath imp  drift clean noiseZ ifbw dem de st  blend smute hicut spk  out  afc  ntone nfreq nreso
    inline const Factory kFactory[] = {
        { "CLEAN FM", "Modern clean reception, almost no noise.", "Чистый современный приём, почти без шума.",
          { -90, 0.00f, 1.0f, 0.00f, 0.00f, 0.00f,  60,  250, 150, 0, 0, 0, 0.30f, 0.50f, 0.20f, 0.00f,  0.0f, 1, 0.00f, 2500, 0.00f } },
        { "VINTAGE HI-FI", "Warm 70s tuner: wide IF, gentle drift, no AFC.", "Тёплый тюнер 70-х: широкая ПЧ, лёгкий дрейф, без AFC.",
          { -72, 0.10f, 0.5f, 0.05f, 0.00f, 0.20f,  80,  400, 180, 1, 0, 0, 0.20f, 0.25f, 0.30f, 0.12f,  0.0f, 0, -0.20f, 2500, 0.00f } },
        { "CAR RADIO", "On the move: fast fading, some multipath.", "В дороге: быстрые замирания, немного многолучёвости.",
          { -64, 0.30f, 2.6f, 0.20f, 0.08f, 0.00f,  70,  350, 120, 0, 0, 0, 0.60f, 0.60f, 0.50f, 0.25f,  0.0f, 1, 0.00f, 2500, 0.00f } },
        { "POCKET RADIO", "Tiny mono speaker, narrow and boxy.", "Маленький моно-динамик, узко и «коробочно».",
          { -58, 0.20f, 1.0f, 0.10f, 0.10f, 0.10f,  50,  300, 110, 0, 0, 1, 0.80f, 0.50f, 0.60f, 0.85f,  1.0f, 1, 0.20f, 2000, 0.35f } },
        { "DISTANT STATION", "Far away: the station barely rises above the hiss.", "Далёкая станция: едва поднимается над шумом.",
          { -42, 0.55f, 0.8f, 0.35f, 0.10f, 0.20f,  30,  700, 100, 0, 0, 0, 0.90f, 0.35f, 0.80f, 0.30f,  0.0f, 1, -0.30f, 2500, 0.00f } },
        { "NIGHT DRIVE", "Late highway: slow fades, soft instability.", "Ночное шоссе: медленные замирания, мягкая нестабильность.",
          { -55, 0.45f, 1.6f, 0.25f, 0.05f, 0.10f,  80,  600, 130, 0, 0, 0, 0.60f, 0.50f, 0.50f, 0.35f,  0.0f, 1, -0.25f, 2500, 0.00f } },
        { "HIP-HOP RADIO", "Punchy radio intro: quick catches, grain, speaker colour.", "Плотное радио-интро: быстрый захват, зерно, окраска динамика.",
          { -62, 0.15f, 1.2f, 0.10f, 0.05f, 0.00f, 120,  220, 140, 0, 0, 0, 0.50f, 0.70f, 0.45f, 0.55f,  1.0f, 1, 0.00f, 2500, 0.00f } },
        { "LO-FI TAPE RADIO", "Wobbly and dusty, like a radio recorded to cassette.", "Плывущий и пыльный, как радио, записанное на кассету.",
          { -50, 0.30f, 0.3f, 0.20f, 0.15f, 0.60f,  60,  500, 120, 1, 1, 0, 0.70f, 0.40f, 0.70f, 0.70f,  0.0f, 0, -0.50f, 1200, 0.15f } },
        { "STORM", "Thunderstorm: crackles, deep fades, unstable signal.", "Гроза: треск, глубокие провалы, нестабильный сигнал.",
          { -48, 0.70f, 3.0f, 0.40f, 0.90f, 0.30f,  50,  450, 130, 0, 0, 0, 0.80f, 0.40f, 0.60f, 0.20f,  0.0f, 1, 0.30f, 4000, 0.20f } },
        { "LOST SIGNAL", "Signal drowning in noise; stations are hard to catch.", "Сигнал тонет в шуме, станции ловятся с трудом.",
          { -35, 0.80f, 0.4f, 0.50f, 0.30f, 0.50f,  20,  900, 100, 0, 0, 0, 1.00f, 0.20f, 0.90f, 0.30f,  0.0f, 1, 0.00f, 2500, 0.00f } },
        { "TUNING SWEEP", "Wide zones and no mute: long noisy sweeps between stations.", "Широкие зоны без приглушения: длинные шумные переходы.",
          { -55, 0.20f, 1.0f, 0.10f, 0.05f, 0.10f, 150, 1500, 200, 0, 0, 0, 0.50f, 0.00f, 0.30f, 0.10f, -2.0f, 1, 0.10f, 3200, 0.25f } },
        { "DX OVERSEAS", "Long-distance reception: narrow IF, slow deep fades.", "Дальний приём: узкая ПЧ, медленные глубокие замирания.",
          { -38, 0.90f, 0.15f, 0.60f, 0.20f, 0.40f, 25,  800,  80, 1, 0, 0, 1.00f, 0.30f, 0.85f, 0.25f,  0.0f, 1, -0.20f, 1500, 0.30f } },
    };
    inline constexpr int kNumFactory = (int) std::size(kFactory);
}
