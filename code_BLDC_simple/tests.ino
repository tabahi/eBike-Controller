/*
 * tests.ino - Diagnostic and Testing Functions
 *
 * This file contains functions for testing hall sensors and MOSFET operation
 */

// ============================================================================
// HALL SENSOR DIAGNOSTIC TEST
// ============================================================================

/**
 * Print current hall sensor readings to serial monitor
 *
 * Output format: H[state] U:[0/1] V:[0/1] W:[0/1]
 * Useful for debugging hall sensor wiring and verifying proper operation
 */
void hall_test() {
  Serial.print("H");
  Serial.print(get_hall_state());
  Serial.print('\t');
  Serial.print("U: ");
  Serial.print(HALL_states[U]);
  Serial.print('\t');
  Serial.print("V: ");
  Serial.print(HALL_states[V]);
  Serial.print('\t');
  Serial.print("W: ");
  Serial.print(HALL_states[W]);
  Serial.println();
}


// ============================================================================
// MOSFET TESTING CONFIGURATION
// ============================================================================

// Voltage threshold for validation (adjust for your voltage divider)
#define CRITITAL_LOW_VOLTS 7      // Minimum acceptable voltage (default: 32.5V for production)
#define VOLTS_ADC_FACTOR 12.31    // ADC to voltage conversion factor
#define _CRITITAL_LOW_VOLTS_ADC CRITITAL_LOW_VOLTS*VOLTS_ADC_FACTOR

int max_volts = 0;  // Stores the maximum voltage detected during tests

// ============================================================================
// MOSFET FUNCTIONALITY TEST
// ============================================================================

/**
 * Test all MOSFETs by switching them individually and measuring voltage
 *
 * This test verifies:
 * - High-side MOSFETs can pull phase voltage to supply rail
 * - Low-side MOSFETs can pull phase voltage to ground
 * - No short circuits or blown FETs
 * - Adequate supply voltage is present
 *
 * Returns:
 *   true:  All FETs working correctly
 *   false: One or more FETs failed (error codes E21 or E22)
 *
 * Expected output at 33V supply:
 *   F0  399  0;
 *   F1  397  0;
 *   F2  396  0;
 */
bool check_fets() {
  motor_off_state();
  int vxh[3] = { 0, 0, 0 };    // Voltage readings with high-side ON
  int vxl[3] = { 255, 255, 255 }; // Voltage readings with low-side ON

  // Test each phase individually
  for (uint8_t i = 0; i < 3; i++) {
    // Test high-side MOSFET
    digitalWrite(LOW_SIDE_pins[i], LOW);
    delay(1);
    digitalWrite(HIGH_SIDE_pins[i], HIGH);
    delay(1);
    vxh[i] = analogAvg(Ox_pins[i]);  // Should read ~supply voltage
    digitalWrite(HIGH_SIDE_pins[i], LOW);
    delay(10);

    // Test low-side MOSFET
    digitalWrite(HIGH_SIDE_pins[i], LOW);
    delay(1);
    digitalWrite(LOW_SIDE_pins[i], HIGH);
    delay(1);
    vxl[i] = analogAvg(Ox_pins[i]);  // Should read ~0V
    digitalWrite(LOW_SIDE_pins[i], LOW);
    motor_off_state();

    // Print test results
    Serial.print('F');
    Serial.print(i);
    Serial.print('\t');
    Serial.print(vxh[i]);
    Serial.print('\t');
    Serial.print(vxl[i]);
    Serial.println(';');
  }

  // Validate test results
  for (uint8_t i = 0; i < 3; i++) {
    if (vxh[i] > max_volts) max_volts = vxh[i];

    // Error: High-side voltage too low (blown FET, disconnected supply, or undervoltage)
    if (vxh[i] < _CRITITAL_LOW_VOLTS_ADC) {
      Serial.println("E21;");  // Error code 21: High-side failure
      return false;
    }

    // Error: Low-side not pulling to ground (blown FET or short circuit)
    if (vxl[i] > 0) {
      Serial.println("E22;");  // Error code 22: Low-side failure
      return false;
    }
  }

  return true;  // All tests passed
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Read analog pin multiple times and return average
 *
 * Reduces noise by averaging 10 samples with 100us delay between reads
 */
int analogAvg(int pin_num) {
  int sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(pin_num);
    delayMicroseconds(100);
  }
  return sum / 10;
}