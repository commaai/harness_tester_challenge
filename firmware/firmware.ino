#include <SD.h>
#include "CY8C9560.h"

enum Status {FAILED, BUSY, GOOD};

#define PIN_UBX_TIMEPULSE 2
#define PIN_UBX_SAFEBOOT 3
#define PIN_UBX_RST_N 4
#define PIN_LED_R 5
#define PIN_LED_G 6
#define PIN_LED_B 7
#define PIN_BTN_TEST 8

#define DBG_SERIAL Serial
#define UBX_SERIAL Serial1
#define SD_CS BUILTIN_SDCARD

#define NUM_HARNESS_PINS 40
// Example connection matrix, just for illustration
uint64_t EXPECTED_CONNECTIONS[NUM_HARNESS_PINS] = {
  0b1000000000000000000000000010000000000000,
  0b0100001000000000000000000000000000000000,
  0b0010000000000000000000000000000000000000,
  0b0001100000001000000000000000000010000000,
  0b0000100000100000000000000001000000000000,
  0b0000010100000000000000000000000000000000,
  0b0000001000000000100000000000000000000000,
  0b0000000100000000000000000000001000001000,
  0b0000000010000000000000000000000000000000,
  0b0000000001000000000000000000000010000000,
  0b0010000000100000000000000000000000000000,
  0b0000000000010000000010000000000000000000,
  0b0000000000001000000000000000000000010000,
  0b0000000000000100000000000000000000000001,
  0b0000000000000010000000001000000000000000,
  0b0000000000001001000000000100000000000000,
  0b0000000000000000100000000000000000000000,
  0b0000010000000000010000000000000000000000,
  0b0000000000000000001000000000000000000000,
  0b0000000000000000000100000000100000000000,
  0b0000000000000000000010000000000000000000,
  0b0000000000000000000001000000000000000000,
  0b0000000000000000000000100000000000000000,
  0b0000000000000000000000010000000001000000,
  0b0000000000000000000000001000000000000000,
  0b0000000000001000000000000100000000000000,
  0b0000000000000000000000000010000000000000,
  0b0000000000100010000000001001000000001000,
  0b0000000000000000000000000000100000000000,
  0b0000000000001000000000000000010000010000,
  0b0000000000000000000000000000001000000000,
  0b0000000000000000000000000000000100000000,
  0b0000000000000000000000000000000010100000,
  0b0000000000000000000000000000000001000010,
  0b0000100000000000000000100000000000100000,
  0b0000010000000000000000000000000000011000,
  0b0000000000000000000000000000000000001000,
  0b0000000000000000000000000000010000000100,
  0b0000000000000000000000000000000000000010,
  0b0000000000000000000000000000000000000001,
};

CY8C9560 cy;
char utc_time[20], date[20];
bool time_fixed = false;

void set_status(Status s) {
  digitalWrite(PIN_LED_R, !(s == FAILED));
  digitalWrite(PIN_LED_B, !(s == BUSY));
  digitalWrite(PIN_LED_G, !(s == GOOD));
}

void process_nmea(char *buf, int len) {
  buf[len] = 0;
  if (strncmp(buf, "$GPRMC", 6) == 0) {
    if (sscanf(buf, "$GPRMC,%10[^,],%*c,%*f,%*c,%*f,%*c,%*f,%*f,%6[^,]", utc_time, date) == 2 && !time_fixed) {
      DBG_SERIAL.println("Time received. Ready to test some harnesses!");
      set_status(GOOD);
      time_fixed = true;
    } else if (!time_fixed) {
      DBG_SERIAL.println("Failed to parse $GPRMC");
    }
  }
}

void log_result(bool passed) {
  File f = SD.open("results.txt", FILE_WRITE);
  if (f) {
    f.print(date); f.print(" - "); f.print(utc_time); f.print(": "); // raw date + time strings are fine here
    f.println(passed ? "Passed" : "Failed");
    f.close();
  } else {
    DBG_SERIAL.println("Failed to open log file");
  }
}

void setup() {
  // Serial ports
  DBG_SERIAL.begin(115200);
  UBX_SERIAL.begin(9600);

  // GPIO
  set_status(BUSY);
  pinMode(PIN_BTN_TEST, INPUT);
  pinMode(PIN_UBX_TIMEPULSE, INPUT);
  digitalWrite(PIN_UBX_SAFEBOOT, LOW);
  digitalWrite(PIN_UBX_RST_N, HIGH);

  // SD card
  if (!SD.begin(SD_CS)) {
    DBG_SERIAL.println("SD card initialization failed");
    while (1);
  }

  DBG_SERIAL.println("Waiting for GPS time lock...");
}

char nmea_buf[64];
int nmea_idx = 0;
void loop() {
  // Process incoming ublox messages
  while (UBX_SERIAL.available()) {
    nmea_buf[nmea_idx++] = UBX_SERIAL.read();
    if (nmea_buf[nmea_idx - 1] == '\n' || nmea_buf[nmea_idx - 1] == '\r') {
      process_nmea(nmea_buf, nmea_idx);
      nmea_idx = 0;

      // Jump out the while here to make sure we're not missing any button presses under high load
      break;
    }
  }

  // Return if we don't have a time fix yet
  if (!time_fixed) return;
  set_status(GOOD);

  // Start testing only if the button is pressed
  if (digitalRead(PIN_BTN_TEST) == LOW) return;
  set_status(BUSY);

  // Test harness by checking the connectivity of every pin
  bool passed = false;
  for (int i = 0; i < NUM_HARNESS_PINS; i++) {
    uint64_t output_mask = 1 << i;
    cy.set_output(output_mask, output_mask);
    cy.set_pd_inputs(~output_mask);

    uint64_t values = cy.read_inputs() & ~output_mask;

    // Log connections to the serial port
    DBG_SERIAL.printf("Pin %d: ", i);
    for (int j = 0; j < NUM_HARNESS_PINS; j++) DBG_SERIAL.printf("%d", (values & (1 << j)) ? 1 : 0);
    DBG_SERIAL.println();

    if (values == EXPECTED_CONNECTIONS[i]) {
      passed = true;
    }
  }

  // Show and log results
  DBG_SERIAL.println(passed ? "Harness passed!" : "Harness failed!");
  log_result(passed);
  set_status(passed ? GOOD : FAILED);
}
