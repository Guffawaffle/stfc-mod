# Windows launcher contracts

The standalone Windows launcher is developed and released from
[`Guffawaffle/stfc-mod-launcher`](https://github.com/Guffawaffle/stfc-mod-launcher).
This directory contains only machine-readable contracts whose source of truth
is the native mod repository:

- `config-schema.guffawaffle.v1.json` is generated from the native TOML
  defaults and presentation overrides by `scripts/generate_config_schema.mjs`.
- `runtime-manifest.guffawaffle.v1.json` describes native runtime paths and
  activation behavior consumed by launcher integrations.

Application UI, installation, update, diagnostics, and release implementation
must not be added here. Consumers should copy or download these versioned
contracts; they must not parse documentation to infer runtime behavior.
