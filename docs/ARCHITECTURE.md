# FM·RX — architecture

## Modules
| Module | Purpose | Threads |
|---|---|---|
| `Parameters.h` | VST3 parameter IDs and ranges (stable) | any |
| `Engine/FmBand` | the whole band: 8 transmitters, air, receiver, AFC, noise colour, speaker, output | audio |
| `Engine/RadioControl` | search, scan, LOCK, STOP/REWIND, 1-SHOT hand-over | audio |
| `Dsp/FmTransmitter` | station audio -> MPX -> FM in the complex baseband, shifted to the station frequency | audio |
| `Dsp/AirChannel` | multipath, Gaussian noise, impulses; `RayleighFader`, `DriftGenerator` | audio |
| `Dsp/Receiver` | IF filter, QUAD/PLL demodulator, stereo decoder with pilot PLL, decimation, de-emphasis, blend, soft mute, hi-cut | audio |
| `Dsp/Polyphase` | polyphase ×L interpolator and ÷L decimator (Kaiser) | audio |
| `Dsp/FmCommon` | uint32 phase, sine table, fast atan2, oversampling choice | audio |
| `Dsp/ClickGen` | synthesized mechanical button clicks | audio |
| `Stations/StationManager` | station files: background loading, atomic snapshot publishing, trims | message + worker |
| `Stations/LoopPlayer` | region playback: loop with crossfade or one-shot | audio |
| `Presets/*` | factory and user presets | message |
| `PluginProcessor` | parameters, state, `processBlock` | audio + message |
| `PluginEditor`, `UI/*` | interface, help, settings | message |

## Controls (RadioControl, after RTVD/RaTune)
Runs on the audio thread once per block, before FmBand. UI commands arrive atomically (the last one wins).
* SEARCH ◀/▶: the dial moves in fixed 4 ms steps (the result does not depend on the block size) and wraps around the
  band edge. A station is "receivable" when its carrier-to-noise at the dial is ≥ 35 − 30·sensitivity dB; the dial
  then settles onto its centre at 2 MHz/s. Hysteresis: from a station, the dial first leaves it. A full circle with
  nothing found returns to the start frequency.
* SCAN: the same, with a 5 s dwell on every station. LOCK: the knob and search are ignored.
* STOP: stops the search; audible stations (zone > −40 dB) fade in ~12 ms and park at their start; re-tuning
  (leave the zone and come back) starts them over. Double STOP (≤ 500 ms) = REWIND: all stations from the start,
  dial to 87.5 MHz.
* 1-SHOT: waits at its start and begins when tuned in. When it ends, it keeps a silent carrier until the dial leaves
  its zone (so the hiss rises smoothly instead of a burst). After the hand-over PAUSE (0–5 s) the receiver searches
  upwards from its frequency. A full circle with nothing found picks the strongest available station; the same one
  repeats only when there is no other. During the pause and search the output is scaled by HAND-OVER NOISE
  (60 ms smoothing).
* While the engine owns the frequency, a 20 Hz processor timer writes it back to the Tune parameter when the dial rests.
  The echo of that write is not treated as a knob move; any other knob move cancels the search. With
  **Keep Tune Automation** on, nothing is ever written to Tune, so DAWs do not switch the automation off.

## Reception zone
`FmBand::zoneGainDb`: 0 dB inside the clean zone (±CLEAN), then a smooth fall to −80 dB across NOISE ZONE
(smoothstep in dB). The station's carrier is scaled by this level and sinks into the air noise, so the station
gradually rises out of the hiss. If CLEAN+NOISE is wider than the air bandwidth allows, the offset is scaled
(rfScale) so the spectrum does not alias; AFC works in real Hz. Search and the band display use the same function.

## Noise colour
After demodulation: `y = x + w·(F(x) − x)`, where F is a ±12 dB tilt around 1 kHz (TONE) and a peaking band at
FREQUENCY with up to +15 dB and Q up to 8 (SHARPNESS). The weight `w = noiseWeight(SNR)` is 1 at SNR ≤ 6 dB and 0 at
SNR ≥ 28 dB (30 ms smoothing), so the hiss between stations is fully coloured and a clean station is untouched.

## Station trim (editor)
Non-destructive: the file is never changed. Station metadata holds trimStart, trimEnd (0 = to the end), fadeIn,
fadeOut; they are saved in the project's STATION node. `StationManager::setTrim/setFades` publish a new snapshot with
a `StationRegion` in samples next to the buffer (the audio thread gets it atomically, like the file). `LoopPlayer`
plays start..end only: LOOP crossfades 10 ms at the seam, ONE-SHOT fades out to END and signals "finished"; fade-in on
every start from START; STOP and restart park at START. Peaks (min/max in 4096 bins) are computed in the background
on load; auto-trim uses the first/last sample above the threshold ± 10 ms; markers snap to the nearest zero crossing
(±2 ms) on release. "▶ FROM START" tunes the receiver to the station and starts it from START. Relinking a moved
file keeps its trim and name.

