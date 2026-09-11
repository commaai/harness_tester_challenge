#include "CY8C9560.h"

bool CY8C9560::begin() {
  pinMode(CY_IRQ_N, INPUT_PULLUP);
  pinMode(CY_RST, OUTPUT);
  digitalWrite(CY_RST, HIGH);
  delay(10);
  digitalWrite(CY_RST, LOW);
  delay(100);

  WIRE.begin();
  WIRE.setClock(100000);

  uint8_t id;
  if(!read_id(id)) return false;
  return id == 0x06;
}

bool CY8C9560::read_register(uint8_t reg, uint8_t &value) {
  WIRE.beginTransmission(addr);
  WIRE.write(reg);
  if (WIRE.endTransmission(false) != 0) return false;
  if (WIRE.requestFrom(addr, 1U) != 1) return false;
  int data = WIRE.read();
  if (data < 0) return false;
  value = static_cast<uint8_t>(data);
  return true;
}

bool CY8C9560::read_registers(uint8_t reg_base, uint8_t len, uint64_t &registers) {
  if (len > 8) return false; // limited to 8 bytes
  WIRE.beginTransmission(addr);
  WIRE.write(reg_base);
  if (WIRE.endTransmission(false) != 0) return false;
  if (WIRE.requestFrom(addr, len) != len) return false;
  registers = 0;
  for (int i = 0; i < len; i++) {
    int data = WIRE.read();
    if (data < 0) return false;
    registers |= static_cast<uint64_t>(static_cast<uint8_t>(data)) << (i * 8);
  }
  return true;
}

bool CY8C9560::write_register(uint8_t reg, uint8_t data) {
  WIRE.beginTransmission(addr);
  if (WIRE.write(reg) != 1) return false;
  if (WIRE.write(data) != 1) return false;
  if (WIRE.endTransmission() != 0) return false;
  return true;
}

bool CY8C9560::write_registers(uint8_t reg_base, uint64_t data, uint8_t len) {
  if (len > 8) return false; // limited to 8 bytes
  WIRE.beginTransmission(addr);
  if (WIRE.write(reg_base) != 1) return false;
  for (int i = 0; i < len; i++) {
    uint8_t byte = static_cast<uint8_t>(data >> (i * 8));
    if (WIRE.write(byte) != 1) return false;
  }
  if (WIRE.endTransmission() != 0) return false;
  return true;
}

bool CY8C9560::read_id(uint8_t &id) {
  uint8_t value;
  if(!read_register(REG_DEVICE_ID_STATUS, value)) return false;
  id = (value >> 4) & 0x0F;
  return true;
}

bool CY8C9560::read_inputs(uint64_t &inputs) {
  return read_registers(REG_INPUT_BASE, 8, inputs);
}

bool CY8C9560::set_pd_inputs(uint64_t pins) {
  for (int i = 0; i < 8; i++) {
    uint8_t mask = static_cast<uint8_t>(pins >> (i * 8));
    if (!write_register(REG_PORT_SELECT, i)) return false;
    // get the existing pin direction of the port
    uint8_t direction;
    if (!read_register(REG_PIN_DIRECTION, direction)) return false;
    direction |= mask; // set the pins as input if not already
    if (!write_register(REG_PIN_DIRECTION, direction)) return false;
    if (!write_register(REG_PIN_DRIVE_MODE_BASE + DRIVE_MODE_PULL_DOWN, mask)) return false;
  }
  return true;
}

bool CY8C9560::set_pu_inputs(uint64_t pins) {
  for (int i = 0; i < 8; i++) {
    uint8_t mask = static_cast<uint8_t>(pins >> (i * 8));
    if (!write_register(REG_PORT_SELECT, i)) return false;
    // get the existing pin direction of the port
    uint8_t direction;
    if (!read_register(REG_PIN_DIRECTION, direction)) return false;
    direction |= mask; // set the pins as input if not already
    if (!write_register(REG_PIN_DIRECTION, direction)) return false;
    if (!write_register(REG_PIN_DRIVE_MODE_BASE + DRIVE_MODE_PULL_UP, mask)) return false;
  }
  return true;
}

bool CY8C9560::set_output(uint64_t pins, uint64_t values) {
  if (!write_registers(REG_OUTPUT_BASE, values, 8)) return false;
  for (int i = 0; i < 8; i++) {
    uint8_t mask = static_cast<uint8_t>(pins >> (i * 8));
    if (!write_register(REG_PORT_SELECT, i)) return false;
    // 0 = output, so clear only the requested direction bits.
    uint8_t direction;
    if (!read_register(REG_PIN_DIRECTION, direction)) return false;
    direction &= static_cast<uint8_t>(~mask);
    if (!write_register(REG_PIN_DIRECTION, direction)) return false;
    if (!write_register(REG_PIN_DRIVE_MODE_BASE + DRIVE_MODE_STRONG, mask)) return false;
  }
  return true;
}

