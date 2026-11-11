# Optional Moltorino native glass plugin — milestone 1

A separately built and packaged Qt plugin, not a Lua plugin. Toggle it with
**Settings > Interface > Enable experimental Liquid Glass plugin**.

- Loads only the trusted packaged module from `<exe>/glass/`.
- Paints the input composer, channel header, and tabs, with smaller composer buttons.
- Defaults to disabled; a missing or invalid module falls back to normal painting.
- Does not access Twitch tokens, private settings, or network APIs.
- Existing profile/settings locations and Moltorino M15.5.2 remain unchanged.

**Current limitation:** This is the verified plugin architecture and optical
edge-light visual prototype only. It does not yet perform real pixel sampling,
backdrop blur, distortion or refraction. Those require a carefully engineered
backdrop compositor and separate performance validation; don't call this first
iteration the completed Apple Liquid Glass effect.

CI captures `Baseline` (module off) and `Glass-Dark` (module on) to compare
the actual application screenshots before any wider rollout. Do not merge this
experimental branch until the user approves the appearance.