## MUTE and clicks
MUTE is a parameter: the output (noise included) fades out in ~8 ms. ClickGen synthesizes mechanical clicks (noise
strike -> two body resonators + a low thump; MUTE gets a double click: press and latch). Command and MUTE clicks are
mixed in after MUTE, at the "Button Clicks" level.

## Two sample rates
Host audio (44.1–192 kHz) and the "air" at Fs_air = L × Fs_host ≥ 380 kHz (44.1 -> ×9, 48 -> ×8, 96 -> ×4).
The air is computed in the **complex baseband around the tuning frequency**: a station at f_k has offset
Δ_k = f_k − f_LO and appears in the air band as an FM signal at Δ_k.
Stations outside the reception zone cost no DSP.

## Per host sample
1. TUNE is smoothed (40 ms); LO = TUNE + drift + AFC correction.
2. For stations in the zone: offset, zone level, fading coefficient; `pushHost(L, R)`:
   15 kHz low-pass (Butterworth 4) -> pre-emphasis -> limiter -> M = (L+R)/2, S = (L−R)/2 -> ×L interpolators.
3. L times at the air rate: MPX = 0.9·(M + S·sin 2θ) + 0.1·sin θ (θ — pilot phase, 38 kHz = doubled phase);
   carrier phase += Δ_k + 75 kHz·MPX; sum of stations × fading -> multipath, impulses, noise ->
   IF filter (Butterworth 8 on I and Q) -> demodulator:
   * QUAD: arg(x[n]·conj(x[n−1])) — phase increment (as in radiofm.demod);
   * PLL: 2nd-order PLL with an atan2 phase detector, ~40 kHz loop (as in radiofm.demod2).
   -> 19 kHz pilot PLL (~30 Hz bandwidth) -> L+R (minus the regenerated pilot, 19 kHz notch) and
   L−R = MPX·2 sin 2θ -> 15 kHz low-pass (Butterworth 6) -> ÷L decimators.
4. At the host rate: stereo blend by pilot lock and SNR, de-emphasis, hi-cut and soft mute by SNR and the detune
   detector, noise colour, speaker colour, output level, soft ceiling, NaN/Inf guard.
5. AFC accumulates a correction (±50 kHz) only when the knob has rested for 0.25 s and SNR > 14 dB.

SNR estimate: IF power minus the known air noise power in the IF bandwidth.
Latency (≈32 samples at 48 kHz, interpolator + decimator) is reported to the host.

## Threads and files
UI -> `loadFileAsync` -> ThreadPool (decoding, resampling to the host rate, levelling to −18 dBFS RMS) ->
`callAsync` -> a new `StationSnapshot` is published atomically. The audio thread calls `acquire()` at the start of a
block; a message-thread timer frees old snapshots. An empty file slot transmits no carrier.

## State (schemaVersion = 2)
```
FMRX_STATE (schemaVersion)
 ├─ PARAMS   (APVTS: receiver, air, zones, noise colour, mute, clicks, search, hand-over,
 │            s1..s8_{on,freq,level,stereo,src,mode}; properties presetName, uiW, uiH)
 └─ STATIONS
     └─ STATION (slot, name, path, trimStart, trimEnd, fadeIn, fadeOut)
```
Older projects without trims open with the whole file; parameters added later get their defaults.
Language, skin and the help-at-startup flag are global settings (`<AppData>/NUWMUN/FM RX/FM RX.settings`);
English is the default language. User presets: `<AppData>/NUWMUN/FM RX/Presets/*.fmrxpreset` (XML).

## Tests
* `tests/test_fm.cpp` — exact tuning (THD+N ≈ 0.17 %), stereo separation (≈31 dB), mono stations, detuning, weak
  signal (loudness kept, noise grows), soft mute, PLL, harsh channel, AFC, neighbours, reception zones, mute, noise
  colour, CPU budget (≈13 % of a core with three stations in the zone).
* `tests/test_control.cpp` — search, scan, LOCK, STOP/REWIND, 1-SHOT hand-over and its pause, player modes, clicks.
* `tests/test_player.cpp` — file preparation, seamless loop, buffer swap.
* `tests/plugin_check.cpp` (`-DFMRX_BUILD_CHECK=ON`) — the processor in a console host: sound, search, MUTE,
  1-SHOT hand-over noise, Keep Tune Automation, presets, state save/restore, trims and relinking, help, UI snapshots.
