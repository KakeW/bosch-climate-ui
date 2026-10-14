# Bosch Climate UI

HACS dashboard features and an ESPHome Midea external component for Bosch Climate 3000i.
Unofficial. Diagnostics and special controls depend on the device; experimental
features do not imply support on every Bosch/Midea model.

## HACS installation / migration

Add `https://github.com/KakeW/bosch-climate-ui` as a **Dashboard** custom repository
in HACS and download it. The resource is:

`/hacsfiles/bosch-climate-ui/bosch-climate-ui.js`

Type: JavaScript module. HACS may add this resource automatically.
After confirming the new resource exists, remove the two old resource entries
`/local/bosch-vane-feature.js` and `/local/bosch-climate-controls.js`, then refresh
all open HA tabs. Keep your existing thermostat YAML exactly as it is.
Do not load the old resources at the same time as this bundle: the first registered
implementation wins. Old files can be retained for rollback.

Existing feature types and configuration keys remain compatible:

```yaml
features:
  - type: climate-hvac-modes
  - type: custom:bosch-climate-controls-feature
    control: preset_mode
    climate_entity: climate.living_room
  - type: custom:bosch-climate-controls-feature
    control: fan_mode
    climate_entity: climate.living_room
  - type: custom:bosch-vane-feature
    entity: select.living_room_vane_request
    climate_entity: climate.living_room
```

The controls show fan Auto/1–5 and preset Tehostus/ECO/Yötila. Service values remain
unchanged. The vane feature tracks swing from the climate entity; fixed-position
highlighting represents the requested position, not continuous physical feedback.
A vertical position selection first disables swing, then requests the position.

Future dashboard updates are installed through HACS; card configuration stays in HA.

## ESPHome installation / migration

HACS does **not** install or flash ESP32 firmware. ESPHome Builder can load the
component from the same repository's `components/midea` folder:

```yaml
external_components:
  - source: github://KakeW/bosch-climate-ui@main
    components: [midea]
    refresh: 1h
```

Replace only your existing local `external_components` source block. Keep every
other setting: climate/sensor IDs and names, actions, Wi-Fi/API/OTA secrets,
GPIO/UART settings, logger configuration and update intervals.

For the original single local-source format, a migration helper preserves all
other YAML text and creates a timestamped backup:

```sh
python3 scripts/migrate-esphome.py /path/to/bosch-climate.yaml
python3 scripts/migrate-esphome.py /path/to/bosch-climate.yaml --apply
```

Then validate and install through ESPHome Builder. `main` is checked after its
refresh interval during validation/build; it does not update running firmware by
itself. To use a fixed version, change `@main` to `@v1.0.0` (or use `--ref v1.0.0`).
New configuration options are opt-in: updates never insert devices/entities into
an existing YAML. Old local components can be retained for rollback.

The component was originally based on ESPHome 2026.8.2. Recent YAML validation
passed; full firmware compilation most recently encountered a dependency server
certificate error. Check compatibility when upgrading ESPHome itself.

## Development

`npm test` checks labels, exact service values and unavailable-device guards.
`npm run build` creates the single HACS bundle from the two source features.
The bundled component includes vane, compressor power and extended diagnostics,
UART display toggle and configurable polling. Self-clean/Silent request switches
are optimistic requests, not confirmed physical state. GEAR experiments were removed.

License: GPL-3.0; see LICENSE and NOTICE.md. No APK, decompiled Bosch code,
credentials, device configuration or household logs are published here.
