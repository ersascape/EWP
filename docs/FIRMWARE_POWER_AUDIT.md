# Firmware architecture and power audit — 2026-09-29

Reviewed `3b5abd0` plus the existing, uncommitted event-driven Apple BLE worker change. This is a source/configuration audit, not a measured battery-life result. No firmware behavior was changed during this audit. The unrelated pasted example sketch is excluded.

The current ESP-IDF + Arduino-component foundation is suitable. Keep it and consolidate ownership of power, radio lifecycle, deadlines and rendering. A kernel fork or another OS migration would not address the concrete problems below.

## Highest-priority findings

### 1. Hotspot cleanup on screen exit — implemented, validation pending

**Confirmed code path; potentially large drain when triggered. A cleanup hook is now implemented in the working tree.**

`PortalApp::onStop()` now calls the idempotent `AppPortal::stop()` function. `ApplicationManager::switchTo()` calls the active app's `onStop()` before changing screens, so navigation, automatic return, or notification/call takeover stops the AP's web server, DNS server, and Wi-Fi. Explicit web stop and sync paths use the same stop function. The AP timeout still runs only while its screen is active, but leaving that screen now stops the AP directly.

This is a first focused fix; longer term the network service should own AP lifetime independently of screens. Build and test navigation, automatic return, notification takeover, and call takeover with the AP running before flashing.

### 2. Sleep policy has multiple owners and incomplete deadlines

**Confirmed architectural mismatch; affects correctness and power.**

`src/ui/watch_ui.cpp:460` implements actual automatic-sleep eligibility. `PowerManager::canSleep()` and its application wake locks are not consulted. Those locks are bookkeeping, disconnected from the ESP-IDF lock owned by the UI. Duplicate acquisitions of the same tag are not reference-counted, so one release can incorrectly clear another holder's constraint if the API is adopted as written.

The UI blocks until the next clock minute (`watch_ui.cpp:470`), ignoring several earlier deadlines:

- BLE advertising restart scheduled 750 ms after disconnect, and retries scheduled later by the BLE HAL.
- The 500 ms watchface media debounce.
- Battery sampling nominally due every 10 seconds.
- Screen timeout and other maintenance deadlines.

A disconnect wakes the UI immediately, but the delayed advertising deadline has not arrived yet. With no other event, the next attempt can be delayed until the minute wake. Likewise a pending song update can wait much longer than 500 ms. Battery qualification can take much longer than its nominal three samples at 10-second intervals.

Fix: services expose their next required deadline; the executor waits for an event or the earliest deadline. Separate user activity, background work, UI visibility, and hardware constraints. Keep the requested 30-second interaction policy, but make one coordinator responsible for it. Use one real, reference-counted power-lock implementation. Remove or restrict the unused manual `enterLightSleep()` API so future callers cannot bypass BLE-aware automatic sleep.

### 3. Actual sleep residency is unmeasured

**Confirmed instrumentation gap.**

Generated build configuration enables PM, tickless idle, and BLE modem sleep. PM profiling and core dumps are disabled. Successful PM configuration and the UI's “armed” log do not measure CPU sleep time.

The installed C3 controller source, `components/bt/controller/esp32c3/bt.c`, confirms that the configured `CONFIG_BT_CTRL_MAIN_XTAL_PU_DURING_LIGHT_SLEEP` path keeps the main crystal powered and avoids the otherwise required Bluetooth no-light-sleep lock. This is compatible with sleeping while connected, but is not the lowest possible clock-power configuration. Do not switch to the internal RC sleep clock blindly: that same controller source warns about its accuracy for connected BLE. An external DS3231 is not automatically a wired MCU/BLE sleep-clock source.

Add a diagnostic build with PM lock profiling, task runtime/stack information, queue high-water/drop counts, radio-state durations, refresh counts, and actual sleep-duration instrumentation supported by the pinned IDF. Emit aggregated reports rather than per-wake logs. Validate battery current with USB disconnected; software idle time alone is not an energy measurement.

PM lock and CPU-mode profiling is now enabled for the current diagnostic firmware. On the first eligible idle wait, firmware prints ESP-IDF's accumulated lock reference counts and residency percentages for CPU-max, APB-max, minimum-frequency and light-sleep modes. Profiling adds runtime overhead; disable `CONFIG_PM_PROFILING` after the power investigation.

