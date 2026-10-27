# Global algorithms

Reviewed 2026-10-27. Classic is the existing openSausage chain. Velvet, Punch, and Prism are coordinated alternatives selected by one host parameter, `algorithm`. They change the processing in all four sections at the same knob settings. They are not knob presets or emulations of named hardware.

## Saturation

All modes retain the stereo-linked compressor, Color emphasis, DC blocker, and 4× FIR oversampling. Classic retains `tanh(x)`. The other modes use the first-order antiderivative quotient `y = (F(x) - F(previous)) / (x - previous)` inside the oversampled section:

| Mode | Curve f(x) | Antiderivative F(x) |
| --- | --- | --- |
| Velvet | `2/pi * atan(x)` | `2/pi * (x*atan(x) - log(1+x*x)/2)` |
| Punch | `clamp(x, -1, 1)` | `x*x/2` inside ±1, otherwise `abs(x)-1/2` |
| Prism | `sin(x)` | `1-cos(x)` |

Double-precision antiderivatives and a midpoint fallback for nearly equal consecutive inputs avoid division instability. Prism folds the signal as drive increases; its loudness is deliberately non-monotonic. Antialiasing reduces artifacts rather than guaranteeing an alias-free result. First-order ADAA also averages over half an oversampled sample, introducing a small high-frequency rolloff and approximately 0.125 base-rate samples of extra wet-path group delay at low levels. The host delay and neutral/bypassed paths remain 16 samples.

Source: Parker, Zavalishin, and Le Bivic, [Reducing the aliasing of nonlinear waveshaping using continuous-time convolution](https://www.dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf), DAFx 2016, especially the rectangular-kernel method and hard-clipper example. Atan and sine antiderivatives are derived here; no source code was copied.

## Reverb

Classic keeps the eight independent damped combs followed by four Schroeder allpasses per channel. Punch uses the same structure with integer delays at 40% of the original length and feedback ranging from 0.25 to 0.75.

Velvet couples all eight delay lines with the Householder reflection `I - 2/8 * 11^T`. Prism uses an eight-point Hadamard butterfly normalized by `1/sqrt(8)`. Both matrices are orthogonal; loop gain remains below one (0.45–0.94), with lowpass damping. Velvet has stronger damping; Prism retains more high-frequency energy. The existing allpass output diffusion supplies the early tail density. These are compact original FDN designs, not full implementations of a commercial hall or a published perceptual optimization.

Sources: Julius O. Smith, [FDN Reverberation](https://ccrma.stanford.edu/~jos/pasp/FDN_Reverberation.html), and Sebastian J. Schlecht, [FDNTB: The Feedback Delay Network Toolbox](https://www.dafx.de/paper-archive/2020/proceedings/papers/DAFx2020_paper_53.pdf), DAFx 2020, for feedback matrices and stability considerations.

## Pitch

Classic keeps the 50 ms dual-head Hann crossfade. Punch uses two triangular windows over 20 ms and lower feedback. Velvet uses four Hann-windowed grains with lengths of 65–76.7 ms. Prism uses 50–59 ms grains, a stereo phase offset, and cross-channel pitch feedback. Each grain advances at the rate needed for the same requested pitch ratio, up to an octave. The four-grain modes use different window lengths and normalize their summed weights to reduce persistent cancellations that occur with uniformly spaced heads on some periodic inputs.

These are time-domain delay/granular shifts. Windowing can produce amplitude modulation and coloration on sustained tones; these modes are intended for creative effects and buildups, not transparent polyphonic correction.

Source: GRAME's official [Faust miscellaneous-effects documentation](https://faustlibraries.grame.fr/libs/misceffects/#pitch-shifting), including `transpose` and overlapping Hann-windowed taps. The unequal window lengths, feedback routing, and tuning are openSausage adaptations; no Faust code is included.

## Width

Every mode preserves the input mid signal and adds width only to the side signal, so the added width cancels on mono summing. Classic keeps the original paired delays. Velvet subtracts the outputs of two distinct four-stage first-order allpass cascades. Punch highpasses the added side signal at 250 Hz, leaving low-frequency width closer to the input. Prism modulates the paired delays in opposite directions at 0.23 Hz with a 2 ms excursion. Spread controls the base delay spacing or allpass coefficients.

Source: [Decorrelation for Immersive Audio Applications and Sound Effects](https://www.dafx.de/paper-archive/2023/DAFx23_paper_3.pdf), DAFx 2023, for the allpass decorrelation principle. Velvet is a full-band cascade, not that paper's DFT-domain decorrelator or its transient reconstruction system.

## Switching and compatibility

The `algorithm` choice is appended to the existing host parameters with version hint 2; earlier parameter IDs and choice index 0 (Classic) remain stable. State without the choice restores Classic even when loaded into an instance previously using another mode. Recipes leave the global choice alone.

Two engines are allocated during preparation. Normally only one runs; a mode change resets the spare, primes its 16-sample dry delay, and crossfades for 20 ms. No allocations occur in the audio callback. Changes arriving during a fade are coalesced into the latest requested mode. Mode changes discard the old tail through the fade. CPU temporarily increases during switching; reverb and pitch effects still have their inherent wet delays.

## Verification

The DSP suite checks distinct processing in every section, independent and master bypass, mono-sum preservation, pitch shift, reverb density/tails, reset, finite output at 8–384 kHz, and block-independent switching. The native editor harness checks all four menu choices, automation, recipe behavior, state recall including old sessions, and resized layouts. See [verification notes](VERIFICATION.md) for executed build and host checks.
