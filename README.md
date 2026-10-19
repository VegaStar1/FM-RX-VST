# FM·RX — 8-station FM band receiver (VST3)

A VST3 plug-in built with JUCE 8: up to eight stereo stations broadcast on one FM band (87.5–108 MHz) and a virtual
receiver lets you tune between them. Inspired by the [FM Radio](http://ssll.fm/fm-radio/) Max for Live device by
SSLL DSP; the code is written from scratch, with DSP based on the author's open
[hefo/maxmsp/radiofm](https://github.com/hefo/maxmsp/tree/main/radiofm) abstractions (MIT).

Unlike typical "radio effects", the sound goes through **real FM modulation and demodulation**. Whistles and
distortion when detuned, capture of the stronger station, a neighbour breaking through a wide filter and stereo
collapsing on a weak signal all happen by themselves rather than being faked.

## Download

Installers are attached to each [release](https://github.com/VegaStar1/FM-RX-VST/releases):

| Platform | File | Installs to |
|---|---|---|
| Windows 10/11 (x64) | `FM-RX-<version>-Windows.zip` (installer + manuals) or `FM-RX-<version>-Windows-Setup.exe` | `C:\Program Files\Common Files\VST3` |
| macOS 10.13+ (Apple Silicon & Intel) | `FM-RX-<version>-macOS.zip` (installer + manuals) or `FM-RX-<version>-macOS.pkg` | `/Library/Audio/Plug-Ins/VST3` |

Both installers also install the PDF user manual (English and Russian, [docs/manual](docs/manual)). On first launch the
plug-in shows a short help window; later it is under the gear button → Help.

## Features

| Area | What it does |
|---|---|
| Stations ×8 | audio file (WAV, AIFF, MP3, FLAC, OGG) or the track input; **LOOP** or **1-SHOT** (plays once, then the receiver searches for the next station by itself); frequency, transmitter power, stereo, on/off; RDS name; drag & drop; mini waveform with trim markers |
| Station editor | click a row: waveform, **START / END** markers (non-destructive trim, snap to zero crossings), fade-in / fade-out, zoom and overview, auto-trim silence, "▶ FROM START", typed times |
| Controls | **SEARCH ◀ ▶** (sweeps the band to the next receivable station, skips weak ones), **SCAN** (stops on every station), **STOP** (double — everything from the start), **LOCK**, **MUTE**; SEARCH row: speed, sensitivity, button click volume; HAND-OVER row: pause and noise of the automatic switch after a 1-SHOT, keep Tune automation |
| Reception zone | **CLEAN ZONE / NOISE ZONE**: where a station is clean and how slowly it rises out of the noise; drawn on the band |
| Transmitter | 15 kHz low-pass, 50/75 µs pre-emphasis, limiter, stereo MPX (L+R, 19 kHz pilot, L−R at 38 kHz), ±75 kHz FM |
| Air | noise, Rayleigh fading (depth and rate), multipath, impulse interference, oscillator drift; noise colour (tone, frequency, sharpness) |
| Receiver | TUNE (automatable), AFC, 40–200 kHz IF filter, QUAD or PLL demodulator, stereo decoder with pilot PLL, auto blend to mono, de-emphasis, soft mute, hi-cut, speaker colour, output |
| Presets | 12 factory presets (CLEAN FM, VINTAGE HI-FI, CAR RADIO, POCKET RADIO, DISTANT STATION, NIGHT DRIVE, HIP-HOP RADIO, LO-FI TAPE RADIO, STORM, LOST SIGNAL, TUNING SWEEP, DX OVERSEAS) + your own |
| Settings (⚙) | English / Russian interface, 5 skins, help |
| Interface | frequency and RDS display, S-meter and SNR, band view with station zones and the needle (click, drag, wheel), resizable window |

Transmitter power affects noise and capture, not loudness — just like real FM.

## Building

Requires CMake ≥ 3.22 and a C++17 compiler. JUCE 8.0.4 is fetched automatically (or pass `-DFMRX_JUCE_DIR=path`).

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The plug-in ends up in `build/FmRx_artefacts/Release/VST3/FM RX.vst3`. GitHub Actions builds Windows and macOS on every
push, runs the core tests with sanitizers and [pluginval](https://github.com/Tracktion/pluginval), builds the
installers, test-installs them and creates a draft release — see [docs/RELEASE.md](docs/RELEASE.md).

Core tests without JUCE:

```
g++ -std=c++17 -O2 -ISource tests/test_fm.cpp Source/Engine/FmBand.cpp -o fm_tests && ./fm_tests
g++ -std=c++17 -O2 -ISource tests/test_control.cpp -o control_tests && ./control_tests
g++ -std=c++17 -O2 -ISource tests/render_demo.cpp Source/Engine/FmBand.cpp -o render_demo && ./render_demo demo.wav
```

## Code layout

Details are in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

```
Source/
  Dsp/        FmCommon, Polyphase (×L / ÷L), FmTransmitter, AirChannel, Receiver, ClickGen, Biquad, FirstOrder, Rng
  Engine/     FmBand — the whole band: 8 transmitters -> air -> receiver; RadioControl — search, scan, LOCK,
              STOP/REWIND, 1-SHOT hand-over (plain C++, no JUCE)
  Stations/   StationPrep (resampling, levelling), StationManager (lock-free loading, trims),
              LoopPlayer (region, loop / one-shot), Waveform (peaks, auto-trim, zero crossings)
  Presets/    FactoryPresets, PresetManager
  UI/         Theme (skins), Strings (EN/RU), AppSettings, HelpPanel, TunerDisplay, StationRow, StationEditor,
              WaveDraw, SettingsPanel
  Parameters.h, PluginProcessor, PluginEditor
packaging/    Windows (Inno Setup) and macOS (pkg) installers, release notes, INSTALL.txt
docs/manual/  user manual generator, screenshots and PDFs
tests/        test_fm (DSP), test_control (search, 1-SHOT, clicks), test_player, render_demo, plugin_check
```

## Licences

The DSP algorithms are based on [radiofm](https://github.com/hefo/maxmsp/tree/main/radiofm) © 2025 Henrik Forssell, MIT —
see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). JUCE is used under its own licence terms. VST is a trademark of
Steinberg Media Technologies GmbH.
