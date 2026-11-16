# Commercial references: saturation controls

Research checked 2026-11-16 against official product help and manuals.

1. **Parallel saturation mix.** FabFilter Saturn 2 documents an independent Mix control for blending dry and distorted signal. openSausage adds a Saturation Mix control to reduce the existing Fatness/Color saturation blend toward clean audio. At 100%, it retains the previous Fatness/Color-derived blend, so existing sessions keep their original sound.
2. **Independent saturation dynamics.** Saturn 2 documents a per-band Dynamics control for compression or gating behavior. openSausage adds a Dynamics amount for its existing linked compressor; 0% bypasses the envelope compression while Fatness still controls saturation drive.
3. **Frequency-focused saturation.** Softube Saturation Knob's manual describes Keep Low and Keep High modes. openSausage adds Neutral, Keep Low, and Keep High focus choices. The focused modes split around the existing 1.2 kHz tone pole; Keep Low leaves the low branch mostly clean, while Keep High leaves the high branch mostly clean.

Primary references:

- [FabFilter Saturn 2 Help — Band controls](https://www.fabfilter.com/help/saturn/using/bandcontrols)
- [Softube Saturation Knob Manual](https://www.softube.com/jp/user-manuals/saturation-knob)

The DSP is an independent implementation. It does not copy vendor code, models, artwork, or exact filter behavior. The analyzer's split point and original shaper modes remain openSausage-specific. Tests cover zero-mix dry transparency, compression amount, focused harmonic behavior, parameter automation/state recall, old-session defaults, and recipe defaults.
