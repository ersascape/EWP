# Ersa Wearable Platform ewp-0.1.4

This release makes OTA checks and installs suspend Bluetooth temporarily to
recover memory needed for Wi-Fi and HTTPS. The updater screen remains active
and reports progress while the phone link is disconnected.

## Highlights

- The OTA service stops advertising, disconnects an active phone connection,
  and tears down the BLE host and Apple companion worker before its memory
  preflight and HTTPS requests.
- BLE advertising resumes if a check or install exits without rebooting. A
  successful install reboots into the new slot and starts Bluetooth normally.
- OTA HTTPS uses ESP-IDF's certificate bundle to reduce transient heap use.
- Added GAP disconnect with a GATT-close fallback and logs that expose the
  disconnect result when a connected phone prevents OTA from starting.

## Upgrade notes

Install this release from the on-watch **System Update** screen while the
watch is connected to Wi-Fi. The phone will disconnect during the update
operation and can reconnect after a non-rebooting check. Keep the watch charged
and near the access point until the update completes.
