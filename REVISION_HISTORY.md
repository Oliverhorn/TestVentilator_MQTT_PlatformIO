# Vornado ESP Firmware Revision History

This project uses `major.minor` firmware versioning. Versions below `1.0` are development snapshots. Version `1.0` is the first stable release.

Current recommended firmware:

`firmware/v1.1-vornado-firmware-version-topic.bin`

## Versions

| Version | Firmware | Status | Notes | SHA256 |
| --- | --- | --- | --- | --- |
| 0.1 | `firmware/v0.1-vornado-nonblocking-mqtt.bin` | Development | MQTT reconnect made non-blocking. | `d3b08c5b398b04ed7203c941e385437cffd7f1cc28ddcdd588cccb44d8df0d49` |
| 0.2 | `firmware/v0.2-vornado-web-mqtt-config.bin` | Development | Added web configuration for MQTT broker, port, user, and password. | `26ad7abbfb6c71459ac8b331f5e0091f280da783e166eadcbc8788e73b10bccf` |
| 0.3 | `firmware/v0.3-vornado-secure-config-fallback.bin` | Development | Added protected config/update pages and fallback setup access point. | `d94de587b75f2ddaf3ae55b1a555fbb63a2132455af833d6ef883612d7bd18c5` |
| 0.4 | `firmware/v0.4-vornado-restart-button.bin` | Development | Added restart action on the config page. | `5dd348697e5079c839e5d8dafb8d27434e44fa12aab8490fa3b6c2e021f7b177` |
| 0.5 | `firmware/v0.5-vornado-configurable-device-name.bin` | Development | Added configurable device name for MQTT topics, client id, hostname, and mDNS. | `1400bad8a7c2b50e4a32cd17c4b610464741f689f5bf4cca814e83332aa2dc48` |
| 0.6 | `firmware/v0.6-vornado-ha-mqtt-discovery.bin` | Development | Added Home Assistant MQTT discovery entities. | `1202113d3df9cc201a9b70ea09ba045f51c8d9da0289db637977843f683d3a85` |
| 0.7 | `firmware/v0.7-vornado-ha-discovery-no-availability.bin` | Development | Removed HA availability dependency after entities appeared unavailable. | `43aff890826ae10275504d01ebd03bac33b38914b076057fadebd5c870e38b4b` |
| 0.8 | `firmware/v0.8-vornado-ha-slider-speed-status.bin` | Development | Added RPM-to-percent reporting, power state, and HA speed slider. | `dad67fe42e6157211da536cdc72a100a0792768b6ea6500fd3d4ee2f01fe01b7` |
| 0.9 | `firmware/v0.9-vornado-slider-regulation-fixes.bin` | Development | Added safer slider regulation, retry limits, target state topic, and reduced MQTT telemetry noise. | `3286f1c44e980fd2f568fb77606fd01d2f3caf3ee3c0cc06e142ee9be1fb8e5b` |
| 0.10 | `firmware/v0.10-vornado-ip-topic.bin` | Development | Added retained IP address MQTT topic and HA IP sensor. | `b4ce5ceca122ce28767116db387fb73f474c0ffd0c13db9e96cdb55cf18d5005` |
| 0.11 | `firmware/v0.11-vornado-calibration-button.bin` | Development | Added MQTT/HA calibration button and calibration status sensor. | `c5366ecc7c7378b978e0afbaf59c3e0453e5427e8e415c35bf7aed87d18b4440` |
| 0.12 | `firmware/v0.12-vornado-calibration-burst-fixes.bin` | Development | Added calibration working buffer, larger calibration storage, raw IR recovery preparation, and burst-based speed jumps. | `a9e884906495494a0687e4a468fd25aae716600947085f52756337029c339a05` |
| 0.13 | `firmware/v0.13-vornado-target-1-99.bin` | Development | Corrected desired speed range to Vornado's 1-99 scale; removed hidden speed 0 power-off behavior. | `43ab2c67e3bab7c7170752e09a492883bafbc83f39b54d875f4708cea24f10a1` |
| 0.14 | `firmware/v0.14-vornado-native-symphony-ir-regression.bin` | Regression / Do not use | Tested native `sendSymphony()` encoder. Manual Plus/Minus did not work with the installed hardware. | `7eced1b51b0918d3c794679f257d44752ebd450d9e24612da1eacc535e908d71` |
| 0.15 | `firmware/v0.15-vornado-raw-ir-recovery.bin` | Recovery | Restored functional raw IR telegram sending with raw frequency `0`; manual Plus works again. | `9514ffda8fab25aafaa343dde9c1bb5f315427004705aab442a7cd6b14230469` |
| 1.0 | `firmware/v1.0-vornado-calibration-99-points.bin` | Stable | Uses raw IR, desired speed 1-99, and calibrates all 99 Vornado speed points instead of stopping on RPM plateaus. | `a11fb26a18a5cd9662bcb120a314c78d270a6ef01661c918d046bbdbf3c0bb0c` |
| 1.1 | `firmware/v1.1-vornado-firmware-version-topic.bin` | Recommended | Publishes the retained firmware version on `Vornado/<device>/firmware/version` and exposes it as a Home Assistant MQTT sensor. | `710d479f9cb6487ede2e66eb9455b7731865e7a087a7c72eef45059674d9670b` |

## Operational Notes

- Flash `v1.1` for the current working firmware.
- After flashing `v1.1`, run calibration once if no valid calibration data is stored. Calibration takes about 10 minutes because all 99 Vornado speed positions are sampled.
- Do not use `v0.14` unless specifically testing the native Symphony encoder regression.
- Speed target values are `1-99`. Use the Power button/entity to switch the fan off.
- The firmware version is published retained on `Vornado/<device>/firmware/version`.
