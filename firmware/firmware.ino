#include <SD.h>
#include <ctype.h>
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
const uint64_t EXPECTED_CONNECTIONS[NUM_HARNESS_PINS] = {
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

bool valid_nmea_checksum(const char *sentence) {
    if (sentence[0] != '$') return false;
    // find the checksum starting point
    const char *asterisk = strchr(sentence, '*');
    if (asterisk == nullptr || strlen(asterisk) < 3) return false;
    // verify checksum is valid hex
    if (!isxdigit(asterisk[1]) || !isxdigit(asterisk[2])) return false;
    uint8_t checksum = 0;
    for (const char *p = sentence + 1; p < asterisk; p++) {
        checksum ^= static_cast<uint8_t>(*p);
    }
    // get the checksum from the sentence
    char received_str[3] = {asterisk[1], asterisk[2], '\0'};
    uint8_t received = static_cast<uint8_t>(strtoul(received_str, nullptr, 16));
    return checksum == received;
}

void process_nmea(char *buf, int len) {
  buf[len] = 0;
  if (strncmp(buf, "$GPRMC", 6) != 0) return;
  if (!valid_nmea_checksum(buf)) {
    DBG_SERIAL.println("Bad NMEA checksum");
    return;
  }

  char status;
  char new_utc_time[20];
  char new_date[20];
  int fields = sscanf(buf, "$GPRMC,%10[^,],%c,%*f,%*c,%*f,%*c,%*f,%*f,%6[^,]", new_utc_time, &status, new_date);

  if (fields != 3) {
    DBG_SERIAL.println("Failed to parse $GPRMC");
    return;
  }
  if (status != 'A') {
    time_fixed = false;
    return;
  }
  strcpy(utc_time, new_utc_time);
  strcpy(date, new_date);
  if (!time_fixed) {
    DBG_SERIAL.println("Time received. Ready to test some harnesses!");
    set_status(GOOD);
    time_fixed = true;
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

  // Initialize RGB LED GPIO
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);

  // GPIO
  set_status(BUSY);
  pinMode(PIN_BTN_TEST, INPUT);
  pinMode(PIN_UBX_TIMEPULSE, INPUT);
  pinMode(PIN_UBX_SAFEBOOT, OUTPUT);
  pinMode(PIN_UBX_RST_N, OUTPUT);
  digitalWrite(PIN_UBX_SAFEBOOT, LOW);
  digitalWrite(PIN_UBX_RST_N, HIGH);

  // CY8C9560
  if(!cy.begin()) {
    DBG_SERIAL.println("CY8C9560 initialization failed");
    while (1);
  }

  // SD card
  if (!SD.begin(SD_CS)) {
    DBG_SERIAL.println("SD card initialization failed");
    while (1);
  }

  DBG_SERIAL.println("Waiting for GPS time lock...");
}
bool last_passed = true;
char nmea_buf[128];
int nmea_idx = 0;
void loop() {
  static bool discarding_nmea = false;
  // Process incoming ublox messages
  while (UBX_SERIAL.available()) {
    char next_byte = UBX_SERIAL.read();

    // discard sequence until current sentence finishes
    if (discarding_nmea) {
        if (next_byte == '\n') {
            discarding_nmea = false;
            nmea_idx = 0;
        }
        continue;
    }

    if (next_byte == '\r') continue;

    if (next_byte == '\n') {
      process_nmea(nmea_buf, nmea_idx);
      nmea_idx = 0;

      // Jump out the while here to make sure we're not missing any button presses under high load
      break;
    }

    if (nmea_idx < sizeof(nmea_buf) - 1) {
      nmea_buf[nmea_idx++] = next_byte;
    } else {
      // discard/reset oversized sentence
      nmea_idx = 0;
      discarding_nmea = true;
    }
  }

  // Return if we don't have a time fix yet
  if (!time_fixed) return;
  set_status(last_passed ? GOOD : FAILED);

  // Button edge trigger
  static bool previous_button = HIGH;
  bool current_button = digitalRead(PIN_BTN_TEST);
  bool button_pressed = previous_button == HIGH && current_button == LOW;
  previous_button = current_button;

  // Start testing only if the button is pressed
  if (!button_pressed) return;  // active low
  set_status(BUSY);

  // Test harness by checking the connectivity of every pin
  bool passed = true;
  for (int i = 0; i < NUM_HARNESS_PINS; i++) {
    uint64_t output_mask = 1ULL << i; // Should be shifted by an unsigned long long
    if (!cy.set_output(output_mask, output_mask)) {
      DBG_SERIAL.println("Failed to configure output");
      passed = false;
      break;
    }
    if (!cy.set_pd_inputs(~output_mask)) {
      DBG_SERIAL.println("Failed to configure inputs");
      passed = false;
      break;
    }
    uint64_t values;
    if (!cy.read_inputs(values)) {
      DBG_SERIAL.println("Failed to read inputs");
      passed = false;
      break;
    }
    values &= ~output_mask;

    // Log connections to the serial port
    DBG_SERIAL.printf("Pin %d: ", i);
    for (int j = 0; j < NUM_HARNESS_PINS; j++) DBG_SERIAL.printf("%d", (values & (1ULL << j)) ? 1 : 0); // Should be shifted by an unsigned long long
    DBG_SERIAL.println();

    if (values != EXPECTED_CONNECTIONS[i]) {
      passed = false;
    }
  }

  // Show and log results
  DBG_SERIAL.println(passed ? "Harness passed!" : "Harness failed!");
  last_passed = passed;
  log_result(passed);
  set_status(passed ? GOOD : FAILED);
}