### 4. BLE has no adaptive connection policy and retries unsupported peers

**Confirmed missing policy; savings depend on negotiated parameters.**

`src/hal/esp32/esp32_bluetooth.cpp` sets a fixed 500 ms advertising interval. Calls to `setMinPreferred`/`setMaxPreferred` do not establish an active/idle connection-parameter policy, and accepted interval/latency/timeout values are not logged.

Use bounded fast discovery followed by slower advertising, and request validated active/idle connection parameters. Record what the phone actually accepts. Apple's published QA1931 lists specific advertising intervals; the current 500 ms value is not one of them. Use supported values and test background reconnect rather than assuming a generic interval is optimal.

The peripheral now advertises at Apple's exact 20 ms fast interval for 30 seconds, then switches to 546.25 ms, with the transition included in the idle scheduler deadline. Service discovery now treats a peer without ANCS/AMS/CTS as a valid connection instead of retrying Apple discovery forever. Negotiated connection parameters still need hardware capture before tuning.

`Esp32AppleClient::discover()` returns false when ANCS, AMS and CTS are all absent. The worker then repeats discovery with a two-second pause. A valid Android/Linux companion connection without those services can consequently keep searching indefinitely. Treat successful discovery with zero optional capabilities as a valid result; retry only transient failures with a budget, and rediscover on a relevant service change or explicit request.

### 5. Blocking GATT operations bypass application timeout handling

**Confirmed reliability risk; not an identified cause of the reported panic.**

ANCS writes, descriptor writes, reads and discovery use synchronous Arduino BLE helpers. The installed helper implementation waits on semaphores with `portMAX_DELAY`. The ANCS ten-second timeout runs on the same worker only after such calls return, so it cannot bound a stuck GATT helper.

Use asynchronous, serialized GATT transactions with deadlines and session/generation validation. Connection teardown must cancel pending operations. Preserve the existing bounded queues and session-scoped UIDs. A blocked task does not itself burn CPU continuously, but stuck subscriptions, recovery and repeated reconnects undermine both reliability and power.

The existing local worker patch removes the former 50 ms steady-state queue poll; retain that direction, but verify all wake paths and real deadlines. In that patch, `pause()` reads elapsed time twice before subtracting from an unsigned timeout: use a single elapsed snapshot to avoid underflow at the deadline. The oversized-packet early-return path should also wake the worker when setting its overflow flag.

The default CPU floor is now 40 MHz. `Dvfs` adds scoped 80 MHz/APB and 160 MHz CPU locks for BLE event dispatch and app rendering, then releases them when that work ends. A low-rate observer logs actual CPU frequency transitions, including changes caused by ESP-IDF peripheral locks; validation on hardware is still needed to confirm the idle floor and power impact.

Boot startup now keeps hardware ownership in one place: the board initializes its buttons/pin mapping, while RTC, battery, display and BLE are initialized by their service managers. The former board-level RTC/display/BLE initialization duplicated those service calls. Startup reports button, RTC, battery, display and BLE checks; optional RTC/BLE failures are reported as degraded operation. Persisted timezone, refresh interval, hotspot credentials and timeout are range-checked and repaired in RAM from defaults before use.

### 6. CTS event now uses the BLE queue; hardware validation pending

**Confirmed concurrency defect; code path fixed and firmware builds.**

`BluetoothManager::init()` now routes Current Time Service events through the same FreeRTOS incoming queue as other BLE events. Queue insertion happens before the UI wake, and only the UI task applies and publishes the time event. This removes the unsynchronized worker-to-UI write to `EventBus::post()`.

Keep application state and event dispatch on one owner task. The CTS race is not evidence for the panic already diagnosed in Wi-Fi/BLE coexistence.

## Display, battery and other subsystem findings

### 7. Display ownership is duplicated

`WatchUi::renderCurrentApp()` directly drives the concrete ESP32/GxEPD2 display and owns partial-refresh counters. `ApplicationManager` and `DisplayManager` also contain rendering/refresh policy, but the runtime does not use their full rendering paths or call `DisplayManager::tick()`.

