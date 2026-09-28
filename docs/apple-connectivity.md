# Apple connectivity and provider boundaries

The watch automatically advertises ANCS solicitation, lets the BLE stack request encryption after connection,
and schedules ANCS/AMS discovery. The worker waits for a latched authentication
completion for that connection before any GATT discovery; it does not compare
identity and private peer addresses. Missing services are retried. Initial pairing and iOS notification
permission are still required. Status → B1 restarts advertising while disconnected.

## Responsibilities

```text
Call / notification / now-playing screens
                  |
      BluetoothManager (application state)
                  |
     IBluetooth (current provider boundary)
                  |
 ESP32 Apple adapter       future companion adapter
         |                      |
 portable ANCS/AMS codecs    Android / Linux integration
         |
 ESP32 BLE GATT transport
```

`include/ersa/protocols/apple_notifications.h` and `apple_media.h` contain portable
C++ decoding and ANCS action validation. They have no Arduino, ESP32, or FreeRTOS
dependencies. ESP32 owns pairing, GATT objects, subscriptions, and its worker queues.
The manager moves callbacks to the main loop before changing application state or UI.

`IBluetooth` is still a combined transport/provider interface; this change does not
claim a complete transport abstraction. When another provider is implemented, extract
its call/media/notification contract from that interface rather than exposing D-Bus
or Android types to screens. Capabilities must decide which controls are available.
MPRIS is a D-Bus media interface on Linux, so a Linux companion must bridge it to
the watch's transport. It is not another BLE service to discover. Android needs a
separate companion integration. Neither is implemented by this change.

## Notification and media behavior

- One ANCS attribute request is in flight at a time. Title and Message are always
  requested; the negative action label is requested when iOS advertises that action.
  Responses are reassembled across GATT fragments and validated against their UID.
- Call events and general Notification Source records have separate compact queues.
  The bounded history favors recent records during large startup bursts. Dropped
  history never disconnects BLE or invalidates a different attribute response.
- Modified UIDs replace existing history entries; removed UIDs leave history.
- An attribute-stream failure retries ANCS subscriptions on the existing BLE link,
  with a two-second delay and at most three recoveries per connection. If recovery
  is exhausted, metadata requests pause until the next connection; BLE and AMS stay up.
- Resubscribing creates a new ANCS epoch. Old fragments cannot be mixed into the new
  session. Call actions use their captured UID and the latest positive/negative flags.
- A watch notification dismissal is sent to iOS only when the notification advertises
  a negative action and its label is “Dismiss” or “Clear.” ANCS supplies no result
  notification for action commands, and other labels are not treated as dismissal.
- ANCS Message is not a guaranteed telephone number. Notification removal only ends
  the ringing notification; it does not establish the phone call's final outcome.
  Native ANCS cannot provide a complete active-call timer, dialing, or hangup API.
- AMS subscribes to supported commands, track title/artist, and playback state.
  Truncated track values are read through Entity Attribute, then bounded for display.
  Sending a playback command does not fabricate a playback-state update.
- GATT Service Changed triggers rediscovery on the existing link. Remote characteristic
  pointers are invalidated before service refresh. One worker owns the BLE client.

## Regression and device verification

`make test` covers fragmented headers and attribute values, empty and maximum-sized
responses, UID mismatch, malformed data, dismissal-label parsing, stale/unavailable
call actions, AMS decoding, notification modification/removal/reset, and disconnect cleanup. The ESP32 firmware
build verifies the hardware adapter compiles; it is not a radio integration test.

For a device check, confirm `burst-safe worker started` in the log, then:

1. Pair with many existing iPhone notifications. `ANCS=1 AMS=1` must not be followed
   by a watch-initiated disconnect. History-burst warnings may occur without link loss.
2. Deliver two messages and an incoming call together; answer/decline while messages
   continue arriving. Caller text and actions must stay attached to the call UID.
   For a notification that exposes “Dismiss” or “Clear,” dismiss it from the watch
   and confirm it disappears on iOS. Notifications with other action labels should
   disappear only from watch history.
3. Start/pause/change music, including long titles; launch the player after pairing.
4. Leave the phone locked and watch idle, then move out of range and return. Verify
   advertising resumes and Apple services are discovered after authentication.
5. If disconnected again, retain the `Auth complete`, `Central disconnected reason`,
   and Apple worker messages. The firmware logs the actual disconnection reason.

Protocol references: [ANCS specification](https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleNotificationCenterServiceSpecification/Specification/Specification.html),
[AMS specification](https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleMediaService_Reference/Specification/Specification.html),
and [MPRIS specification](https://specifications.freedesktop.org/mpris/latest/).
