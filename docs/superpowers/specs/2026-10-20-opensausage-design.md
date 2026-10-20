# openSausage design

Deliver a native four-effect rack, generated artwork, a GitHub repository under alanmarcero with pipelines, and a README with actual screenshots. The user selected the name openSausage, then expanded the scope to Sausage Fattener plus the three Secret Sauce effects. Each section must have full bypass. Turning knobs must animate graphics and make the sausage increasingly aggressive.

Follow openGlitchFX and analog-master-vst: C++20, JUCE 8.0.9, CMake 3.22+, Catch2 3.9.1, macOS universal and Windows VST3 validation. Original DSP and artwork; no bit-identical or measured sonic equivalence is claimed.

## Audio

Fixed serial order: Sausage → Space → Rise → Width → Output.

- Sausage: Fatness and Color. 4× FIR oversampling, stereo-linked compression, saturation, Color emphasis, DC filtering. Fixed 16-sample latency with matched dry delay.
- Space: Mix and Size. Eight damped comb delays create stereo reverberation; Size controls decay.
- Rise: Rise and Mix. Dual-head pitch shifting up to one octave with high-pass lift and bounded recirculating feedback. Automating Rise produces a buildup.
- Width: Width and Spread. Mid/side scaling with decorrelated mono excitation; the mid sum is preserved. On mono-output layouts Width passes through.
- Output: −24 to +12 dB. No automatic limiter.

Each module has an independent automatable full bypass. Smooth transitions suppress switching clicks; after settling the stage is exact dry passthrough, including no wet tails. Delay buffers clear when the bypass fade completes. Master bypass includes output gain and maintains latency. All parameters persist with a host session. Five recipes reset all parameters. No processing-time allocations. Nonfinite input and settings are sanitized.

## UI

1160 × 720 resizable JUCE editor with four simultaneous panels. Each has two knobs, a visible ACTIVE/BYPASSED switch and reactive graphic. Generated three-frame sausage expressions switch with Fatness, and its body expands and moves. Knobs, presets and host automation all drive the same visuals; bypass returns graphics to rest. Native control values remain readable, with text entry, double-click reset and tooltips. JUCE-managed resize handles and tooltips stay in actual screen coordinates.

## Delivery and tests

VST3 on macOS arm64/x86_64 and Windows x64; AU on macOS; standalone on both. CI runs DSP tests, editor interaction/session recall tests, screenshot capture, VST3 pluginval 1.0.4 strictness 5 and artifact packaging. Linux DSP jobs run ASan/UBSan. Version tags publish release ZIPs after builds pass. macOS bundles are ad hoc signed, not notarized. Repository uses AGPL-3.0 consistently with JUCE's open-source option.

Tests cover neutral latency, gain, saturation harmonics, Color response, compression linking, stereo independence, reverb tails, octave rising, mid-preserving width, all bypasses, tail clearing, silence, reset, invalid samples, automation ramps, block partition independence, UI attachments, recipe selection, session recall, animation reactions, and resize-handle placement. README screenshots are native renders, not generated UI mockups. Validate CI after pushing.
