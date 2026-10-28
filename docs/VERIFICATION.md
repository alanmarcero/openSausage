# Verification — v1.2.0, 2026-10-27

Global algorithm sets: Classic, Velvet, Punch, and Prism.

| Local macOS check | Result |
| --- | --- |
| Release DSP suite, including all four modes | 29/29 passed |
| AddressSanitizer / UndefinedBehaviorSanitizer DSP suite | 29/29 passed |
| Native VST3, AU, standalone, and editor harness build | Passed (arm64) |
| VST3 pluginval 1.0.4, strictness 5 | Passed |
| Selector, distinct audio for every choice, host automation, recipe preservation | Passed |
| Session audio recall, reopened editor, old-session fallback to Classic | Passed |
| Default, per-mode, bypassed, and minimum-size native screenshots | Captured and inspected |
| Classic comparison with 85ba290 across five rates, mono/stereo and parameter/bypass changes | Bit-identical for 614,400 compared output bytes |
| Independent rapid-switch probe | 1,270,240 samples; zero intercepted C++ allocations or nonfinite outputs |

The ADAA regression measures the folded third harmonic against direct shaping on a coherent high-frequency sine; amplitude ratios were 0.129 (Velvet), 0.170 (Punch), and 0.069 (Prism). This verifies the algorithm on that signal, not an alias-free guarantee for arbitrary audio. Existing Classic behavior remains the default for old sessions. Tests also cover repeated inputs, clip boundaries, latest-choice switching in mono/stereo, neutral and bypassed delay alignment, and block-size independence.

The tag workflow builds and validates the macOS universal and Windows x64 packages before publishing [v1.2.0](https://github.com/alanmarcero/openSausage/releases/tag/v1.2.0). The local run did not perform a listening assessment or a DAW/AU host test. Algorithm sources and tradeoffs are in [ALGORITHMS.md](ALGORITHMS.md).

---

# Verification — 2026-10-20

Verified implementation: `979b26c019ca0fb3763763ad2c4e510826c6290f`.

[Successful GitHub Actions run](https://github.com/alanmarcero/openSausage/actions/runs/39489252218)

| Check | Result |
| --- | --- |
| Linux DSP tests with AddressSanitizer / UndefinedBehaviorSanitizer | 22/22 passed |
| macOS universal build: VST3, AU, standalone | Passed |
| Windows x64 build: VST3, standalone | Passed |
| DSP suite on macOS and Windows | 22/22 passed on each |
| Native controls, five bypass attachments, recipe selection, session recall with mixed bypass states | Passed on both platforms |
| Parameter-driven mascot change and resize-handle placement at minimum/maximum size | Passed on both platforms |
| VST3 pluginval 1.0.4, strictness 5 | Passed on both platforms |
| Native editor screenshot generation | Five images on each platform |
| Local macOS release and ASan/UBSan suites | 22/22 passed in each |

Downloaded and inspected both CI archives. Windows includes the VST3 bundle and standalone executable. macOS includes VST3, AU, and standalone; `lipo -archs` confirms `arm64 x86_64` for each packaged executable. Both archives include the license and installation README. CI signs macOS bundles ad hoc.

An independent source review identified a resize-coordinate defect; a native regression assertion failed before the fix and passed after it. The first Linux sanitizer run caught a fractional delay wrap overflow. A local ASan reproduction confirmed it; integer index wrapping fixed the issue. Both local sanitizers and the hosted rerun pass.

The AU was compiled and packaged; VST3 is the format validated by pluginval. No DAW listening comparison against the proprietary reference plugins was performed. The tag-triggered release workflow is configured; platform packages for this build are available from the successful Actions run above.