Panel power-off after partial refresh is already implemented; GxEPD2 also powers off after a full `display(false)`. Do not count a hypothetical always-powered panel as a proven drain. However, the manager's internal power state can disagree with the HAL, and panel hibernation is not used.

Consolidate drawing, invalidation, coalescing, refresh accounting and panel power into one display owner. Compare final pixels or rendered state before issuing a waveform: `ApplicationManager::handleEvent()` currently returns handled for every event when an app exists, so no-op buttons can trigger refreshes. Notification bursts can also cause successive refreshes despite little visible change.

The BUSY callback polls buttons and delays 1 ms throughout a roughly 1.2-second partial waveform. Replace this coupling with button edge capture/debounce deadlines and a BUSY completion wait, with a bounded timeout. Respect the interaction policy while doing so. Evaluate panel hibernation only after verifying that wake preserves correct differential updates; controller reset behavior can require additional refresh work and erase the expected savings.

### 8. CPU idle floor lowered to 40 MHz; hardware validation pending

`src/main.cpp` now sets minimum frequency to 40 MHz and maximum to 160 MHz (previously 80 MHz minimum). The firmware builds with this setting. Verify BLE stability, display/input responsiveness and current on-device; driver locks may still demand higher clocks. A lower floor helps awake idle periods; it does not directly reduce radio energy or energy during actual CPU sleep. Tickless idle is already enabled, so enabling it again is not a new optimization.

### 9. Battery estimate and low-battery policy need separation

`src/core/battery.cpp` takes 24 ADC samples, including at least 12 ms of busy delays per measurement, and maps filtered voltage to a generic percentage curve. `Esp32Battery::isCharging()` treats voltage at or above 4250 mV as charging; hardware charge status is not available. The BLE Battery Service is initialized to 100 and never updated. Board capabilities describe a battery gauge even though the measurement is an ADC divider.

Represent voltage, estimate confidence, measurement validity, and unknown charging status explicitly. Calibrate divider/ADC readings against a meter and the actual cell. Do not label a percentage jump as real charging, nor promise accurate SOC from smoothing. Use adaptive sampling: longer intervals for a healthy idle battery, shorter intervals near low voltage or during recovery. Put these deadlines into the scheduler.

Critical shutdown depends partly on the generic percentage curve and on filtered voltage. Invalid/out-of-range readings become “disconnected,” which currently resets the power level to Normal. Add an explicit invalid-measurement policy. On a timed critical-battery wake, check voltage before initializing BLE and doing a full display update, rather than repeating a normal boot and waiting for three fresh qualifying samples.

Adaptive sampling is now implemented in `PowerManager`: healthy readings use a 60-second interval, mid-range readings use 20–30 seconds, and low/critical readings retain 10-second checks. The UI's idle wait includes the next battery sample deadline, and the host test verifies the slower interval and three-sample critical qualification. This reduces ADC work while healthy; battery-life improvement still requires current measurement on hardware.

### 10. Network work blocks the UI and bypasses NetworkManager

Apps call synchronous `NetSync` routines directly. During a long sync the UI cannot dispatch BLE events or service buttons normally; bounded queues can overflow. Normal sync completion does turn Wi-Fi off, which is good. The panic log confirms that disabling Wi-Fi modem sleep with BLE active makes ESP-IDF abort. `connectWiFi()` now keeps modem sleep enabled; firmware compilation succeeded, while hardware testing of Wi-Fi sync with BLE connected remains pending.

Use a bounded network worker with cancellation, result events and a guaranteed radio lease release. Measure sync duration and current with modem sleep enabled. Keep an overall operation deadline as well as per-request timeouts. Existing NetworkManager handles are not wired into this runtime path.

### 11. Persistence and diagnostics need tightening

