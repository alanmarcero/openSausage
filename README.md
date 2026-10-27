# openSausage

[Install with OpenVST Manager](https://github.com/alanmarcero/alan-m-OpenVSTManager/releases/latest)

[![build](https://github.com/alanmarcero/openSausage/actions/workflows/build.yml/badge.svg)](https://github.com/alanmarcero/openSausage/actions/workflows/build.yml)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](LICENSE)

**Saturation, reverb, rising pitch, and stereo width. Four effects in one native plugin.**

![openSausage native editor](assets/native-ui.png)

*Captured from the released v1.1.0 VST3 editor. [Capture details and provenance](docs/SCREENSHOTS.md).*

openSausage combines the two-knob saturation workflow of **Sausage Fattener** with original implementations of the three effect types in **Secret Sauce**: Space In-Your-Face, Eternal Return, and Wide Awake Rainbow. Turn up the heat, add a room, lift the pitch, and stretch the stereo field without opening four plugin windows.

Built by Alan Marcero with C++20 and JUCE 8, following [openGlitchFX](https://github.com/alanmarcero/openGlitchFX) and [Analog Master](https://github.com/alanmarcero/analog-master-vst).

## Four effects, four full bypass switches

| Section | Controls | What it does |
| --- | --- | --- |
| **Sausage** | Fatness · Color | Stereo-linked compression and 4× oversampled saturation, with high-frequency emphasis. |
| **Space** | Mix · Size | Stereo reverb, from short rooms to long diffuse tails. Size sets the decay. |
| **Rise** | Rise · Mix | Dual-head pitch shifting up to an octave, high-pass lift, and recirculating pitch feedback. Automate Rise for buildups. |
| **Width** | Width · Spread | Mid/side expansion plus decorrelation that creates stereo information from a mono source. Spread changes the delay spacing. |

The chain runs **Sausage → Space → Rise → Width → Output**. All four can run together or be bypassed independently. Click **ACTIVE** to put a section into **BYPASSED**. After a short smoothed transition, a bypassed section passes its incoming signal unchanged, removes its wet signal and tails, and leaves the other sections running. Reverb and delay buffers clear when their fade completes.

**Master bypass** skips the complete chain, including output trim, while preserving latency. **Output** trims the processed signal from −24 to +12 dB. Levels are not automatically limited; use the output meter and trim to keep headroom.

## A sausage with an attitude

Fatness changes the mascot from a cheeky grin to a furious expression, fattens its body, and adds movement. The four sections also have reactive graphics: a saturating waveform, expanding reverb rings, rising arrows, and a widening stereo display. These read the same parameters as the DSP, so presets and host automation animate them too. Bypassed sections return to their resting graphics.

The mascot expressions and metal faceplate are generated artwork embedded in the plugin. Generation prompts are in [assets/PROMPTS.md](assets/PROMPTS.md).

## Use it

1. Insert openSausage on an audio track or bus. It starts neutral.
2. Choose a recipe or turn up one section at a time. **Gentle heat**, **Wide drums**, **In orbit**, and **Lift off** provide starting points; **Clean slate** resets everything. Selecting a recipe also re-enables the sections and master processing.
3. Automate any knob or bypass switch in your DAW. Settings are saved with the session.
4. Drag a knob to adjust it, double-click to reset it, or click its value to enter a number. Hover for help. Resize the editor from its corner.

Mono and stereo layouts are supported. Width needs a stereo output; on a mono track it passes through. Width preserves the mid signal, but the added stereo impression disappears when summed to mono. Saturation reports a fixed **16 samples** of latency in every state, including bypass. The host can compensate for it.

## Download and install

1. Download and install [OpenVST Manager](https://github.com/alanmarcero/alan-m-OpenVSTManager/releases/latest) for macOS or Windows.
2. Open the manager and refresh the catalog.
3. Choose **openSausage**, select **VST3** (or **AU** on macOS), and install.
4. Rescan plugins in your DAW. If you chose a custom install folder, add it to the DAW's search paths.

Build packages are available under **Artifacts** on a successful [Actions run](https://github.com/alanmarcero/openSausage/actions/workflows/build.yml). The [v1.1.0 release](https://github.com/alanmarcero/openSausage/releases/tag/v1.1.0) contains the formats listed below.

| Platform | Package contents | Install location |
| --- | --- | --- |
| macOS 11+, Apple Silicon / Intel | Universal VST3, AU, standalone app | VST3: `~/Library/Audio/Plug-Ins/VST3/`; AU: `~/Library/Audio/Plug-Ins/Components/` |
| Windows x64 | VST3, standalone executable | VST3: `C:\Program Files\Common Files\VST3\` |

Extract the package, copy the **entire** `openSausage.vst3` bundle (or the AU `.component`) into the appropriate folder, then rescan plugins in your DAW. The standalone app is included for auditioning through an audio interface; configure its input/output device before using it.

macOS builds are signed ad hoc, not notarized with a Developer ID. macOS may require approval in Privacy & Security when opening a downloaded build. Windows builds are unsigned.

## Build

Requirements: CMake 3.22+, Git, and a C++20 compiler (Xcode/Clang on macOS or Visual Studio 2022 on Windows). CMake fetches pinned JUCE 8.0.9 and Catch2 3.9.1 sources on the first configure.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

For a universal macOS build, add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0` to the configure command. Windows uses the Visual Studio generator; add `-A x64`.

Plugins are in `build/openSausage_artefacts/Release/`. To run the DSP suite without JUCE or a graphical environment:

```sh
cmake -S . -B build-core -DOS_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
```

Capture the real editor and run the native control/session checks:

```sh
build/sausage_editor_capture_artefacts/Release/sausage_editor_capture docs/screenshots
```

On Windows, append `.exe`. The harness exercises knobs, all five bypass switches, recipes, parameter recall, audio processing, parameter-driven mascot changes, and editor resizing before saving screenshots.

## Pipelines and verification

- **Linux:** dependency-free DSP tests with AddressSanitizer and UndefinedBehaviorSanitizer.
- **macOS:** universal VST3/AU/standalone build, DSP tests, native editor checks, ad hoc signing, pluginval 1.0.4 at strictness level 5, screenshots, and a downloadable ZIP.
- **Windows:** x64 VST3/standalone build, the same DSP/editor/VST3 validation, screenshots, and a downloadable ZIP.
- **Release:** pushing a `v*` tag runs the build workflow and publishes both platform ZIPs only when all jobs succeed.

Tests cover delayed neutral audio, independent full bypass, tail removal, master bypass, stereo linking, harmonic generation, Color response, gain ramps, reverb tails, octave shifting, mono-to-stereo widening, reset, invalid samples, and block-size independence. pluginval adds host-facing checks for parameters, state, editor lifecycle, buses, and audio processing. The AU is built and packaged; pluginval CI validation targets VST3.

## Source map

```text
Source/Core/       Pure C++ saturation and four-effect rack
Source/Plugin/     JUCE processor, parameters, editor, and knob drawing
Tests/             Audio behavior tests
Tools/             Native editor interaction and screenshot harness
assets/            Embedded faceplate, mascot expressions, and released-editor capture
docs/SCREENSHOTS.md Released-editor capture provenance
docs/screenshots/  Earlier development captures
.github/workflows/ Build, validation, packaging, and tag releases
```

## License and reference

AGPL-3.0. Copyright © 2026 Alan Marcero. See [LICENSE](LICENSE) and [THIRD_PARTY.md](THIRD_PARTY.md).

The [Dada Life product descriptions](https://dadalife.com/plugins/?id=secret-sauce) are the workflow reference. openSausage is an independent implementation with original DSP and artwork. It is not affiliated with Dada Life or Tailored Noise, and is not a bit-identical or measured sonic reproduction of their plugins. The four sections cover Sausage Fattener plus the three Secret Sauce effects; Endless Smile is a separate product.
