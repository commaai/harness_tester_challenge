# Harness tester bug list

This is a findings-only submission for the challenge. It intentionally does not
attempt to fix the design.

## Schematic and hardware

1. The GNSS UART is wired TX-to-TX and RX-to-RX.
   `U3` `TXD/SPI_MISO` is on `UBX-TXD`, which goes to Teensy pin
   `1_TX1`. `U3` `RXD/SPI_MOSI` is on `UBX-RXD`, which goes to Teensy pin
   `0_RX1`. The module TX pin needs to reach the MCU RX pin, and vice versa, so
   the firmware will not receive NMEA data.

2. `CY_SDA` is pulled down instead of up.
   `R2` pulls `CY_SCL` to `+3.3V`, but `R3` connects `CY_SDA` to `GND`. This
   holds the I2C data line low and prevents normal I2C communication with the
   `CY8C9560`.

3. The RGB LED has no current-limiting resistors.
   `D3` has its common anode tied to `+3.3V`, and `LED_R`, `LED_G`, and
   `LED_B` go directly to Teensy GPIO pins. Turning on an LED channel creates an
   uncontrolled current path through the MCU pin.

4. The harness header pin names do not match the bit positions used by the
   expander registers after `CBL_19`.
   The schematic maps `CBL_20..CBL_27` to port 3 bits 0..7, which are register
   bits 24..31, not bits 20..27. `CBL_28..CBL_35` are bits 32..39, and
   `CBL_36..CBL_39` are bits 40..43. Firmware treats `CBL_n` as bit `n`, so
   the tester reads and drives the wrong pins for half the connector and never
   scans the last four harness pins.

5. The GNSS RF chain is not a usable 50 ohm RF layout.
   The antenna, LNA, matching parts, and module input are connected with tiny
   discrete parts and ordinary board routing rather than a controlled-impedance
   feed with a clear ground reference and keepout strategy. This makes GPS lock
   unreliable even if the UART is fixed.

6. The ceramic GNSS antenna placement is mechanically and electrically poor.
   The antenna is crowded directly under the Teensy/module area in the board
   render, with nearby copper and components in the keepout area. A patch
   antenna needs a deliberate ground-plane and keepout implementation, not
   overlap with the controller module.

## Firmware

1. `cy.begin()` is never called in `setup()`.
   The I2C bus is never started, the expander reset pin is never configured, and
   the device ID is never checked before `loop()` starts using the expander.

2. `CY8C9560::begin()` leaves the expander held in reset.
   `RESET_N` is active low, but `begin()` drives it high, then low, and never
   returns it high. Even if `begin()` were called, the expander would remain in
   reset.

3. The GNSS control pins are written without being configured as outputs.
   `PIN_UBX_SAFEBOOT` and `PIN_UBX_RST_N` get `digitalWrite()` calls in
   `setup()`, but no `pinMode(..., OUTPUT)`. On Arduino-style GPIO this sets
   input state or pullups rather than actively controlling the GNSS module.

4. The test button logic is inverted.
   The schematic pulls `BTN_TEST` up through `R4` and the switch shorts it to
   ground when pressed. Firmware returns when the pin is low and starts a test
   when it is high, so it tests while the button is not pressed.

5. The pass/fail accumulator cannot represent "all rows must pass".
   `passed` starts as `false` and becomes `true` when any one pin's measured
   connections match. It is never cleared on a later mismatch, so a harness can
   pass with only one correct row.

6. The harness bit operations use 32-bit shifts.
   `1 << i` and `1 << j` are used while `i` and `j` can exceed 31. Those shifts
   overflow or become undefined before being widened to `uint64_t`, breaking all
   upper harness pins.

7. The expander output pin is immediately changed back to input.
   The loop calls `cy.set_output(output_mask, output_mask)` and then
   `cy.set_pd_inputs(~output_mask)`. `set_pd_inputs()` writes
   `REG_PIN_DIRECTION` to `0xFF` for every port, including the port containing
   the intended output, so no pin is left actively driving the harness.

8. `set_output()` and the input helper methods change entire ports, not just
   the requested pins.
   `set_output()` writes `REG_PIN_DIRECTION` as `0x00` for each port, and the
   input helpers write `0xFF` for each port. The `pins` mask is only used for
   drive-mode registers, so unrelated expander pins are repeatedly reconfigured.

9. The firmware ignores the physical expander-to-header mapping.
   Even aside from the 32-bit shifts, `read_inputs()` returns raw CY8C9560 port
   bits. The code compares those raw bits directly against `EXPECTED_CONNECTIONS`
   indexed by harness pin number, so `CBL_20..CBL_39` are compared against the
   wrong matrix columns.

10. `nmea_buf` can overflow on normal NMEA traffic.
    The buffer is 64 bytes, there is no bounds check while reading, and
    `process_nmea()` writes `buf[len] = 0`. RMC sentences are commonly longer
    than this, so the GPS parser can corrupt memory before reaching a newline.

11. The parser only accepts `$GPRMC`.
    Multi-constellation u-blox modules commonly emit `$GNRMC`; those sentences
    are ignored, so time lock can fail even when the module has valid time.

12. The parser accepts invalid RMC status.
    The RMC status field is discarded with `%*c`, so a void fix can still mark
    `time_fixed = true` and allow test results to be logged with invalid time.

13. `CY8C9560::read_register()` and `read_registers()` do not check I2C return
    values.
    Failed transactions still call `WIRE.read()` and use whatever value is
    returned, making a disconnected or wedged expander look like real pin data.

## PCB layout

1. The RF layout around `AE1`, `U5`, `L1`, `C5`, and `U3` is not controlled
   impedance and has no clear RF keepout or via-fenced ground reference.
   This is a layout-level showstopper for a GNSS receiver.

2. The GNSS antenna is placed too close to the Teensy module and other metal.
   The 3D render shows the antenna crowded by the controller module rather than
   placed at a board edge with the required keepout.

3. The 40 harness traces are long, dense, and unprotected.
   Every external harness pin runs directly into the CY8C9560 with no ESD,
   current limiting, or series isolation. A bad harness or operator contact can
   damage the expander and make the tester unreliable.
