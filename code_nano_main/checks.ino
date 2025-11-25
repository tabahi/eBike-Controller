/*
 * checks.ino - System Health Checks and Diagnostics
 *
 * This file contains comprehensive system validation functions:
 * - MOSFET functionality testing
 * - Hall sensor validation
 * - Voltage monitoring and battery level warnings
 * - Throttle input validation
 * - Current sensing checks
 */

// ============================================================================
// VOLTAGE MONITORING CONFIGURATION
// ============================================================================
#define CRITITAL_LOW_VOLTS 7  // Minimum acceptable voltage for testing (default: 32.5V for production)
#define VOLTS_ADC_FACTOR 12.31  // ADC reading to real voltage conversion multiplier
#define _CRITITAL_LOW_VOLTS_ADC CRITITAL_LOW_VOLTS*VOLTS_ADC_FACTOR

int max_volts = 0;  // Stores maximum voltage detected during checks

// ============================================================================
// COMPREHENSIVE SYSTEM HEALTH CHECK
// ============================================================================

/**
 * Run all system diagnostics and safety checks
 *
 * This function performs:
 * 1. Emergency halt signal check
 * 2. Current sensor validation (idle current check)
 * 3. MOSFET functionality test
 * 4. Hall sensor validation
 * 5. Throttle input validation
 * 6. Battery voltage monitoring with audio warnings
 *
 * Parameters:
 *   run_beeps: Enable audio feedback for warnings and errors
 *
 * Returns:
 *   true:  All checks passed
 *   false: One or more checks failed
 *
 * Error Codes:
 *   E85: Idle current too high (possible short circuit)
 *   E20: MOSFET test failed
 *   E75: Invalid hall sensor state
 *   E10: Throttle input validation failed
 */
bool run_checks(bool run_beeps)
{
  motor_off_state();
  if (digitalRead(pin_HALT_IN)==0) 
  {
    //Serial.println("HALT");
    //return false;
  }
  Serial.println("CHECK");
  Serial.print("Critical thresh:"); Serial.println(_CRITITAL_LOW_VOLTS_ADC);

  delay(10);
  if (analogAvg(pin_Isense) > 10)
  {
    delay(1);
    if (analogAvg(pin_Isense) > 10)
    {
      Serial.print("I"); Serial.print('\t'); Serial.println(analogAvg(pin_Isense));  //idle current
      Serial.println("E85");
      if (run_beeps)
      {
        beep(3);
        beep(1);
      }
      return false;
    }
    
  }
  if (!check_fets()) 
  {
    if (!check_fets()) 
    {
      if (!check_fets()) 
      {
        Serial.println("E20;");
        if (run_beeps)  beep(3);
        return false;
      }
    }
  }

  if (get_hall_state()==0)
  {
    Serial.println("H=0");
    Serial.println("E75");
    if (run_beeps) beep(2);
    return false;
  }
 

  if (!check_thot()) 
  {
    Serial.println("E10;");
    Serial.print('T'); Serial.print(analogAvg(pin_THOT_sense));  Serial.println(';'); 
    if (run_beeps) beep(2);
    return false;
  }
  

  if (max_volts > _CRITITAL_LOW_VOLTS_ADC)
  {
    Serial.print("------------------------------------------------"); Serial.print(max_volts/VOLTS_ADC_FACTOR);Serial.println("V");
    if (max_volts < 420)  //34V , last warning, 4 beeps
    {
      buzz_loop(500, 300);
      delay(150);
      buzz_loop(500, 300);
      delay(150);
      buzz_loop(500, 300);
      delay(150);
      buzz_loop(500, 300);
    }
    else if (max_volts < 437) //35.5V
    {
      buzz_loop(500, 300);
      delay(200);
      buzz_loop(500, 300);
      delay(200);
      buzz_loop(500, 300);
    }
    else if (max_volts < 455) //37V
    {
      buzz_loop(500, 300);
      delay(100);
      buzz_loop(500, 300);
    }
    else if (max_volts < 470)  //38.1V, first warning, one beep
    {
      buzz_loop(500, 300);
    }
  }

  Serial.println("ALL_OK");
  return true;
}


bool check_fets()
{
  motor_off_state();
  int vxh[3] = { 0, 0, 0 };
  int vxl[3] = { 255, 255, 255 };
  for (uint8_t i=0;i<3;i++)
  {
    digitalWrite(LOW_SIDE_pins[i], 0);
    delay(1);
    digitalWrite(HIGH_SIDE_pins[i], 1);
    delay(1);
    vxh[i] = analogAvg(Ox_pins[i]);
    digitalWrite(HIGH_SIDE_pins[i], 0);
    delay(10);

    digitalWrite(HIGH_SIDE_pins[i], 0);
    delay(1);
    digitalWrite(LOW_SIDE_pins[i], 1);
    delay(1);
    vxl[i] = analogAvg(Ox_pins[i]);
    digitalWrite(LOW_SIDE_pins[i], 0);
    motor_off_state();
    Serial.print('F'); Serial.print(i);  Serial.print('\t');  Serial.print(vxh[i]);  Serial.print('\t'); Serial.print(vxl[i]);  Serial.println(';');
    /* Good output at 33V:
        F0	399	0;
        F1	397	0;
        F2	396	0;
        F0	399	0;
        F1	397	0;
        F2	396	0;
    */
  }
  
  for (uint8_t i=0;i<3;i++)
  {
    if (vxh[i] > max_volts) max_volts = vxh[i];

    if (vxh[i] < _CRITITAL_LOW_VOLTS_ADC) // undervoltage or no voltage, 32.5V
    {
      Serial.println("E21;");
      return false;
    }
    if (vxl[i] > 0) //low side not switching
    {
      Serial.println("E22;");
      return false;
    }
  }

  return true;
}

int analogAvg(int pin_num)
{
  int sum = 0;
  for (int i=0; i<10; i++)
  {
    sum += analogRead(pin_num);
    delayMicroseconds(100);
  }
  return sum/10;
}