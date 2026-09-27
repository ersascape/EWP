# PCB pin trace

Source: the user's EasyEDA PCB JSON (`editorVersion: 6.5.51`, XIAO reference U8),
provided on 2026-09-27. GPIO translations were checked against the installed
Arduino XIAO_ESP32C3 `pins_arduino.h`. Top/bottom refer to the PCB drawing's Y axis.

| Signal | PCB net / connection | XIAO label | ESP32-C3 GPIO |
| --- | --- | --- | --- |
| Upper button, S2 (Y=3249.5) | S2.2 -> U8_3 -> U8 pad 3 | D2/A2 | 4 |
| Lower button, S1 (Y=3290) | S1.1 -> U8_2 -> U8 pad 2 | D1/A1 | 3 |
| RTC SDA | U7_SDA -> U8 pad 5 | SDA/D4 | 6 |
| RTC SCL | U7_SCL -> U8 pad 6 | SCL/D5 | 7 |
| EPD SCK | U5_4 -> U8 pad 9 | SCK/D8 | 8 |
| EPD MOSI | U5_3 -> U8 pad 11 | MOSI/D10 | 10 |
| EPD CS | U5_5 -> U8 pad 4 | D3/A3 | 5 |
| EPD DC | U5_6 -> U8 pad 8 | RX/D7 | 20 |
| EPD RST | U5_7 -> U8 pad 7 | TX/D6 | 21 |
| EPD BUSY | U5_8 -> U8 pad 10 | MISO/D9 | 9 |

Both buttons close to GND. R1 (100 kOhm) pulls U8_2 to VCC, and R2 (100 kOhm)
pulls U8_3 to VCC. Firmware also enables the requested internal pull-ups.
XIAO pad numbers are footprint terminal numbers, not GPIO numbers.

## Upper button / buzzer conflict

**Assembly clarification:** the user confirmed that this is an older design
and U3 is not fitted. The loading concern below does not apply to that assembly.

U3 (`GSC4417YA-16R4000`) pad 1 is on U8_3, and pad 2 is GND. The routed trace
from U3 pad 1 at (4024.61, 3344.5) joins S2's net at (4033.126, 3323.151).
Thus the buzzer and upper button share the same GPIO-to-ground path; this is
more than a duplicate net label in the footprint. If fitted, the buzzer may
load the pull-up and keep GPIO4 LOW even with S2 released.

Check the USB raw-input logs first. If GPIO4 stays LOW, power off and isolate
U3 before retesting the button. Do not drive GPIO4 HIGH to test it: pressing
the button would short that output to ground. A board revision should separate
the button input from a suitable buzzer driver/output.

This trace establishes the design's connections, not solder continuity on the
assembled board. It does not diagnose the reported RTC/display freeze; USB
logs now distinguish RTC seconds, loop progress, and display BUSY waits.
