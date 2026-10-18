# Third-party notices

## radiofm (Max/MSP abstractions)

The FM modulator, channel, demodulator (quadrature and PLL) and DSB-SC stereo coding in `Source/Dsp/` are a C++
re-implementation based on the ideas in https://github.com/hefo/maxmsp/tree/main/radiofm, used under the MIT License:

```
MIT License

Copyright (c) 2025 Henrik Forssell

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.```

## FREQUENCY-VST

`Source/Dsp/Biquad.h`, `Source/Dsp/Rng.h`, `Source/Stations/StationPrep.*` and the lock-free station loading scheme
in `Source/Stations/StationManager.*` are adapted from the author's own FREQUENCY-VST project.

## JUCE

JUCE 8 (https://juce.com) is fetched at build time and is licensed under AGPLv3 or the JUCE commercial licence.
