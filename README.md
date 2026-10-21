# Meowbass 🐈

<img src="assets/icon.svg" width="96" align="right" alt="icon">

Free, open source psychoacoustic bass enhancer for Linux (VST3 and standalone). Splits the bass with a phase-aligned crossover, makes harmonics from it, evens out their level with a dynamics stage and adds them back so small speakers suggest deep bass.

**Manual:** [PDF with interface tour, opens in the browser](https://github.com/SpeakingColors/meowbass/blob/main/docs/Meowbass-manual.pdf)

## Status
Early scaffold. It builds in CI (Linux VST3 and standalone) but has not been tuned by ear yet.

<p align="center">
  <a href="https://ko-fi.com/studio87"><img src="https://img.shields.io/badge/Support_Studio_87-on_Ko--fi-FF5E5B?style=for-the-badge&logo=kofi&logoColor=white" alt="Support Studio 87 on Ko-fi"></a>
</p>

☕ Free and open source, made by one person. If it helps your music, a coffee on [Ko-fi](https://ko-fi.com/studio87) keeps it coming. 💜

## Docs
- [Roadmap](ROADMAP.md)

## Build
CMake 3.22+, a C++17 compiler and the usual JUCE Linux libraries. `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`. JUCE and the shared cat-ui look are fetched automatically.

## License
AGPL-3.0-or-later, see [LICENSE](LICENSE).

Original design based on the general missing-fundamental idea. Other bass enhancers (for example Waves MaxxBass) are commercial and some techniques are patented: this project uses its own structure and no code, presets or artwork from them. Not legal advice; check before selling anything.
