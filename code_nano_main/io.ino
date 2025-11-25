/*
 * io.ino - Input/Output Functions
 *
 * This file contains:
 * - Hall sensor reading and decoding
 * - Hall sensor diagnostic testing
 * - Temperature sensing (for NTC thermistor)
 * - Buzzer control and audio feedback patterns
 */

// ============================================================================
// HALL SENSOR FUNCTIONS
// ============================================================================

/**
 * Read and decode hall sensor states into commutation step (1-6)
 *
 * Hall sensors provide 6 valid states as the motor rotates. Each state
 * corresponds to a specific rotor position and determines which motor
 * phases should be energized.
 *
 * Returns:
 *   1-6: Valid hall state corresponding to motor position
 *   0:   Invalid state (error condition or transition)
 */
uint8_t get_hall_state() {
  
  for (uint8_t i=0;i<3;i++)
  {
    HALL_states[i] =  digitalRead(HALL_pins[i]);
  }
  
  if ((HALL_states[U]) && (!HALL_states[V]) && (!HALL_states[W])) return 1;
  else if ((HALL_states[U]) && (HALL_states[V]) && (!HALL_states[W])) return 2;
  else if ((!HALL_states[U]) && (HALL_states[V]) && (!HALL_states[W])) return 3;
  else if ((!HALL_states[U]) && (HALL_states[V]) && (HALL_states[W])) return 4;
  else if ((!HALL_states[U]) && (!HALL_states[V]) && (HALL_states[W])) return 5;
  else if ((HALL_states[U]) && (!HALL_states[V]) && (HALL_states[W])) return 6;

  return 0;
}


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
// TEMPERATURE SENSING
// ============================================================================

/**
 * Read NTC thermistor and convert to temperature in Celsius
 *
 * Calibration data for MF52 type NTC thermistor:
 *   ADC -> Temperature
 *   65  -> 100°C
 *   114 -> 80°C
 *   203 -> 60°C
 *   355 -> 40°C
 *   512 -> 25°C
 *   682 -> 10°C
 *
 * Fitted equation: T = 111.72 - 0.269*x + 0.00018*x²
 * R² = 0.986
 *
 * Datasheet: https://www.gotronic.fr/pj2-mf52type-1554.pdf
 */
int get_temprature()
{

  /* 1024 bit readings to celcius:
  65 to 100*
  114 to 80*
  203 to 60*
  355 to 40*
  512 to 25*
  682 to 10*
  // fit eq: y = 111.7198 - 0.2690269*x + 0.0001800678*x^2*
  // R2=0.986

  https://www.gotronic.fr/pj2-mf52type-1554.pdf
  */
  

  
  int x = 0;
  for (unsigned char i = 0; i<5;i++)
  {
    //x += analogRead(pin_temp_sense);
  }
  x /= 5;
  int temp_celcius = 111.7198 - (0.2690269*x) + (0.0001800678*x*x);

  return temp_celcius;
}

// ============================================================================
// AUDIO FEEDBACK (BUZZER)
// ============================================================================

/**
 * Generate audio feedback patterns for different system events
 *
 * Parameters:
 *   kind: Event type
 *     0 = Ready/startup confirmation
 *     1 = Overload/overcurrent warning
 *     2 = General errors
 *     3 = Severe errors (critical faults)
 *
 * Note: Uncomment the first line to disable all beeps
 */
void beep(uint8_t kind)
{
  //return; //silence - uncomment to disable all audio feedback

  if (kind==1) //overload/overcurrent
  {
    buzz_loop(500, 100);
    buzz_loop(400, 100);
    buzz_loop(300, 100);
    buzz_loop(200, 100);
    buzz_loop(600, 500);
  }
  else if (kind==2) //errors
  {
    buzz_loop(300, 300);
    buzz_loop(350, 200);
    buzz_loop(400, 100);
  }
  else if (kind==3) //severe errors
  {
    buzz_loop(350, 500);
    buzz_loop(410, 500);
    buzz_loop(460, 400);
    buzz_loop(510, 400);
  }
  else if (kind==0) //ready
  {
    buzz_loop(200, 100);
    buzz_loop(180, 100);
    buzz_loop(160, 100);
    buzz_loop(140, 200);
    buzz_loop(120, 200);
    buzz_loop(100, 300);
    buzz_loop(80, 400);
  }
}

/**
 * Generate square wave tone on buzzer
 *
 * Creates an audible tone by toggling the buzzer pin
 *
 * Parameters:
 *   delay: Half-period in microseconds (controls pitch)
 *   counts: Number of cycles (controls duration)
 *
 * Example: buzz_loop(500, 300) creates ~1kHz tone for ~0.3 seconds
 */
void buzz_loop(unsigned long delay, uint16_t counts)
{
  for (int i = 0; i < counts; i++)
  {
    delayMicroseconds(delay);
    digitalWrite(pin_BUZZ, HIGH);
    delayMicroseconds(delay);
    digitalWrite(pin_BUZZ, LOW);
  }
}
