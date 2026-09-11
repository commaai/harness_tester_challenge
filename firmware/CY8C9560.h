#ifndef _CY8C9560_h_
#define _CY8C9560_h_

#include <Arduino.h>
#include <Wire.h>

#define WIRE Wire2
#define CY8C9560_ADDR 0b0100000
#define CY_RST 22
#define CY_IRQ_N 23

#define REG_INPUT_BASE 0x00
#define REG_OUTPUT_BASE 0x08
#define REG_PORT_SELECT 0x18
#define REG_PIN_DIRECTION 0x1C
#define REG_PIN_DRIVE_MODE_BASE 0x1D
#define REG_DEVICE_ID_STATUS 0x2E

#define DRIVE_MODE_PULL_UP 0x00
#define DRIVE_MODE_PULL_DOWN 0x01
#define DRIVE_MODE_OPEN_DRAIN_HIGH 0x02
#define DRIVE_MODE_OPEN_DRAIN_LOW 0x03
#define DRIVE_MODE_STRONG 0x04
#define DRIVE_MODE_SLOW_STRONG 0x05
#define DRIVE_MODE_HIGH_IMPEDANCE 0x06

class CY8C9560 {
private:
  bool read_register(uint8_t reg, uint8_t &value);
  bool read_registers(uint8_t reg_base, uint8_t len, uint64_t &registers); // limited to 8 bytes

  bool write_register(uint8_t reg, uint8_t data);
  bool write_registers(uint8_t reg_base, uint64_t data, uint8_t len); // limited to 8 bytes
  
  uint8_t addr;
public:

  constexpr CY8C9560(uint8_t addr = CY8C9560_ADDR) : addr(addr) { }
  bool begin();

  bool read_id(uint8_t &id);
  bool read_inputs(uint64_t &inputs);

  bool set_pd_inputs(uint64_t pins);
  bool set_pu_inputs(uint64_t pins);
  bool set_output(uint64_t pins, uint64_t values);
};

#endif
