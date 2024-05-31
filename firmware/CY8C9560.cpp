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

  return read_id() == 0x06;
}

uint8_t CY8C9560::read_register(uint8_t reg) {
  WIRE.beginTransmission(addr);
  WIRE.write(reg);
  WIRE.endTransmission(false);
  WIRE.requestFrom(addr, 1U);
  return WIRE.read();
}

uint64_t CY8C9560::read_registers(uint8_t reg_base, uint8_t len) {
  WIRE.beginTransmission(addr);
  WIRE.write(reg_base);
  WIRE.endTransmission(false);
  WIRE.requestFrom(addr, len);
  uint64_t registers = 0;
  for (int i = 0; i < len; i++) {
    registers |= (uint64_t)WIRE.read() << (i * 8);
  }
  return registers;
}

void CY8C9560::write_register(uint8_t reg, uint8_t data) {
  WIRE.beginTransmission(addr);
  WIRE.write(reg);
  WIRE.write(data);
  WIRE.endTransmission();
}

void CY8C9560::write_registers(uint8_t reg_base, uint64_t data, uint8_t len) {
  WIRE.beginTransmission(addr);
  WIRE.write(reg_base);
  for (int i = 0; i < len; i++) {
    WIRE.write((uint8_t) (data >> (i * 8)));
  }
  WIRE.endTransmission();
}

uint8_t CY8C9560::read_id() {
  return (read_register(REG_DEVICE_ID_STATUS) >> 4) & 0x0F;
}

uint64_t CY8C9560::read_inputs() {
  return read_registers(REG_INPUT_BASE, 8);
}

void CY8C9560::set_pd_inputs(uint64_t pins) {
  for (int i = 0; i < 8; i++) {
    write_register(REG_PORT_SELECT, i);
    write_register(REG_PIN_DIRECTION, 0xFF);
    write_register(REG_PIN_DRIVE_MODE_BASE + DRIVE_MODE_PULL_DOWN, pins >> (i * 8));
  }
}

void CY8C9560::set_pu_inputs(uint64_t pins) {
  for (int i = 0; i < 8; i++) {
    write_register(REG_PORT_SELECT, i);
    write_register(REG_PIN_DIRECTION, 0xFF);
    write_register(REG_PIN_DRIVE_MODE_BASE + DRIVE_MODE_PULL_UP, pins >> (i * 8));
  }
}

void CY8C9560::set_output(uint64_t pins, uint64_t values) {
  write_registers(REG_OUTPUT_BASE, values, 8);
  for (int i = 0; i < 8; i++) {
    write_register(REG_PORT_SELECT, i);
    write_register(REG_PIN_DIRECTION, 0x00);
    write_register(REG_PIN_DRIVE_MODE_BASE + DRIVE_MODE_STRONG, pins >> (i * 8));
  }
}

