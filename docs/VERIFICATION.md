# Verification — 2026-10-20

Verified implementation: `b3f85bfa39deef2ff4534ecef0d98a225deaa2b9`.

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
