# openSausage

A four-part effect plugin for adding saturation, reverb, rising pitch, and stereo width to a track or bus.

![openSausage editor](assets/native-ui.png)

[Download the latest release](https://github.com/alanmarcero/openSausage/releases/latest) · [Install with OpenVST Manager](https://github.com/alanmarcero/alan-m-OpenVSTManager/releases/latest)

## Effects

The sections run in this order: Sausage, Space, Rise, and Width.

- **Sausage:** Fatness and Color adjust saturation. Mix blends clean and saturated audio. Dynamics controls compression. Neutral Focus saturates all frequencies. Keep Low protects bass; Keep High keeps treble cleaner.
- **Space:** Mix controls stereo reverb amount. Size changes decay.
- **Rise:** Shift pitch upward by up to one octave. Mix sets the effect level. Automate Rise for a pitch build.
- **Width:** Widen stereo audio or create stereo from mono. Spread changes stereo spacing.

Each section has its own bypass. Master bypass skips the full chain and output trim. Output trim ranges from −24 to +12 dB; the output meter helps you set levels. The plugin does not limit peaks automatically.

Choose one global style: Classic, Velvet, Punch, or Prism. It changes all four effects while keeping knob settings. Factory recipes provide starting points, including Clean slate, Gentle heat, Wide drums, In orbit, and Lift off.

DAW sessions recall settings. Controls and bypass switches support automation. The grilled sausage illustration rocks on the host beat and follows tempo changes and seeks. It rests when the host stops. Standalone uses an internal 120 BPM clock. The four effect displays follow the same clock. Drag a knob to adjust it, double-click to reset it, click its value to type, or hover for help. Resize the editor from its corner.

The plugin supports mono and stereo audio. Width needs a stereo output, and its added width disappears when summed to mono. Reported latency stays fixed at 16 samples.

## Formats

- **macOS 11 or newer:** Universal VST3, Audio Unit (AU), and standalone app.
- **Windows x64:** VST3 and standalone app.

Use [OpenVST Manager](https://github.com/alanmarcero/alan-m-OpenVSTManager/releases/latest) or download a package from the [latest release](https://github.com/alanmarcero/openSausage/releases/latest).

## License

AGPL-3.0. See [LICENSE](LICENSE) and [third-party notices](THIRD_PARTY.md).