- The `ota_ab.csv` layout preserves the 64 KiB coredump partition. Flash core dumps are enabled; retain the matching ELF and decode the dump before another panic overwrites it. A saved `PANIC` reason alone cannot locate the crash.
- Session uptime is checkpointed to NVS every five minutes. It is approximate and can miss the final interval; it is not measured battery endurance.
- Recent-call enrichment rewrites multiple keys for the whole recent list. Batch/version these writes and avoid committing unchanged values.
- `CONFIG no stored preferences` is emitted whenever opening the namespace fails. Log the actual storage failure separately from “not configured”; this message alone does not prove Wi-Fi credentials were erased.
- RTC and display initialization happen through both board and service initialization. Choose one owner and propagate initialization errors.
- `WatchClock` reads hardware approximately every second while its loop runs; idle blocking already reduces that rate. Use monotonic interpolation and scheduled reconciliation rather than treating this as an unconditional one-second wake source.
- BLE/manual time-source priority never expires. Add source freshness so NTP can become a fallback after an old higher-priority source is no longer available.
- App compatibility lifecycle methods `onEnter/onExit` are distinct from the manager's `onStart/onStop`; reconcile them so initialization/cleanup overrides actually run.
- Networking currently disables TLS certificate verification and logs a session cookie. Remove secret logging and restore certificate validation while refactoring that path.
- The older power-design document still describes the pre-IDF sleep situation; it needs updating to match the built configuration.

## Recommended ownership model

Keep protocol parsing and feature models portable. Put platform-specific sleep, clocks, GPIO, ADC, radio and display operations behind HAL interfaces.

Apple ANCS/AMS/CTS is one source, not the application model. The new `ICompanionSource` contract carries source identity, availability, runtime capabilities, normalized callbacks, and accepted/rejected commands independently of `IBluetooth` transport. `BluetoothManager` injects both contracts separately; the T1E currently composes them in one ESP32 driver object, while host tests use distinct transport and source objects. MPRIS belongs in a Linux source adapter, while Android can supply an independent companion source. A multi-source registry and extracting `Esp32AppleClient` into a standalone provider remain follow-up work.

1. One application executor owns UI/model changes and consumes bounded events. Calls/disconnects have reserved capacity; replaceable media state is coalesced.
2. A power coordinator combines interaction policy, real hardware leases, battery state and the earliest service deadline. It requests automatic-sleep eligibility through the platform HAL.
3. One display owner batches invalidations, skips identical frames and accounts for panel power/refresh cost.
4. Network and BLE workers own their transactions, deadlines and cancellation. Screen visibility does not determine whether radio cleanup runs.
5. ANCS, AMS, CTS and future companion providers report independent capabilities. Absence of an optional capability is a normal state.

## Implementation order and validation

1. Build and hardware-check the AP cleanup change; then fix CTS queueing and missing service deadlines, and add crash and power diagnostics.
2. Complete/review the event-driven BLE worker and bounded discovery/GATT recovery. Keep the current interactive behavior stable.
3. Measure and tune connection parameters; measure the new CPU idle floor separately.
4. Consolidate display invalidation/BUSY handling, then evaluate panel hibernation.
5. Calibrate battery readings and implement adaptive sampling/early low-voltage boot handling.

Measure average current and event charge at the battery connection for disconnected advertising, connected idle with no music, media playback, notification bursts, display refresh, Wi-Fi sync, and AP exit. Record negotiated BLE parameters and sleep/lock statistics for each run. Specifically reproduce AP exit by incoming notification and reconnect while the UI is already idle.

Host unit tests cover useful service/protocol behavior, but cannot establish actual sleep residency, negotiated BLE behavior, panel current, or hardware battery endurance. The current working-tree firmware, including the hotspot lifecycle, notification-screen, Wi-Fi/BLE coexistence, agenda cleanup, 40 MHz CPU floor, and CTS queue fixes, builds successfully; it has not been flashed or validated on hardware. The reported two-hour session ended in a PANIC according to the user, so it is not a confirmed full-charge-to-empty result.

## External references

- [ESP-IDF 4.4.7 C3 power management](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32c3/api-reference/system/power_management.html): PM locks, DFS, tickless idle and timed task wakeups. For the precise BLE clock configuration, the installed C3 controller source was also inspected.
- [Apple QA1931](https://developer.apple.com/library/archive/qa/qa1931/_index.html): published advertising intervals and connection-parameter constraints. Treat phone acceptance and measured reconnect behavior as validation requirements.
- [ESP-IDF 4.4.7 C3 core dumps](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32c3/api-guides/core_dump.html): capturing task/register/stack context for postmortem debugging.
