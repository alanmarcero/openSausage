# openSausage implementation plan

**Goal:** Deliver a working four-effect native plugin, generated art, screenshot README, and passing GitHub build/release pipelines.

**Architecture:** A dependency-free DSP library owns fixed-latency saturation, stereo reverb, pitch-rise and stereo width. Each section has full bypass and parameter-reactive graphics. A JUCE processor owns host parameters and serialization; its editor shares production code with a screenshot/interaction harness.

**Tech stack:** C++20, CMake 3.22+, JUCE 8.0.9, Catch2 3.9.1, pluginval 1.0.4.

**Spec:** ../specs/2026-10-20-opensausage-design.md

## Constraints and review focus

Use openSausage consistently. Follow existing Alan Marcero plugin layouts. Test real audio behavior. Keep latency stable through bypass and automation. No processing allocations. Check zero-size blocks, state restored before prepare, asymmetric stereo input, rapid automation, and editor attachment lifetimes.

## Tasks

- [x] DSP: create `Source/Core/SausageCore.h/.cpp` and `Tests/CoreTests.cpp`. Write behavior tests, run against a passthrough stub to observe failures, then implement FIR oversampling, linked dynamics, Color, gain, smoothing, full independent bypasses, reverb, rising pitch, and width in SausageCore and SauceRack. Verify with `ctest --test-dir build-core --output-on-failure`.
- [x] Host/UI: create `Source/Plugin/PluginProcessor.h/.cpp`, `PluginEditor.h/.cpp`, `SausageLookAndFeel.h/.cpp`, and `Tools/EditorCapture.cpp`. Attach parameters, implement persistence, render generated assets with knob-reactive expression frames and per-module graphics, exercise native controls, resize placement and recall, then capture clean and driven states from the compiled editor.
- [x] Delivery: add `.github/workflows/build.yml` and `release.yml`, README, license/third-party notes, image prompts and screenshots. Build native formats, run CTest and pluginval, review the complete change, create `alanmarcero/openSausage`, push, and inspect Actions results. Fix any failures before reporting completion.

## Verification progress

22 DSP cases pass in Release and under AddressSanitizer/UndefinedBehaviorSanitizer. Native capture verifies all five bypass attachments, mixed bypass state recall, parameter-driven mascot changes, and resize handles at both size limits. Initial Linux CI exposed a fractional delay wrap overflow; reproduced locally with ASan and fixed using integer index wrapping. Hosted run 39489252218 passed all three jobs. Downloaded archives were checked; every macOS format contains both arm64 and x86_64. See ../../VERIFICATION.md.
