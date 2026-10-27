# Third-party software and attribution

- **JUCE 8.0.9**: https://github.com/juce-framework/JUCE/tree/8.0.9 — AGPL-3.0 or a separate commercial JUCE license. This repository uses the AGPL option. JUCE includes its own third-party notices and the Steinberg VST3 SDK notices in its source distribution.
- **Catch2 3.9.1**: https://github.com/catchorg/Catch2/tree/v3.9.1 — Boost Software License 1.0. Test dependency only.
- **pluginval 1.0.4**: https://github.com/Tracktion/pluginval/releases/tag/v1.0.4 — GPL-3.0. Downloaded for validation only; not included in the plugin.
- **Artwork**: `assets/sausage.png`, `assets/sausage-expressions.png`, and `assets/enamel.png` were generated for openSausage using the built-in image generation tool. Prompts are recorded in `assets/PROMPTS.md` and `assets/EXPRESSION-PROMPT.md`.

openSausage is an independent implementation inspired by the workflows of Sausage Fattener and the Secret Sauce bundle (Space In-Your-Face, Eternal Return, Wide Awake Rainbow). No original plugin code, binaries, artwork, or branding are included. Those product names belong to their respective owners. This project is not affiliated with Dada Life or Tailored Noise and does not claim sonic equivalence.

The additional DSP modes are original implementations of published signal-processing methods. References and the distinction between source techniques and our adaptations are listed in [docs/ALGORITHMS.md](docs/ALGORITHMS.md). No third-party DSP code or runtime dependencies were added.
