# Harness Tester Challenge Bug List

Bug-list submission only. I did not change the design or firmware.

## Schematic / Hardware Design

1. GPS UART TX/RX are crossed incorrectly. The schematic nets put Teensy `0_RX1` on `UBX-RXD` and Teensy `1_TX1` on `UBX-TXD`, while the NEO-M8N `RXD` is also on `UBX-RXD` and `TXD` is also on `UBX-TXD`; RX is tied to RX and TX is tied to TX.

2. `CY_SDA` is pulled down, not up. `R3` connects `CY_SDA` to `GND`, which holds the I2C data line low and prevents the bus from working.

3. The RGB status LED has no current-limiting resistors. `D3` cathodes are driven directly by Teensy GPIOs from a common-anode `+3.3V` LED.

4. The CY8C9560 I2C address pins are reused as harness pins. `A1`, `A2`, and `A3` are on `CBL_19`, `CBL_17`, and `CBL_16`, so the expander address can float or change with the harness under test while firmware assumes a fixed address.

5. `UBX-SAFEBOOT` is connected to a GPIO but has no safe external default. Because the firmware drives this low, the GPS can be forced into safeboot instead of normal operation.

6. `UBX-RST_N` has no external pull-up or reset supervisor. The GNSS module reset state depends on firmware pin configuration during boot.

7. The MOSFET gate clamp uses a Schottky diode (`D2`) instead of a zener/TVS gate-source clamp. It does not protect Q1 Vgs from automotive transients.

8. The input TVS has no fuse or series current-limiting element. A sustained surge or reverse/fault event can turn the TVS into an input short with no sacrificial protection.

9. Harness pins are wired directly to CY8C9560 GPIOs with no ESD, over-voltage, or series-current protection. A bad harness, charged cable, or externally driven pin can damage the expander.

10. The `+3.3V` rail is sourced from the Teensy 4.1 regulator for the GPS, CY8C9560, active RF path, I2C pull-up, button pull-up, and LED. That depends on the Teensy's small 3.3 V budget instead of a board-level regulator sized for the load.

## PCB Layout / Fabrication

11. The PCB has no routed copper tracks at all. The KiCad board contains zero `segment` objects, zero `via` objects, and zero `zone` objects, so same-named nets are not physically connected between footprints.

12. The 2x20 harness connector footprint is placed/rotated off the board. With the edge cuts at x=119.5..180.5 mm, 34 of the 40 `J3` pads land outside the board outline.

13. `J3` also physically overlaps `U4`. Their transformed courtyard/footprint boxes overlap around the CY8C9560 package area.

14. `C6` overlaps the `U5` MAX2679 footprint/courtyard, creating an assembly collision in the RF front end.

15. The GNSS antenna has no RF keepout or ground-plane treatment. The patch antenna is placed immediately next to the GNSS module/RF parts on a board with no copper reference plane, which breaks the antenna environment.

16. The RF feed between antenna, LNA, matching parts, and NEO-M8N is not routed as a controlled-impedance RF trace and has no continuous ground reference.

17. The linear regulator has no thermal copper area. A 12 V to 5 V linear drop feeding the Teensy and peripherals can dissipate significant heat, but the layout provides no copper pour or heatsinking.

18. Decoupling capacitors are not actually connected by copper because of the unrouted board, so every IC effectively loses its local supply bypassing in the fabricated layout.

19. The cable connector being mostly outside the edge cuts means the harness cannot mechanically mate to a fabricated board as designed.

20. The PCB ratsnest/airwires in the render are misleadingly left as if routed; the gerbers only contain pads/footprints, not the required interconnect.

## Firmware

21. `cy.begin()` is never called in `setup()`. The CY8C9560 reset sequence, I2C bus initialization, and ID check never run.

22. Because `WIRE.begin()` is only inside `CY8C9560::begin()`, the firmware talks to the expander without initializing the selected I2C peripheral.

23. `CY8C9560::begin()` drives reset high, then low, and leaves `CY_RST` low. For an active-low reset pin, that leaves the expander held in reset.

24. The LED pins are never configured as outputs before `set_status()` writes them. The status LED will not be driven correctly.

25. `PIN_UBX_SAFEBOOT` and `PIN_UBX_RST_N` are never configured as outputs. The writes in `setup()` only affect input/pull state, not deterministic GNSS control.

26. If `PIN_UBX_SAFEBOOT` were configured as an output, the firmware drives it low, which asserts the active-low safeboot input instead of leaving the GNSS module in normal boot.

27. The test button logic is inverted. The hardware pull-up/button-to-ground makes a press read `LOW`, but the loop returns when the button is `LOW` and starts testing when it is not pressed.

28. Test result aggregation is wrong. `passed` starts `false` and is set `true` if any one row matches, so a harness can pass when only one tested pin has the expected connectivity.

29. `1 << i` is a 32-bit shift before assignment to `uint64_t`. Pins 32-39 produce undefined/wrong masks; the upper eight harness pins cannot be tested correctly.

30. The serial print loop repeats the same 32-bit shift bug with `1 << j`, so reported connectivity for pins 32-39 is wrong.

31. `CY8C9560::set_output()` writes `REG_PIN_DIRECTION` as `0x00` for every port, making all pins on every port outputs instead of only the selected driven pin.

32. The firmware switches the expander output and pulldown modes, then reads immediately with no settling delay. Cable capacitance and GPIO mode changes can be sampled before the net stabilizes.

33. `nmea_buf` has no bounds check. A long NMEA sentence can increment `nmea_idx` past the 64-byte buffer before a newline arrives.

34. `process_nmea()` writes `buf[len] = 0`; when the buffer is full, this is a one-byte overflow even before considering longer input.

35. The parser only accepts `$GPRMC`. A u-blox configured for multi-GNSS NMEA can emit `$GNRMC`, so the tester may never obtain time even with a valid fix.

36. The RMC parser ignores the status field. It accepts both valid (`A`) and invalid (`V`) RMC sentences and can mark time fixed without a real GPS lock.

37. NMEA checksums are not validated before accepting time/date data, so corrupted serial input can set the tester clock.

38. The firmware logs and tests repeatedly while the button condition remains true; there is no press edge detection or debounce.

39. On SD initialization failure, the firmware stalls forever without setting the status LED to `FAILED`.

40. The expected harness matrix is hard-coded as an example in firmware. There is no way to load/select definitions for the many car-specific harnesses the tester is supposed to verify.
