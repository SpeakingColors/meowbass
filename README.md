# Meowbass 🐈

<img src="assets/icon.svg" width="96" align="right" alt="icon">

Free, open source psychoacoustic bass enhancer for Linux (VST3 and standalone). Splits the bass with a phase-aligned crossover, makes harmonics from it, evens out their level with a dynamics stage and adds them back so small speakers suggest deep bass.

## Status
Early scaffold. It builds in CI (Linux VST3 and standalone) but has not been tuned by ear yet. Free and open source. Donations welcome: https://ko-fi.com/studio87

## Docs
- [Roadmap](ROADMAP.md)

## Build
CMake 3.22+, a C++17 compiler and the usual JUCE Linux libraries. `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`. JUCE and the shared cat-ui look are fetched automatically.

## License
AGPL-3.0-or-later, see [LICENSE](LICENSE).

Original design based on the general missing-fundamental idea. Other bass enhancers (for example Waves MaxxBass) are commercial and some techniques are patented: this project uses its own structure and no code, presets or artwork from them. Not legal advice; check before selling anything.
