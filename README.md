# FM·RX — 8-станционный FM-приёмник (VST3)

VST3-плагин на JUCE 8: до восьми стерео-станций на общем FM-диапазоне 87.5–108 МГц и виртуальный приёмник,
которым вы между ними настраиваетесь. Идея — Max for Live-девайс [FM Radio от SSLL DSP](http://ssll.fm/fm-radio/);
код написан с нуля, DSP — по мотивам открытых абстракций автора [hefo/maxmsp/radiofm](https://github.com/hefo/maxmsp/tree/main/radiofm) (MIT).

Главное отличие от «радио-эффектов»: звук проходит **настоящую ЧМ-модуляцию и демодуляцию**. Поэтому свист и
искажения на расстройке, захват сильной станции, прорыв соседки при широком фильтре и распад стерео на слабом
сигнале получаются сами, а не имитируются.

## Что умеет

| Блок | Функции |
|---|---|
| Станции ×8 | файл (WAV, AIFF, MP3, FLAC, OGG) или вход дорожки; **LOOP** (по кругу) или **1-SHOT** (один раз, затем приёмник сам ищет следующую); частота, мощность передатчика, стерео, вкл; RDS-имя; drag & drop; мини-волна с маркерами обрезки |
| Редактор станции | клик по строке: волна, маркеры **НАЧАЛО / КОНЕЦ** (неразрушающая обрезка, прилипают к нулю волны), fade-in / fade-out, зум и обзор, авто-обрезка тишины, «▶ С НАЧАЛА», ввод времени вручную |
| Управление | **ПОИСК ◀ ▶** (листает диапазон до принимаемой станции, слабые пропускает), **СКАН** (пауза на каждой станции), **СТОП** (двойной - всё в начало), **LOCK**, **MUTE**; строка «ПОИСК»: скорость, чувствительность, громкость щелчков кнопок |
| Зона приёма | **CLEAN ZONE / NOISE ZONE**: где станция чистая и как долго она проступает из шума; зоны видны на шкале |
| Передатчик | ФНЧ 15 кГц, преэмфазис 50/75 мкс, лимитер, стерео MPX (L+R, пилот 19 кГц, L−R на 38 кГц), ЧМ ±75 кГц |
| Эфир | шум, рэлеевские замирания (глубина и скорость), многолучёвость, импульсные помехи, дрейф гетеродина |
| Приёмник | TUNE (автоматизируется), AFC, ПЧ-фильтр 40–200 кГц, демодулятор QUAD или PLL, стереодекодер с ФАПЧ по пилоту, авто-бленд в моно, деэмфазис, soft mute, hi-cut, окраска динамиком, выход |
| Пресеты | 12 заводских (CLEAN FM, VINTAGE HI-FI, CAR RADIO, POCKET RADIO, DISTANT STATION, NIGHT DRIVE, HIP-HOP RADIO, LO-FI TAPE RADIO, STORM, LOST SIGNAL, TUNING SWEEP, DX OVERSEAS) + свои |
| Настройки (⚙) | язык русский / английский, 5 скинов |
| Интерфейс | табло частоты и RDS, S-метр и SNR, диапазон с зонами станций и стрелкой (клик, перетаскивание, колесо), изменяемый размер окна |

Мощность передатчика влияет на шум и захват, а не на громкость — как в настоящем FM.

## Установка

Готовые установщики - в релизах GitHub (выпускаются по тегу `vX.Y.Z`, см. [docs/RELEASE.md](docs/RELEASE.md)):
`FM-RX-<версия>-Windows-Setup.exe` (VST3 -> `C:\Program Files\Common Files\VST3`) и `FM-RX-<версия>-macOS.pkg`
(универсальный, VST3 -> `/Library/Audio/Plug-Ins/VST3`). Оба ставят PDF-инструкцию на русском и английском
([docs/manual](docs/manual)). При первом запуске плагин показывает краткую помощь; потом - шестерёнка -> Помощь.

## Сборка

Нужны CMake ≥ 3.22 и компилятор C++17. JUCE 8.0.4 скачивается автоматически (или `-DFMRX_JUCE_DIR=путь`).

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Готовый плагин: `build/FmRx_artefacts/Release/VST3/FM RX.vst3`. GitHub Actions собирает его на каждый push
(артефакт `FM-RX-vst3-win-x64`) и прогоняет тесты ядра с санитайзерами.

Тесты ядра без JUCE:

```
g++ -std=c++17 -O2 -ISource tests/test_fm.cpp Source/Engine/FmBand.cpp -o fm_tests && ./fm_tests
g++ -std=c++17 -O2 -ISource tests/render_demo.cpp Source/Engine/FmBand.cpp -o render_demo && ./render_demo demo.wav
```

## Устройство

Подробно — в [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

```
Source/
  Dsp/        FmCommon, Polyphase (×L / ÷L), FmTransmitter, AirChannel, Receiver, ClickGen, Biquad, FirstOrder, Rng
  Presets/    FactoryPresets, PresetManager
  Engine/     FmBand — весь диапазон: 8 передатчиков -> эфир -> приёмник; RadioControl — поиск, скан, LOCK,
              STOP/REWIND, one-shot (чистый C++, без JUCE)
  Stations/   StationPrep (ресемплинг, нормализация), StationManager (загрузка без блокировок, обрезка),
              LoopPlayer (отрезок, loop / one-shot), Waveform (волна, авто-обрезка, переход через ноль)
  UI/         Theme (скины), Strings (RU/EN), AppSettings, TunerDisplay, StationRow, StationEditor, WaveDraw, SettingsPanel
  Parameters.h, PluginProcessor, PluginEditor
tests/        test_fm (DSP), test_control (поиск, one-shot, щелчки), test_player, render_demo, plugin_check
```

## Дальше

- FM·TX — лёгкий плагин-передатчик на других дорожках (звук в приёмник через общий буфер)
- живой FFT-спектр эфира, сайдчейн как отдельный источник

## Лицензии

DSP-алгоритмы основаны на [radiofm](https://github.com/hefo/maxmsp/tree/main/radiofm) © 2025 Henrik Forssell, MIT —
см. [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). JUCE распространяется на своих условиях (AGPLv3 / коммерческая).
